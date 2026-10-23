// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_ion_conservation.cpp — изоляция: одиночный MN (сома+AIS+аксон, без синапсов/глии).
// Проверка консервации ионов: взвешенная по объёму сумма
// Σ (intra_ions × V_intra + extra_ions × 10 × V_intra) должна быть константой.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstdio>
#include <cstring>
#include <vector>
#include "types.h"
#include "bion_loader.h"
#include "cpg_config.h"
#include "run_tick_common.h"  // segment_volume_compute
#include "modifier_helpers.h"  // ghk_current_pA, segment_area — прямая диагностика

extern "C" {
    void core_run_tick(Segment*, uint32_t, IntraConnection*, uint32_t,
                       Synapse*, uint32_t, int, uint64_t*, float,
                       CurrentStimulus*, uint32_t*, VoltageClamp*, uint32_t*,
                       RampStimulus*, uint32_t*, IonInjection*, uint32_t*,
                       LigandInjection*, uint32_t*, MechanoStimulus*, uint32_t*);
}
extern double nalcn_current(Modifier*, Segment*, float);  // прямая диагностика
extern double nmda_current(Modifier*, Segment*, float);
extern double ampa_current(Modifier*, Segment*, float);

static void weighted_sum(const BionData& d, float* na_i, float* na_e, float* k_i, float* k_e) {
    float s_nai = 0.0f, s_nae = 0.0f, s_ki = 0.0f, s_ke = 0.0f;
    for (uint32_t i = 0; i < d.num_segments; i++) {
        float vol = segment_volume_compute(&d.segments[i]);
        s_nai += d.segments[i].intra_ions[0] * vol;
        s_nae += d.segments[i].extra_ions[0] * EXTRA_VOLUME_RATIO * vol;
        s_ki  += d.segments[i].intra_ions[1] * vol;
        s_ke  += d.segments[i].extra_ions[1] * EXTRA_VOLUME_RATIO * vol;
    }
    *na_i = s_nai; *na_e = s_nae; *k_i = s_ki; *k_e = s_ke;
}

// Диагностика ионных потоков: изолированный сегмент MN-сомы (clamp V_m=-61),
// по одному модификатору — измеряем Δионов за 200 мс. Честный путь: тот же
// core_run_tick, что и в симуляции (без прямой подмены формул).
static void flux_decompose(const char* label,
                           const std::vector<std::pair<uint8_t, float>>& mods,
                           int N = 2000, bool clamp_vm = true, float cl_init = 5.0f, float k_o_init = 5.0f) {
    Segment s = {};
    s.id = 0; s.neuron_id = 0;
    s.radius = 6.0f; s.length = 12.0f; s.V_m = -61.0f; s.segment_type = 1;
    cpg::init_ions(s);
    s.intra_ions[3] = cl_init;   // переопределить Cl_i (для проверки обратимости KCC2)
    s.extra_ions[1] = k_o_init;  // переопределить K_o (для проверки driving KCC2)
    cpg::apply_mods(s, mods);
    cpg::apply_state_params(s);

    float na0 = s.intra_ions[0], k0 = s.intra_ions[1], ca0 = s.intra_ions[2], cl0 = s.intra_ions[3];
    uint64_t tc = 0;
    for (int t = 0; t < N; t++) {
        core_run_tick(&s, 1, nullptr, 0, nullptr, 0, 0, &tc, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        if (clamp_vm) s.V_m = -61.0f;  // voltage clamp покоя
    }
    printf("[FLUX] %-8s dNa=%+8.4f dK=%+8.4f dCl=%+8.4f dCa=%+8.4f  (мМ/%dмс, V_m=%.3f K_i=%.3f Cl_i=%.4f K_o=%.3f Cl_o=%.3f | ΣK=%.3f ΣCl=%.3f ΣNa=%.3f)\n",
           label, s.intra_ions[0]-na0, s.intra_ions[1]-k0,
           s.intra_ions[3]-cl0, s.intra_ions[2]-ca0, N/10,
           s.V_m, s.intra_ions[1], s.intra_ions[3], s.extra_ions[1], s.extra_ions[3],
           s.intra_ions[1] + s.extra_ions[1]*EXTRA_VOLUME_RATIO,
           s.intra_ions[3] + s.extra_ions[3]*EXTRA_VOLUME_RATIO,
           s.intra_ions[0] + s.extra_ions[0]*EXTRA_VOLUME_RATIO);
}

TEST_CASE("MN ion flux decomposition (rest -61)", "[ion-flux]") {
    // Прямой GHK-флюкс (без модификатора) — эталонная форма потока при V=-61:
    // Na in=15/out=140, K in=140/out=5, Cl in=5/out=130, Ca in=1e-4/out=1.2
    printf("[GHK] f_Na(-61)=%+.3e  f_K(-61)=%+.3e  f_Cl(-61)=%+.3e  f_Ca(-61)=%+.3e  (flux C/m3)\n",
           ghk_monovalent(-61.0f, 15.0f, 140.0f),
           ghk_monovalent(-61.0f, 140.0f, 5.0f),
           ghk_anion(-61.0f, 5.0f, 130.0f),
           ghk_divalent(-61.0f, 0.0001f, 1.2f));
    // Прямой расчёт NALCN токов (γ=27, density=0.1, P_Na:P_K:P_Ca=0.433:0.400:0.167):
    {
        Segment probe = {};
        probe.radius = 6.0f; probe.length = 12.0f; probe.segment_type = 1;
        cpg::init_ions(probe);
        float area = segment_area(&probe);
        double P_Na, P_K, P_Ca;
        nalcn_partial_P(27.0f, &P_Na, &P_K, &P_Ca);   // γ=27, fNa:fK:fCa=1.3:1.2:0.5 (Lu 2007)
        double N = 0.1f * area;                         // density 0.1 × area
        double Ina = P_Na * ghk_monovalent(-61.0f, probe.intra_ions[ION_NA], probe.extra_ions[ION_NA]) * N * A_TO_PA;
        double Ik  = P_K  * ghk_monovalent(-61.0f, probe.intra_ions[ION_K],  probe.extra_ions[ION_K])  * N * A_TO_PA;
        double Ica = P_Ca * ghk_divalent(-61.0f, probe.intra_ions[ION_CA], probe.extra_ions[ION_CA]) * N * A_TO_PA;
        printf("[NALCN-direct] area=%.1f Ina=%+.4f Ik=%+.4f Ica=%+.4f (пА)\n", area, Ina, Ik, Ica);
    }
    // Прямой вызов nalcn_current (один тик) — локализация: сама функция vs core_run_tick
    {
        Segment probe = {};
        probe.radius = 6.0f; probe.length = 12.0f; probe.segment_type = 1;
        cpg::init_ions(probe);
        cpg::apply_mods(probe, {{MOD_NALCN, 0.1f}});
        cpg::apply_state_params(probe);
        probe.V_m = -61.0f;
        float k0 = probe.intra_ions[1], na0 = probe.intra_ions[0];
        float I = nalcn_current(&probe.modifiers[0], &probe, 0.1f);
        printf("[NALCN-call] I=%.4f dNa=%.6f dK=%.6f (один тик)\n",
               I, probe.intra_ions[0]-na0, probe.intra_ions[1]-k0);
    }
    // NMDA (Ca²⁺-проницаемый) — проверка NaN после double (рецепторы catCD падали)
    {
        Segment probe = {};
        probe.radius = 6.0f; probe.length = 12.0f; probe.segment_type = 1;
        cpg::init_ions(probe);
        cpg::apply_mods(probe, {{MOD_NT_RECEPTOR_NMDA, 0.1f}});
        probe.neurotransmitter[NT_GLUTAMATE] = 1.0f;
        probe.modifiers[0].state[0] = 1.0f;  // r активация
        probe.V_m = -61.0f;
        float I_nmda = nmda_current(&probe.modifiers[0], &probe, 0.1f);
        float I_ampa = ampa_current(&probe.modifiers[0], &probe, 0.1f);
        printf("[REC-call] NMDA I=%.4f AMPA I=%.4f (NaN nmda=%d ampa=%d)\n",
               I_nmda, I_ampa, (I_nmda!=I_nmda), (I_ampa!=I_ampa));
    }
    flux_decompose("K2P",    {{MOD_K2P_LEAK, cpg::MN_K2P}});
    flux_decompose("NALCN",  {{MOD_NALCN, cpg::MN_NALCN_DENS}});
    flux_decompose("PUMP",   {{MOD_PUMP_NAK, cpg::MN_PUMP}});
    flux_decompose("KCC2",   {{MOD_KCC2, cpg::KCC2}});
    // Обратимость: при Cl_i=3 (ниже равновесия 4.64) KCC2 должен ВВОДИТЬ Cl (driving<0).
    flux_decompose("KCC2@Cl3", {{MOD_KCC2, cpg::KCC2}}, 2000, true, 3.0f);
    // Тот же низкий Cl_i, но высокий K_o=40 (как в полной симуляции после K2P-вывода).
    flux_decompose("KCC2@Cl3Ko40", {{MOD_KCC2, cpg::KCC2}}, 2000, true, 3.0f, 40.0f);
    flux_decompose("Cl",     {{MOD_IONOTROPIC_CL, cpg::MN_CL_LEAK}});
    flux_decompose("Nav1.2", {{MOD_AXON_NAV12, cpg::MN_NA_DENS}});
    flux_decompose("ALL",    cpg::MN_soma_mods());
    // Длинный прогон полного MN — сходится ли KCC2 к равновесию [K]i[Cl]i=[K]o[Cl]o
    // (Cl_i_eq = 5·130/140 = 4.64 мМ), останавливая K_i-дрейф.
    flux_decompose("ALL(2s)", cpg::MN_soma_mods(), 20000);
    flux_decompose("ALL(20s)", cpg::MN_soma_mods(), 200000);
    // Без clamp: куда уезжает покой (V_rest) при текущих плотностях.
    flux_decompose("REST(2s)", cpg::MN_soma_mods(), 20000, false);
    flux_decompose("REST(10s)", cpg::MN_soma_mods(), 100000, false);
    flux_decompose("REST(30s)", cpg::MN_soma_mods(), 300000, false);
    flux_decompose("REST(60s)", cpg::MN_soma_mods(), 600000, false);
    flux_decompose("REST(300s)", cpg::MN_soma_mods(), 3000000, false);
    flux_decompose("REST(600s)", cpg::MN_soma_mods(), 6000000, false);
    SUCCEED();
}

TEST_CASE("MN ion conservation (isolated)", "[ion-conservation]") {
    cpg::CpgNetwork net;
    cpg::add_neuron(net, 0, cpg::Y_MN, 0, cpg::R_MN, cpg::V_MN,
                    cpg::MN_soma_mods(), cpg::MN_ais_mods(), cpg::MN_axon_mods());
    char path[256]; snprintf(path, sizeof(path), "/tmp/ion_cons.bion");
    cpg::save_bion(net, path);
    BionData d; memset(&d, 0, sizeof(d));
    if (bion_load_host(path, &d) != 0) { std::remove(path); FAIL("load fail"); }

    uint64_t tc = 0;
    float nai0, nae0, ki0, ke0, nai, nae, ki, ke;
    weighted_sum(d, &nai0, &nae0, &ki0, &ke0);
    printf("[CONS] t=0 Na_i=%.1f Na_e=%.1f K_i=%.1f K_e=%.1f V_m=%.2f\n", nai0, nae0, ki0, ke0, d.segments[0].V_m);
    for (int t = 0; t < 20000; t++) {
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        if (t % 2000 == 1999) {
            weighted_sum(d, &nai, &nae, &ki, &ke);
            printf("[CONS] t=%d Na_i=%.1f(d=%.1f) Na_e=%.1f(d=%.1f) K_i=%.1f(d=%.1f) K_e=%.1f(d=%.1f) V_m=%.2f\n",
                   t + 1, nai, nai-nai0, nae, nae-nae0, ki, ki-ki0, ke, ke-ke0, d.segments[0].V_m);
        }
    }
    bion_unload_host(&d); std::remove(path);
    SUCCEED();
}

TEST_CASE("synapse NaN check (pre/post + NMDA)", "[syn-nan]") {
    Segment pre = {}, post = {};
    pre.id = 0; pre.neuron_id = 0; pre.radius = 5; pre.length = 10; pre.segment_type = 1; pre.V_m = -55;
    post.id = 1; post.neuron_id = 1; post.radius = 5; post.length = 10; post.segment_type = 1; post.V_m = -65;
    cpg::init_ions(pre);
    cpg::init_ions(post);
    cpg::apply_mods(pre, {{MOD_AXON_NAV12, 15.6f}, {MOD_KV21, 3.84f}, {MOD_PUMP_NAK, 0.2f}, {MOD_K2P_LEAK, 0.03f}});
    cpg::apply_state_params(pre);
    cpg::apply_mods(post, {{MOD_AXON_NAV12, 15.6f}, {MOD_KV21, 3.84f}, {MOD_PUMP_NAK, 0.2f}, {MOD_K2P_LEAK, 0.03f}, {MOD_NT_RECEPTOR_NMDA, 0.1f}});
    cpg::apply_state_params(post);

    Synapse syn = {};
    syn.id = 0; syn.pre_segment = 0; syn.post_segment = 1; syn.type = SYNAPSE_TYPE_CHEMICAL;
    syn.weight = 0.5f; syn.release_probability = 1.0f; syn.release_amount = 1.0f;
    syn.tau_clearance = 5.0f; syn.pre_area = 0.1f; syn.post_area = 0.1f;
    syn.post_receptor_count = 1;
    syn.post_receptors[0].type = MOD_NT_RECEPTOR_NMDA;
    syn.post_receptors[0].density = 0.1f;

    // Сохранить в файл (как write_bion_2seg_syn) и загрузить через bion_load_host
    char path[256]; snprintf(path, sizeof(path), "/tmp/syn_nan.bion");
    NetworkHeader h = {}; h.magic = BION_MAGIC; h.version = BION_VERSION;
    h.num_neurons = 2; h.num_segments = 2; h.num_synapses = 1;
    Neuron n0 = {}; n0.segment_count = 1; n0.threshold = -20;
    Neuron n1 = {}; n1.id = 1; n1.segment_start = 1; n1.segment_count = 1; n1.threshold = -20;
    FILE* f = fopen(path, "wb");
    fwrite(&h, sizeof(h), 1, f);
    fwrite(&n0, sizeof(n0), 1, f); fwrite(&n1, sizeof(n1), 1, f);
    fwrite(&pre, sizeof(pre), 1, f); fwrite(&post, sizeof(post), 1, f);
    fwrite(&syn, sizeof(syn), 1, f);
    fclose(f);

    BionData d; memset(&d, 0, sizeof(d));
    if (bion_load_host(path, &d) != 0) { std::remove(path); FAIL("load fail"); }

    uint64_t tc = 0;
    int nan_tick = -1;
    for (int t = 0; t < 100; t++) {
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        if (d.segments[0].V_m != d.segments[0].V_m || d.segments[1].V_m != d.segments[1].V_m) { nan_tick = t; break; }
    }
    printf("[SYN-NAN] pre V_m=%.3f post V_m=%.3f (nan_tick=%d)\n", d.segments[0].V_m, d.segments[1].V_m, nan_tick);
    bion_unload_host(&d); std::remove(path);
    SUCCEED();
}

TEST_CASE("CPG full network ion drift (with glia)", "[cpg-drift]") {
    cpg::CpgNetwork net = cpg::build_cpg(1, 0);
    char path[256]; snprintf(path, sizeof(path), "/tmp/cpg_drift.bion");
    cpg::save_bion(net, path);
    BionData d; memset(&d, 0, sizeof(d));
    if (bion_load_host(path, &d) != 0) { std::remove(path); FAIL("load fail"); }

    uint64_t tc = 0;
    int N = d.num_segments;
    for (int t = 0; t < 600000; t++) {  // 60 с
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        if (t % 100000 == 99999) {
            float sNa=0, sK=0, sCl=0, sK_o=0, vmin=1e9, vmax=-1e9, vglia=0, kglia=0, koglia=0; int nglia=0;
            for (int i = 0; i < N; i++) {
                float vol = segment_volume_compute(&d.segments[i]);
                sNa += (d.segments[i].intra_ions[0] + d.segments[i].extra_ions[0]*EXTRA_VOLUME_RATIO)*vol;
                sK  += (d.segments[i].intra_ions[1] + d.segments[i].extra_ions[1]*EXTRA_VOLUME_RATIO)*vol;
                sCl += (d.segments[i].intra_ions[3] + d.segments[i].extra_ions[3]*EXTRA_VOLUME_RATIO)*vol;
                sK_o += d.segments[i].extra_ions[1]*vol;
                if (d.segments[i].neuron_id >= 0xFFFFFFFEu) {  // глия (радиальная 0xFFFFFFFE / астроцит 0xFFFFFFFF)
                    vglia += d.segments[i].V_m; kglia += d.segments[i].intra_ions[1]; koglia += d.segments[i].extra_ions[1]; nglia++;
                }
                if (d.segments[i].V_m < vmin) vmin = d.segments[i].V_m;
                if (d.segments[i].V_m > vmax) vmax = d.segments[i].V_m;
            }
            sK_o /= (N ? N : 1);
            if (nglia) { vglia/=nglia; kglia/=nglia; koglia/=nglia; }
            printf("[CPG] t=%d ΣNa=%.1f ΣK=%.1f ΣCl=%.1f K_o_avg=%.2f V=[%.1f..%.1f] GLIA:V=%.1f K_i=%.1f K_o=%.2f\n",
                   t+1, sNa, sK, sCl, sK_o, vmin, vmax, vglia, kglia, koglia);
        }
    }
    bion_unload_host(&d); std::remove(path);
    SUCCEED();
}
