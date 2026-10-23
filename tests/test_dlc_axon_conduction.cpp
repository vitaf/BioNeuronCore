// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_dlc_axon_conduction.cpp — диагностика задачи #8: проводимость аксона.
// Гипотеза: длинный немиелинизированный аксон (25 сегм) ослабляет спайк сомы.
// Эксперимент: dIN-конфигурация (cpg::) с переменным числом аксонных сегментов (1 vs 25).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstdio>
#include <cstring>
#include "types.h"
#include "bion_loader.h"
#include "cpg_config.h"
#include "kernel_dispatch.h"
#include "modifier_helpers.h"

extern "C" {
    void core_run_tick(Segment*, uint32_t, IntraConnection*, uint32_t,
                       Synapse*, uint32_t, int, uint64_t*, float,
                       CurrentStimulus*, uint32_t*, VoltageClamp*, uint32_t*,
                       RampStimulus*, uint32_t*, IonInjection*, uint32_t*,
                       LigandInjection*, uint32_t*, MechanoStimulus* mechano, uint32_t* mechano_count);
}

// Строим сома + AIS + N_axon аксонных сегментов. use_dlc=true → NAV12DEV (dlc), false → dIN (Nav1.2+DIN_NAK).
static uint32_t build_chain(Segment* segs, IntraConnection* conns, uint32_t N_axon, bool use_dlc) {
    uint32_t N = 1 + 1 + N_axon;  // сома + AIS + N_axon
    for (uint32_t i = 0; i < N; i++) { segs[i] = Segment{}; }

    // сома (0) — одинакова у dIN и dlc (DIN_NAK 1.0, NAP 3.0, HCN1 120, NMDA 0.25, KV42 0.4, K2P 4.17, NALCN 0.08, PUMP 0.6, KCC2 0.8, Cl 0.1, CA_BUF 0.5, PMCA 0.3)
    segs[0].id = 0; segs[0].neuron_id = 0; segs[0].segment_type = 1;
    segs[0].radius = cpg::R_DIN; segs[0].length = cpg::R_DIN * 2; segs[0].V_m = cpg::V_DIN;
    cpg::init_ions(segs[0]);
    cpg::apply_mods(segs[0], cpg::dIN_soma_mods());
    cpg::apply_state_params(segs[0]);

    // AIS (1)
    segs[1].id = 1; segs[1].neuron_id = 0; segs[1].segment_type = 2;
    segs[1].radius = use_dlc ? 0.2f : cpg::AIS_R;  // Hull 2015: AIS тот же радиус, что аксон
    segs[1].length = cpg::AIS_L; segs[1].V_m = cpg::V_DIN;
    cpg::init_ions(segs[1]);
    if (use_dlc) {
        // dlc_MHR AIS: Nav1.2 500 + K2P 4.17 (без PUMP) — blueprint neuron_library.h
        cpg::apply_mods(segs[1], {{MOD_AXON_NAV12, 500.0f}, {MOD_K2P_LEAK, 4.17f}});
    } else {
        cpg::apply_mods(segs[1], cpg::dIN_ais_mods());
    }
    cpg::apply_state_params(segs[1]);

    // аксон (2 .. 2+N_axon-1)
    for (uint32_t i = 0; i < N_axon; i++) {
        uint32_t k = 2 + i;
        segs[k].id = k; segs[k].neuron_id = 0; segs[k].segment_type = 2;
        segs[k].radius = use_dlc ? 0.2f : cpg::AXON_R;   // d=0.4 мкм (Hull 2015)
        segs[k].length = use_dlc ? 40.0f : cpg::AXON_L;
        segs[k].V_m = cpg::V_DIN;
        cpg::init_ions(segs[k]);
        if (use_dlc)
            cpg::apply_mods(segs[k], {{MOD_AXON_NAV12, 2.0f}, {MOD_K2P_LEAK, 4.17f}, {MOD_CAV21, 100.0f}, {MOD_CA_BUFFER, 0.5f}, {MOD_PMCA, 0.3f}});  // Hull 2015 аксон
        else
            cpg::apply_mods(segs[k], cpg::dIN_axon_mods());
        cpg::apply_state_params(segs[k]);
    }
    // связи
    for (uint32_t i = 0; i < N - 1; i++) {
        conns[i].segment_a = i; conns[i].segment_b = i + 1; conns[i].is_intracellular = 1;
    }
    return N;
}

static void run_axon_len(uint32_t N_axon, const char* tag, bool use_dlc) {
    uint32_t N = 1 + 1 + N_axon;
    Segment* segs = new Segment[N]();
    IntraConnection* conns = new IntraConnection[N - 1]();
    uint32_t n = build_chain(segs, conns, N_axon, use_dlc);
    // убрать bath-NMDA (пейсмейкер) — измеряем изолированный покой/спайк без пейсмейкера
    for (uint8_t j = 0; j < segs[0].modifier_count; j++) {
        if (segs[0].modifiers[j].type == MOD_NT_RECEPTOR_NMDA) {
            for (uint8_t k = j; k + 1 < segs[0].modifier_count; k++) segs[0].modifiers[k] = segs[0].modifiers[k + 1];
            segs[0].modifier_count--; break;
        }
    }
    uint64_t tc = 0;

    for (int t = 0; t < 2000; t++)   // settle 200 мс
        core_run_tick(segs, n, conns, n - 1, nullptr, 0, 0, &tc, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    float rest_soma = segs[0].V_m;

    CurrentStimulus stim = {0, 400.0f, 50.0f, tc};
    uint32_t stim_cnt = 1;
    float* peak = new float[n];
    for (uint32_t i = 0; i < n; i++) peak[i] = -200.0f;
    for (int t = 0; t < 500; t++) {
        core_run_tick(segs, n, conns, n - 1, nullptr, 0, 0, &tc, 0.1f,
                      &stim, &stim_cnt, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        for (uint32_t i = 0; i < n; i++) if (segs[i].V_m > peak[i]) peak[i] = segs[i].V_m;
    }

    printf("[AXLEN:%s] rest_soma=%.1f | peaks: soma=%.1f AIS=%.1f", tag, rest_soma, peak[0], peak[1]);
    for (uint32_t i = 0; i < N_axon && i < 8; i++) printf(" ax%d=%.1f", i, peak[2 + i]);
    if (N_axon > 8) printf(" ... ax%d(терм)=%.1f", N_axon - 1, peak[2 + N_axon - 1]);
    printf("\n");

    delete[] peak;
    delete[] segs;
    delete[] conns;
}

TEST_CASE("dlc axon conduction: axon length sweep", "[dlc-axon]") {
    printf("=== dIN config (Nav1.2 AIS 500 + DIN_NAK axon) ===\n");
    run_axon_len(1, "din-ax1", false);
    run_axon_len(25, "din-ax25", false);
    printf("=== dlc config (NAV12DEV AIS 2000 + NAV12DEV axon 400) ===\n");
    run_axon_len(1, "dlc-ax1", true);
    run_axon_len(25, "dlc-ax25", true);
}

// Трассировка runaway AIS на длинном аксоне: что гонит AIS к +150?
TEST_CASE("dIN AIS runaway trace (25-seg axon)", "[dlc-axon][runaway]") {
    const uint32_t N = 1 + 1 + 25;
    Segment segs[N] = {};
    IntraConnection conns[N - 1] = {};
    uint32_t n = build_chain(segs, conns, 25, false);  // dIN, 25 аксон
    for (uint8_t j = 0; j < segs[0].modifier_count; j++) {
        if (segs[0].modifiers[j].type == MOD_NT_RECEPTOR_NMDA) {
            for (uint8_t k = j; k + 1 < segs[0].modifier_count; k++) segs[0].modifiers[k] = segs[0].modifiers[k + 1];
            segs[0].modifier_count--; break;
        }
    }
    uint64_t tc = 0;
    for (int t = 0; t < 2000; t++)
        core_run_tick(segs, n, conns, n - 1, nullptr, 0, 0, &tc, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    printf("[RUN] settle rest: soma=%.2f AIS=%.2f Na_i_AIS=%.3f\n",
           segs[0].V_m, segs[1].V_m, segs[1].intra_ions[0]);

    CurrentStimulus stim = {0, 400.0f, 50.0f, tc};
    uint32_t stim_cnt = 1;
    for (int t = 0; t < 400; t++) {
        core_run_tick(segs, n, conns, n - 1, nullptr, 0, 0, &tc, 0.1f,
                      &stim, &stim_cnt, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        if (t % 20 == 0 || segs[1].V_m > 100.0f) {
            // токи модификаторов AIS (сегмент 1)
            float I_nav = 0, I_k2p = 0;
            for (uint8_t j = 0; j < segs[1].modifier_count; j++) {
                float I = dispatch_modifier_current(0, segs[1].modifiers[j].type,
                                                    &segs[1].modifiers[j], &segs[1], 0.1f);
                if (segs[1].modifiers[j].type == MOD_AXON_NAV12) I_nav = I;
                if (segs[1].modifiers[j].type == MOD_K2P_LEAK) I_k2p = I;
            }
            printf("[RUN] t=%d AIS V=%.1f Na_i=%.3f Na_o=%.3f | I_nav=%.1f I_k2p=%.1f I_buf=%.1f | soma=%.1f ax0=%.1f\n",
                   t, segs[1].V_m, segs[1].intra_ions[0], segs[1].extra_ions[0],
                   I_nav, I_k2p, segs[1].I_buffer, segs[0].V_m, segs[2].V_m);
        }
        if (segs[1].V_m > 140.0f) break;
    }
}

// Свип Na-плотности аксона: какая плотность DIN_NAK проводит ПД по 25 сегм?
TEST_CASE("axon Na density sweep: conduction", "[dlc-axon][density]") {
    const uint32_t N_axon = 25;
    const uint32_t N = 1 + 1 + N_axon;
    for (float dens : {1.0f, 2.0f, 5.0f, 10.0f, 20.0f, 40.0f}) {
        Segment segs[N] = {};
        IntraConnection conns[N - 1] = {};
        uint32_t n = build_chain(segs, conns, N_axon, false);  // dIN
        for (uint8_t j = 0; j < segs[0].modifier_count; j++) {
            if (segs[0].modifiers[j].type == MOD_NT_RECEPTOR_NMDA) {
                for (uint8_t k = j; k + 1 < segs[0].modifier_count; k++) segs[0].modifiers[k] = segs[0].modifiers[k + 1];
                segs[0].modifier_count--; break;
            }
        }
        // задать DIN_NAK density на аксонных сегментах (2..N-1)
        for (uint32_t i = 2; i < n; i++)
            for (uint8_t j = 0; j < segs[i].modifier_count; j++)
                if (segs[i].modifiers[j].type == MOD_AXON_NAV12) segs[i].modifiers[j].density = dens;
        uint64_t tc = 0;
        for (int t = 0; t < 2000; t++)
            core_run_tick(segs, n, conns, n - 1, nullptr, 0, 0, &tc, 0.1f,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        CurrentStimulus stim = {0, 400.0f, 50.0f, tc};
        uint32_t stim_cnt = 1;
        float peak_terminal = -200, peak_soma = -200;
        for (int t = 0; t < 500; t++) {
            core_run_tick(segs, n, conns, n - 1, nullptr, 0, 0, &tc, 0.1f,
                          &stim, &stim_cnt, nullptr, nullptr, nullptr, nullptr,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            if (segs[0].V_m > peak_soma) peak_soma = segs[0].V_m;
            if (segs[n - 1].V_m > peak_terminal) peak_terminal = segs[n - 1].V_m;
        }
        printf("[DENS] DIN_NAK axon=%.0f -> soma_peak=%.1f terminal_peak=%.1f\n",
               dens, peak_soma, peak_terminal);
    }
}

// Абляция покоя dlc: пошагово добавляем различия dIN→dlc и меряем покой.
static float ablate_rest(bool with_dend, bool dlc_ais, float axon_r, float axon_l, float init_vm = -51.0f, bool keep_nmda = false) {
    const uint32_t N_axon = 25;
    const uint32_t n_dend = with_dend ? 2 : 0;
    const uint32_t N = 1 + 1 + N_axon + n_dend;
    Segment* segs = new Segment[N]();
    IntraConnection* conns = new IntraConnection[N - 1]();
    for (uint32_t i = 0; i < N; i++) segs[i] = Segment{};

    // сома (0)
    segs[0].id = 0; segs[0].neuron_id = 0; segs[0].segment_type = 1;
    segs[0].radius = 5.0f; segs[0].length = 10.0f; segs[0].V_m = init_vm;
    cpg::init_ions(segs[0]); cpg::apply_mods(segs[0], cpg::dIN_soma_mods()); cpg::apply_state_params(segs[0]);
    if (!keep_nmda)
        for (uint8_t j = 0; j < segs[0].modifier_count; j++)
            if (segs[0].modifiers[j].type == MOD_NT_RECEPTOR_NMDA) {
                for (uint8_t k = j; k+1 < segs[0].modifier_count; k++) segs[0].modifiers[k] = segs[0].modifiers[k+1];
                segs[0].modifier_count--; break;
            }
    // AIS (1)
    segs[1].id = 1; segs[1].neuron_id = 0; segs[1].segment_type = 2;
    segs[1].radius = 0.5f; segs[1].length = 20.0f; segs[1].V_m = init_vm;
    cpg::init_ions(segs[1]);
    if (dlc_ais) cpg::apply_mods(segs[1], {{MOD_AXON_NAV12, 500.0f}, {MOD_K2P_LEAK, 4.17f}});
    else cpg::apply_mods(segs[1], cpg::dIN_ais_mods());
    cpg::apply_state_params(segs[1]);
    // аксон (2 .. 2+N_axon-1)
    for (uint32_t i = 0; i < N_axon; i++) {
        uint32_t k = 2 + i;
        segs[k].id = k; segs[k].neuron_id = 0; segs[k].segment_type = 2;
        segs[k].radius = axon_r; segs[k].length = axon_l; segs[k].V_m = init_vm;
        cpg::init_ions(segs[k]); cpg::apply_mods(segs[k], cpg::dIN_axon_mods()); cpg::apply_state_params(segs[k]);
    }
    // дендрит (2+N_axon ..) — ветвится от сомы, HCN1 120 + KV42 0.4
    if (with_dend) {
        for (uint32_t i = 0; i < n_dend; i++) {
            uint32_t k = 2 + N_axon + i;
            segs[k].id = k; segs[k].neuron_id = 0; segs[k].segment_type = 2;
            segs[k].radius = 1.0f; segs[k].length = 30.0f; segs[k].V_m = init_vm;
            cpg::init_ions(segs[k]);
            cpg::apply_mods(segs[k], {{MOD_HCN1, 120.0f}, {MOD_KV42, 0.4f}, {MOD_CA_BUFFER, 0.5f}, {MOD_PMCA, 0.3f}});
            cpg::apply_state_params(segs[k]);
        }
    }
    // связи: сома→AIS→аксон (линейно) + сома→дендрит (ветвь)
    uint32_t ci = 0;
    conns[ci++] = {0, 1, 1};                      // сома→AIS
    conns[ci++] = {1, 2, 1};                      // AIS→axon0
    for (uint32_t i = 0; i + 1 < N_axon; i++)
        conns[ci++] = {2 + i, 2 + i + 1, 1};      // axon[i]→axon[i+1]
    if (with_dend) {
        conns[ci++] = {0, 2 + N_axon, 1};         // сома→дендрит[0]
        conns[ci++] = {2 + N_axon, 2 + N_axon + 1, 1};  // дендрит[0]→дендрит[1]
    }

    uint64_t tc = 0;
    for (int t = 0; t < 2000; t++)
        core_run_tick(segs, N, conns, N - 1, nullptr, 0, 0, &tc, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    float rest = segs[0].V_m;
    delete[] segs; delete[] conns;
    return rest;
}

TEST_CASE("dlc rest ablation: why -66 not -51", "[dlc-axon][rest-ablation]") {
    printf("[ABLATE] dIN baseline            (no dend, dIN AIS, axon 0.5/50): rest=%.1f\n",
           ablate_rest(false, false, 0.5f, 50.0f));
    printf("[ABLATE] + dendrite              (HCN1+KV42):                    rest=%.1f\n",
           ablate_rest(true,  false, 0.5f, 50.0f));
    printf("[ABLATE] + dlc AIS               (K2P 4.17, no PUMP):            rest=%.1f\n",
           ablate_rest(true,  true,  0.5f, 50.0f));
    printf("[ABLATE] + axon geometry          (0.6/40):                       rest=%.1f\n",
           ablate_rest(true,  true,  0.6f, 40.0f));
}

// Бистабильность dlc: из разных начальных V_m → разные равновесия?
TEST_CASE("dlc bistability: rest vs init V_m", "[dlc-axon][bistability]") {
    printf("[BI] full dlc, init -51: rest=%.1f\n", ablate_rest(true, true, 0.6f, 40.0f, -51.0f));
    printf("[BI] full dlc, init -60: rest=%.1f\n", ablate_rest(true, true, 0.6f, 40.0f, -60.0f));
    printf("[BI] full dlc, init -66: rest=%.1f\n", ablate_rest(true, true, 0.6f, 40.0f, -66.0f));
    printf("[BI] full dlc, init -70: rest=%.1f\n", ablate_rest(true, true, 0.6f, 40.0f, -70.0f));
    printf("[BI] full dlc, init -80: rest=%.1f\n", ablate_rest(true, true, 0.6f, 40.0f, -80.0f));
}

// Бистабильность dlc С NMDA (bath-NMDA активен) — даёт ли NMDA второе равновесие −66?
TEST_CASE("dlc bistability WITH NMDA", "[dlc-axon][bistability-nmda]") {
    printf("[BI-NMDA] full dlc + NMDA, init -51: rest=%.1f\n", ablate_rest(true, true, 0.6f, 40.0f, -51.0f, true));
    printf("[BI-NMDA] full dlc + NMDA, init -66: rest=%.1f\n", ablate_rest(true, true, 0.6f, 40.0f, -66.0f, true));
    printf("[BI-NMDA] full dlc + NMDA, init -80: rest=%.1f\n", ablate_rest(true, true, 0.6f, 40.0f, -80.0f, true));
}

// Проверка гейтинга HCN1: селится ли m при dt=0.1 vs dt=0.025
TEST_CASE("HCN1 gating: m settles at dt=0.1 vs 0.025", "[dlc-axon][hcn-gating]") {
    for (float dt : {0.1f, 0.025f}) {
        uint32_t n_ticks = (uint32_t)(200.0f / dt);  // 200 мс симуляции в обоих
        Segment segs[3] = {};
        IntraConnection conns[2] = {};
        uint32_t n = build_chain(segs, conns, 1, false);
        for (uint8_t j = 0; j < segs[0].modifier_count; j++)
            if (segs[0].modifiers[j].type == MOD_NT_RECEPTOR_NMDA) {
                for (uint8_t k = j; k+1 < segs[0].modifier_count; k++) segs[0].modifiers[k] = segs[0].modifiers[k+1];
                segs[0].modifier_count--; break;
            }
        uint64_t tc = 0;
        for (uint32_t t = 0; t < n_ticks; t++)
            core_run_tick(segs, n, conns, n-1, nullptr, 0, 0, &tc, dt,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        float hcn_m = -1, din_m = -1;
        for (uint8_t j = 0; j < segs[0].modifier_count; j++) {
            if (segs[0].modifiers[j].type == MOD_HCN1) hcn_m = segs[0].modifiers[j].state[0];
            if (segs[0].modifiers[j].type == MOD_AXON_NAV12) din_m = segs[0].modifiers[j].state[0];
        }
        printf("[HCN] dt=%.3f: V_m=%.2f HCN1_m=%.4f (mi(-51)=0.047) DIN_NAK_m=%.4f\n",
               dt, segs[0].V_m, hcn_m, din_m);
    }
}

// Диагностика покоя dIN: по-канальный ток при устоявшемся покое.
TEST_CASE("dIN rest diagnosis: per-channel currents", "[dlc-axon][rest]") {
    const uint32_t N = 3;
    Segment segs[N] = {};
    IntraConnection conns[N - 1] = {};
    uint32_t n = build_chain(segs, conns, 1, false);  // dIN, 1 аксон
    // убрать NMDA (как test_din_single_cell) — измеряем изолированный покой без пейсмейкера
    for (uint8_t j = 0; j < segs[0].modifier_count; j++) {
        if (segs[0].modifiers[j].type == MOD_NT_RECEPTOR_NMDA) {
            for (uint8_t k = j; k + 1 < segs[0].modifier_count; k++)
                segs[0].modifiers[k] = segs[0].modifiers[k + 1];
            segs[0].modifier_count--;
            break;
        }
    }
    uint64_t tc = 0;
    for (int t = 0; t < 2000; t++)
        core_run_tick(segs, n, conns, n - 1, nullptr, 0, 0, &tc, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    printf("[REST] soma V_m=%.2f  ions: Na=%.3f K=%.3f Cl=%.3f Mg=%.3f A=%.1f\n",
           segs[0].V_m, segs[0].intra_ions[0], segs[0].intra_ions[1],
           segs[0].intra_ions[3], segs[0].intra_ions[4], segs[0].intra_ions[5]);
    for (uint8_t j = 0; j < segs[0].modifier_count; j++) {
        float I = dispatch_modifier_current(0, segs[0].modifiers[j].type,
                                            &segs[0].modifiers[j], &segs[0], 0.1f);
        printf("[REST]   type=%d dens=%.2f I=%.4f pA\n",
               segs[0].modifiers[j].type, segs[0].modifiers[j].density, I);
    }
    // Свип NALCN: найти плотность, дающую покой ~−51
    printf("[REST] NALCN sweep (rest vs density):\n");
    for (float nalcn : {0.08f, 0.06f, 0.04f, 0.03f, 0.02f, 0.01f}) {
        Segment s2[N]; IntraConnection c2[N - 1];
        uint32_t n2 = build_chain(s2, c2, 1, false);
        for (uint8_t j = 0; j < s2[0].modifier_count; j++) {
            if (s2[0].modifiers[j].type == MOD_NT_RECEPTOR_NMDA) {
                for (uint8_t k = j; k + 1 < s2[0].modifier_count; k++) s2[0].modifiers[k] = s2[0].modifiers[k + 1];
                s2[0].modifier_count--; break;
            }
        }
        for (uint8_t j = 0; j < s2[0].modifier_count; j++)
            if (s2[0].modifiers[j].type == MOD_NALCN) s2[0].modifiers[j].density = nalcn;
        uint64_t tc2 = 0;
        for (int t = 0; t < 2000; t++)
            core_run_tick(s2, n2, c2, n2 - 1, nullptr, 0, 0, &tc2, 0.1f,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        printf("[REST]   NALCN=%.2f -> V_m=%.2f mV\n", nalcn, s2[0].V_m);
    }
    // Свип NaP: персистентный Na (Sautois не имеет) — деполяризует ли покой?
    printf("[REST] NaP sweep (rest vs density):\n");
    for (float nap : {3.0f, 2.0f, 1.0f, 0.5f, 0.0f}) {
        Segment s2[N]; IntraConnection c2[N - 1];
        uint32_t n2 = build_chain(s2, c2, 1, false);
        for (uint8_t j = 0; j < s2[0].modifier_count; j++) {
            if (s2[0].modifiers[j].type == MOD_NT_RECEPTOR_NMDA) {
                for (uint8_t k = j; k + 1 < s2[0].modifier_count; k++) s2[0].modifiers[k] = s2[0].modifiers[k + 1];
                s2[0].modifier_count--; break;
            }
        }
        for (uint8_t j = 0; j < s2[0].modifier_count; j++)
            if (s2[0].modifiers[j].type == MOD_NAP_CHANNEL) s2[0].modifiers[j].density = nap;
        uint64_t tc2 = 0;
        for (int t = 0; t < 2000; t++)
            core_run_tick(s2, n2, c2, n2 - 1, nullptr, 0, 0, &tc2, 0.1f,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        printf("[REST]   NaP=%.2f -> V_m=%.2f mV\n", nap, s2[0].V_m);
    }
    // Свип K2P: найти плотность, дающую покой ~−51
    printf("[REST] K2P sweep (rest vs density, NALCN=0.08):\n");
    for (float k2p : {4.17f, 6.0f, 8.0f, 10.0f, 12.0f, 16.0f, 20.0f}) {
        Segment s2[N]; IntraConnection c2[N - 1];
        uint32_t n2 = build_chain(s2, c2, 1, false);
        for (uint8_t j = 0; j < s2[0].modifier_count; j++) {
            if (s2[0].modifiers[j].type == MOD_NT_RECEPTOR_NMDA) {
                for (uint8_t k = j; k + 1 < s2[0].modifier_count; k++) s2[0].modifiers[k] = s2[0].modifiers[k + 1];
                s2[0].modifier_count--; break;
            }
        }
        for (uint8_t j = 0; j < s2[0].modifier_count; j++)
            if (s2[0].modifiers[j].type == MOD_K2P_LEAK) s2[0].modifiers[j].density = k2p;
        uint64_t tc2 = 0;
        for (int t = 0; t < 2000; t++)
            core_run_tick(s2, n2, c2, n2 - 1, nullptr, 0, 0, &tc2, 0.1f,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        printf("[REST]   K2P=%.2f -> V_m=%.2f mV\n", k2p, s2[0].V_m);
    }
}

// Фаза 3: выброс глутамата — терминаль >0 мВ → Cav2.1 → bouton_Ca → синаптотагмин → cleft_nt.
// Ранее терминаль −31.5 мВ → Cav2.1 (HVA ~0 мВ) закрыт → max_glu=0 (память #360).
TEST_CASE("dlc glutamate release: Cav2.1 -> bouton_Ca -> cleft", "[dlc-axon][release]") {
    const uint32_t N_axon = 25;
    const uint32_t N = 1 + 1 + N_axon;  // сома + AIS + 25 аксон
    Segment* segs = new Segment[N]();
    IntraConnection* conns = new IntraConnection[N - 1]();
    uint32_t n = build_chain(segs, conns, N_axon, true);  // dlc (MOD_AXON_NAV12 ×2 + Cav2.1 100 на аксоне)
    // убрать bath-NMDA (как другие тесты) — измеряем изолированный спайк
    for (uint8_t j = 0; j < segs[0].modifier_count; j++) {
        if (segs[0].modifiers[j].type == MOD_NT_RECEPTOR_NMDA) {
            for (uint8_t k = j; k + 1 < segs[0].modifier_count; k++) segs[0].modifiers[k] = segs[0].modifiers[k + 1];
            segs[0].modifier_count--; break;
        }
    }
    // Синапс: пре = терминаль (n-1), пост = сома (0, dummy target)
    Synapse syn = {};
    syn.id = 0;
    syn.pre_segment = n - 1;   // аксонная терминаль
    syn.post_segment = 0;      // сома
    syn.type = SYNAPSE_TYPE_CHEMICAL;
    syn.release_probability = 1.0f;
    syn.release_amount = 1.0f;
    syn.RRP_max = 0.0f;        // без RRP-моделирования (release = amount × p)
    syn.pre_area = 0.1f;       // ~0.1 µm² (типичный бутон)
    syn.bouton_Ca = 0.0001f;   // 100 нМ baseline (BOUTON_CA_BASELINE_MM)
    syn.tau_clearance = 5.0f;  // глутамат в щели ~5 мс (иначе expf(-dt/0)=0 → мгновенная очистка)

    uint64_t tc = 0;
    for (int t = 0; t < 2000; t++)   // settle 200 мс
        core_run_tick(segs, n, conns, n - 1, &syn, 1, 0, &tc, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

    CurrentStimulus stim = {0, 400.0f, 50.0f, tc};
    uint32_t stim_cnt = 1;
    float peak_terminal = -200.0f, peak_bouton_Ca = 0.0f, total_glu = 0.0f;
    for (int t = 0; t < 500; t++) {
        core_run_tick(segs, n, conns, n - 1, &syn, 1, 0, &tc, 0.1f,
                      &stim, &stim_cnt, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        if (segs[n - 1].V_m > peak_terminal) peak_terminal = segs[n - 1].V_m;
        if (syn.bouton_Ca > peak_bouton_Ca) peak_bouton_Ca = syn.bouton_Ca;
        total_glu += syn.cleft_neurotransmitter[NT_GLUTAMATE];
    }
    printf("[RELEASE] terminal_peak=%.1f mV | peak_bouton_Ca=%.3f mM (%.1f uM; SYT_KD=10 uM) | cleft_glu_sum=%.4f\n",
           peak_terminal, peak_bouton_Ca, peak_bouton_Ca * 1000.0f, total_glu);

    delete[] segs;
    delete[] conns;
}

// Фаза 4: изолированный ребаунд (bath-NMDA пейсмейкер + Ih + Na-де-инактивация).
// Свип гиперполяризации: глубина (амплитуда) × длительность. Ребаунд = spike после сброса IPSP.
TEST_CASE("dlc rebound: hyperpolarizing pulse sweep", "[dlc-axon][rebound]") {
    const uint32_t N_axon = 25;
    const uint32_t N = 1 + 1 + N_axon;
    for (float amp : {-100.0f, -200.0f, -400.0f, -800.0f}) {
        for (float dur : {20.0f, 50.0f, 100.0f}) {
            Segment* segs = new Segment[N]();
            IntraConnection* conns = new IntraConnection[N - 1]();
            uint32_t n = build_chain(segs, conns, N_axon, true);  // dlc, bath-NMDA ОСТАВЛЕНА (пейсмейкер)
            uint64_t tc = 0;
            // settle 500 мс — bath-NMDA должна вывести в пейсмейкерное состояние
            for (int t = 0; t < 5000; t++)
                core_run_tick(segs, n, conns, n - 1, nullptr, 0, 0, &tc, 0.1f,
                              nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                              nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            float rest = segs[0].V_m;
            // гиперполяризующий импульс (ингибиторный сброс)
            CurrentStimulus stim = {0, amp, dur, tc};
            uint32_t stim_cnt = 1;
            float min_v = 200.0f, peak_v = -200.0f;
            int spikes = 0;
            for (int t = 0; t < 1500; t++) {  // 150 мс после импульса
                core_run_tick(segs, n, conns, n - 1, nullptr, 0, 0, &tc, 0.1f,
                              &stim, &stim_cnt, nullptr, nullptr, nullptr, nullptr,
                              nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
                if (segs[0].V_m < min_v) min_v = segs[0].V_m;
                if (segs[0].V_m > peak_v) peak_v = segs[0].V_m;
                if (segs[0].V_m > 0.0f) spikes++;
            }
            printf("[REBOUND] amp=%.0f pA dur=%.0f ms | rest=%.1f min=%.1f peak=%.1f spikes=%d\n",
                   amp, dur, rest, min_v, peak_v, spikes);
            delete[] segs;
            delete[] conns;
        }
    }
}

// Полный dlc_MHR: сома + AIS + аксон + дендрит (HCN1+KV42) — как assembleNeuron.
static float g_hcn_mult = 1.0f;  // множитель плотности HCN1 дендрита (свип)
static float g_nap_mult = 1.0f;  // множитель плотности NaP сомы (свип)
static uint32_t build_dlc_full(Segment* segs, IntraConnection* conns, uint32_t N_axon) {
    // сома(0) + AIS(1) + аксон(2..2+N_axon-1) + дендрит(2+N_axon, 3+N_axon)
    uint32_t N = 2 + N_axon + 2;
    for (uint32_t i = 0; i < N; i++) segs[i] = Segment{};
    // сома
    segs[0].id = 0; segs[0].neuron_id = 0; segs[0].segment_type = 1;
    segs[0].radius = cpg::R_DIN; segs[0].length = cpg::R_DIN * 2; segs[0].V_m = cpg::V_DIN;
    cpg::init_ions(segs[0]); cpg::apply_mods(segs[0], cpg::dIN_soma_mods()); cpg::apply_state_params(segs[0]);
    for (uint8_t j = 0; j < segs[0].modifier_count; j++)
        if (segs[0].modifiers[j].type == MOD_NAP_CHANNEL) segs[0].modifiers[j].density *= g_nap_mult;
    // AIS
    segs[1].id = 1; segs[1].neuron_id = 0; segs[1].segment_type = 2;
    segs[1].radius = 0.2f; segs[1].length = cpg::AIS_L; segs[1].V_m = cpg::V_DIN;
    cpg::init_ions(segs[1]); cpg::apply_mods(segs[1], {{MOD_AXON_NAV12, 500.0f}, {MOD_K2P_LEAK, 4.17f}}); cpg::apply_state_params(segs[1]);
    // аксон
    for (uint32_t i = 0; i < N_axon; i++) {
        uint32_t k = 2 + i;
        segs[k].id = k; segs[k].neuron_id = 0; segs[k].segment_type = 2;
        segs[k].radius = 0.2f; segs[k].length = 40.0f; segs[k].V_m = cpg::V_DIN;
        cpg::init_ions(segs[k]);
        cpg::apply_mods(segs[k], {{MOD_AXON_NAV12, 2.0f}, {MOD_K2P_LEAK, 4.17f}, {MOD_CA_BUFFER, 0.5f}, {MOD_PMCA, 0.3f}});
        cpg::apply_state_params(segs[k]);
    }
    // дендрит (2 сегмента): HCN1 6× градиент × dens_mult (свип плотности), без KV42
    for (uint32_t i = 0; i < 2; i++) {
        uint32_t k = 2 + N_axon + i;
        segs[k].id = k; segs[k].neuron_id = 0; segs[k].segment_type = 2;
        segs[k].radius = 1.0f; segs[k].length = 30.0f; segs[k].V_m = cpg::V_DIN;
        cpg::init_ions(segs[k]);
        float hcn = (i == 0) ? 120.0f * g_hcn_mult : 720.0f * g_hcn_mult;  // 6× градиент × множитель
        cpg::apply_mods(segs[k], {{MOD_HCN1, hcn}, {MOD_CA_BUFFER, 0.5f}, {MOD_PMCA, 0.3f}});
        cpg::apply_state_params(segs[k]);
    }
    // связи: сома→AIS→аксон (линейно) + сома→дендрит (ветвь)
    uint32_t ci = 0;
    conns[ci++] = {0, 1, 1};                       // сома→AIS
    conns[ci++] = {1, 2, 1};                       // AIS→axon0
    for (uint32_t i = 0; i + 1 < N_axon; i++)
        conns[ci++] = {2 + i, 3 + i, 1};           // axon[i]→axon[i+1]
    conns[ci++] = {0, 2 + N_axon, 1};              // сома→дендрит0
    conns[ci++] = {2 + N_axon, 3 + N_axon, 1};     // дендрит0→дендрит1
    return N;
}

// Фаза 4: ребаунд ПОЛНОЙ морфологии (с дендритом) — свип амплитуды.
TEST_CASE("dlc rebound full morphology: hyperpolarizing sweep", "[dlc-axon][rebound-full]") {
    const uint32_t N_axon = 25;
    const uint32_t N = 2 + N_axon + 2;
    for (float hcn_mult : {1.0f, 3.0f, 10.0f}) {
    g_hcn_mult = hcn_mult;
    for (float amp : {-100.0f, -200.0f, -300.0f}) {
        Segment* segs = new Segment[N]();
        IntraConnection* conns = new IntraConnection[N - 1]();
        uint32_t n = build_dlc_full(segs, conns, N_axon);
        uint64_t tc = 0;
        for (int t = 0; t < 5000; t++)   // settle 500 мс
            core_run_tick(segs, n, conns, n - 1, nullptr, 0, 0, &tc, 0.1f,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        float rest = segs[0].V_m;
        CurrentStimulus stim = {0, amp, 50.0f, tc};
        uint32_t stim_cnt = 1;
        float min_v = 200.0f, peak_v = -200.0f;
        int spikes = 0;
        for (int t = 0; t < 4000; t++) {
            core_run_tick(segs, n, conns, n - 1, nullptr, 0, 0, &tc, 0.1f,
                          &stim, &stim_cnt, nullptr, nullptr, nullptr, nullptr,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            if (segs[0].V_m < min_v) min_v = segs[0].V_m;
            if (segs[0].V_m > peak_v) peak_v = segs[0].V_m;
            if (segs[0].V_m > 0.0f) spikes++;
        }
        printf("[RB-FULL] HCN×%.0f amp=%.0f | rest=%.1f min=%.1f peak=%.1f spikes=%d\n",
               hcn_mult, amp, rest, min_v, peak_v, spikes);
        delete[] segs;
        delete[] conns;
    }
    }
}

// Фаза 4: спайкует ли дендрит при спайке сомы (регенерация) или пассивно следует (затухание).
TEST_CASE("dlc spike propagation: soma vs dendrite", "[dlc-axon][propagation]") {
    const uint32_t N_axon = 25;
    const uint32_t N = 2 + N_axon + 2;
    g_hcn_mult = 1.0f;
    Segment* segs = new Segment[N]();
    IntraConnection* conns = new IntraConnection[N - 1]();
    uint32_t n = build_dlc_full(segs, conns, N_axon);
    // убрать bath-NMDA (чтобы сома была возбудима, как в Фазе 2)
    for (uint8_t j = 0; j < segs[0].modifier_count; j++) {
        if (segs[0].modifiers[j].type == MOD_NT_RECEPTOR_NMDA) {
            for (uint8_t k = j; k + 1 < segs[0].modifier_count; k++) segs[0].modifiers[k] = segs[0].modifiers[k + 1];
            segs[0].modifier_count--; break;
        }
    }
    uint64_t tc = 0;
    for (int t = 0; t < 2000; t++)   // settle 200 мс
        core_run_tick(segs, n, conns, n - 1, nullptr, 0, 0, &tc, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    CurrentStimulus stim = {0, 400.0f, 50.0f, tc};
    uint32_t stim_cnt = 1;
    for (int t = 0; t < 500; t++) {
        core_run_tick(segs, n, conns, n - 1, nullptr, 0, 0, &tc, 0.1f,
                      &stim, &stim_cnt, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        if (t % 25 == 0)
            printf("[PROP] t=%d soma=%.1f AIS=%.1f ax0=%.1f dend0=%.1f dend1=%.1f\n",
                   t, segs[0].V_m, segs[1].V_m, segs[2].V_m,
                   segs[2 + N_axon].V_m, segs[3 + N_axon].V_m);
    }
    delete[] segs;
    delete[] conns;
}

// Фаза 4: ребаунд от ТОНИЧЕСКОЙ гиперполяризации (Na h де-инактивирован) → сброс → ПД.
TEST_CASE("dlc rebound from tonic hyperpolarization", "[dlc-axon][rebound-tonic]") {
    const uint32_t N_axon = 25;
    const uint32_t N = 2 + N_axon + 2;
    g_hcn_mult = 3.0f;
    for (float nap_mult : {1.0f, 3.0f, 10.0f}) {
    g_nap_mult = nap_mult;
    for (float tonic : {-100.0f, -200.0f}) {
        Segment* segs = new Segment[N]();
        IntraConnection* conns = new IntraConnection[N - 1]();
        uint32_t n = build_dlc_full(segs, conns, N_axon);
        uint64_t tc = 0;
        for (int t = 0; t < 2000; t++)   // settle 200 мс
            core_run_tick(segs, n, conns, n - 1, nullptr, 0, 0, &tc, 0.1f,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        CurrentStimulus tonic_stim = {0, tonic, 300.0f, tc};  // тоническая гиперполяризация 300 мс
        uint32_t cnt = 1;
        for (int t = 0; t < 3000; t++)
            core_run_tick(segs, n, conns, n - 1, nullptr, 0, 0, &tc, 0.1f,
                          &tonic_stim, &cnt, nullptr, nullptr, nullptr, nullptr,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        float rest_tonic = segs[0].V_m;
        float peak_v = -200.0f; int spikes = 0;
        for (int t = 0; t < 3000; t++) {  // сброс тоники → ребаунд
            core_run_tick(segs, n, conns, n - 1, nullptr, 0, 0, &tc, 0.1f,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            if (segs[0].V_m > peak_v) peak_v = segs[0].V_m;
            if (segs[0].V_m > 0.0f) spikes++;
        }
        printf("[RB-TONIC] NaP×%.0f tonic=%.0f | rest_tonic=%.1f peak=%.1f spikes=%d\n",
               nap_mult, tonic, rest_tonic, peak_v, spikes);
        delete[] segs;
        delete[] conns;
    }
    }
}

// Фаза 4: ребаунд от ПОВТОРНЫХ IPSP (тоническое торможение cIN ~10 Гц).
// Медленный Ih накапливается между импульсами → ребаунд «пробивает» порог.
TEST_CASE("dlc rebound repeated IPSPs", "[dlc-axon][rebound-rep]") {
    const uint32_t N_axon = 25;
    const uint32_t N = 2 + N_axon + 2;
    for (float amp : {-50.0f, -100.0f, -200.0f}) {
        Segment* segs = new Segment[N]();
        IntraConnection* conns = new IntraConnection[N - 1]();
        uint32_t n = build_dlc_full(segs, conns, N_axon);
        uint64_t tc = 0;
        for (int t = 0; t < 5000; t++)   // settle 500 мс
            core_run_tick(segs, n, conns, n - 1, nullptr, 0, 0, &tc, 0.1f,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        // 15 импульсов −20мс через 50мс (20 Гц) — Ih копится (деактивация ~30мс)
        const int N_P = 15;
        CurrentStimulus stims[N_P];
        for (int i = 0; i < N_P; i++) stims[i] = {0, amp, 20.0f, tc + i * 50.0f};
        uint32_t stim_cnt = N_P;
        float min_v = 200.0f, peak_v = -200.0f;
        int spikes = 0;
        for (int t = 0; t < 15000; t++) {  // 1500 мс (15 импульсов + хвост)
            core_run_tick(segs, n, conns, n - 1, nullptr, 0, 0, &tc, 0.1f,
                          stims, &stim_cnt, nullptr, nullptr, nullptr, nullptr,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            if (segs[0].V_m < min_v) min_v = segs[0].V_m;
            if (segs[0].V_m > peak_v) peak_v = segs[0].V_m;
            if (segs[0].V_m > 0.0f) spikes++;
        }
        printf("[RB-REP] amp=%.0f | min=%.1f peak=%.1f spikes=%d\n", amp, min_v, peak_v, spikes);
        delete[] segs;
        delete[] conns;
    }
}

// INaT вольт-клэмп: пик transient Na dIN при шаге −80→−10 мВ, сверка со Svensson 1.13 pA/µm².
extern double nav12_current(Modifier* m, Segment* s, float dt);

TEST_CASE("dIN INaT voltage-clamp: peak vs Svensson 1.13 pA/um2", "[dlc-axon][inat]") {
    Segment s = {};
    s.V_m = -10.0f;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K]  = 4.0f;
    s.radius = 5.0f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_AXON_NAV12;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    // пик INaT: m активирован (m_inf(−10)), h≈1 (ещё не инактивирован), nf/ns≈0 (K закрыт)
    s.modifiers[0].state[0] = nav12_m_inf(-10.0f);
    s.modifiers[0].state[1] = 1.0f;
    s.modifiers[0].state[2] = 0.0f;
    s.modifiers[0].state[3] = 0.0f;
    double I_peak = nav12_current(&s.modifiers[0], &s, 0.0f);  // dt=0: без ионного потока
    float area = 4.0f * 3.14159265f * 5.0f * 5.0f;  // 314 µm²
    float J = (float)(-I_peak / area);  // pA/µm² (return = −I_total → I_total = −return; inward Na даёт J>0)
    printf("[INAT] m=%.3f h=1 V=-10: I_peak=%.1f pA, area=%.1f um2, J=%.3f pA/um2 (Svensson 1.13)\n",
           nav12_m_inf(-10.0f), -I_peak, area, J);
}

// Фаза 5 (2026-10-06): диагностика — ребаунд dIN требует «steady depolarisation» (Soffe 2009).
// Гипотеза: ребаунд-спайк идёт при удержании dIN у порога тоническим деполяризующим входом.
// Минимальная репродукция: изолированный dIN (сома+AIS+аксон, без NMDA/дендрита — униполярный Li 2001).
// Одна переменная × метрика: скан тонической деполяризации (drive) при фиксированном тормозном
// сбросе (hyper) → пик ребаунда V_m vs порог transient Na (−28 мВ), число спайков.
TEST_CASE("dIN rebound: steady depolarisation scan", "[dlc-axon][rebound-drive]") {
    const uint32_t N_axon = 1;   // dIN: сома + AIS + 1 аксон (SEGS_PER_NEURON=3, как в сети)
    const uint32_t N = 1 + 1 + N_axon;
    for (float hyper : {-100.0f, -200.0f}) {
        for (float drive : {0.0f, 25.0f, 50.0f, 75.0f, 100.0f, 150.0f, 200.0f, 300.0f}) {
            Segment* segs = new Segment[N]();
            IntraConnection* conns = new IntraConnection[N - 1]();
            uint32_t n = build_chain(segs, conns, N_axon, false);  // use_dlc=false → dIN
            uint64_t tc = 0;
            // settle 200 мс → покой (V_DIN=−52)
            for (int t = 0; t < 2000; t++)
                core_run_tick(segs, n, conns, n - 1, nullptr, 0, 0, &tc, 0.1f,
                              nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                              nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            float rest = segs[0].V_m;
            // фаза 1: тоническая гиперполяризация (тормозной сброс ~ глициновый IPSP) 300 мс
            CurrentStimulus hyper_stim = {0, hyper, 300.0f, tc};
            uint32_t hc = 1;
            for (int t = 0; t < 3000; t++)
                core_run_tick(segs, n, conns, n - 1, nullptr, 0, 0, &tc, 0.1f,
                              &hyper_stim, &hc, nullptr, nullptr, nullptr, nullptr,
                              nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            float rest_hyper = segs[0].V_m;
            // фаза 2: сброс торможения + steady depolarisation (скан) 500 мс
            CurrentStimulus drive_stim = {0, drive, 500.0f, tc};
            uint32_t dc = 1;
            float peak = -200.0f; int spikes = 0;
            for (int t = 0; t < 5000; t++) {
                core_run_tick(segs, n, conns, n - 1, nullptr, 0, 0, &tc, 0.1f,
                              &drive_stim, &dc, nullptr, nullptr, nullptr, nullptr,
                              nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
                if (segs[0].V_m > peak) peak = segs[0].V_m;
                if (segs[0].V_m > 0.0f) spikes++;
            }
            printf("[RB-DRIVE] hyper=%+.0f drive=%+.0f | rest=%.1f rest_hyper=%.1f peak=%.1f spikes=%d\n",
                   hyper, drive, rest, rest_hyper, peak, spikes);
            delete[] segs;
            delete[] conns;
        }
    }
}
