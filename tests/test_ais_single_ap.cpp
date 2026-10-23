// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_ais_single_ap.cpp — изолированный человеческий AIS (сома + 2×AIS + аксон).
// #34: проверка одиночного ПД после литературных плотностей AIS (Kv1.1=9, Kv1.2=5.6, Kv7=7)
// с градиентами Nav1.6 ↑дист(10×) / Nav1.2 ↓дист(0.1×) / Kv1.1/Kv1.2 ↑дист(3×) и реальной геометрией
// (AIS r=0.5 мкм, L=20 мкм×2; сома r=12; аксон r=0.8, L=50). Стимул → одиночный ПД → печать параметров.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstdio>
#include <cstring>
#include "types.h"
#include "bion_loader.h"
#include "kernel_dispatch.h"

extern "C" {
    void core_run_tick(Segment*, uint32_t, IntraConnection*, uint32_t,
                       Synapse*, uint32_t, int, uint64_t*, float,
                       CurrentStimulus*, uint32_t*, VoltageClamp*, uint32_t*,
                       RampStimulus*, uint32_t*, IonInjection*, uint32_t*,
                       LigandInjection*, uint32_t*, MechanoStimulus* mechano, uint32_t* mechano_count);
}

// Помощник: добавить модификатор с плотностью.
static void add_mod(Segment& s, uint8_t type, float density) {
    if (s.modifier_count < MAX_MODIFIERS_PER_SEGMENT) {
        Modifier& m = s.modifiers[s.modifier_count++];
        m.type = type; m.density = density;
    }
}

// Инициализация ионов (как init_ions в cpg_config.h) + физиологичная температура.
static void init_ions(Segment& s) {
    s.intra_ions[ION_NA] = 10.0f;   // Na⁺ мМ
    s.intra_ions[ION_K]  = 140.0f;  // K⁺ мМ
    s.intra_ions[ION_CA] = 0.0001f; // Ca²⁺ мМ
    s.intra_ions[ION_CL] = 10.0f;   // Cl⁻ мМ
    s.intra_ions[ION_MG] = 1.0f;    // Mg²⁺ мМ
    s.extra_ions[ION_NA] = 120.0f;
    s.extra_ions[ION_K]  = 5.0f;
    s.extra_ions[ION_CA] = 1.0f;
    s.extra_ions[ION_CL] = 120.0f;
    s.extra_ions[ION_MG] = 1.0f;
    s.temperature = 37.0f;   // физиологичная (37°C)
    s.ATP_level   = 1.0f;
    s.ER_Ca2      = 0.5f;
}

TEST_CASE("AIS single AP: human densities + gradients + geometry", "[ais-single-ap]") {
    const uint32_t N = 4;   // сома(0), AIS-прокс(1), AIS-дист(2), аксон(3)
    Segment segs[N] = {};
    IntraConnection conns[N-1] = {};

    // ── Геометрия (реальный нейрон: сома r=12, AIS r=0.5/L=20×2, аксон r=0.8/L=50) ──
    segs[0].id = 0; segs[0].neuron_id = 0; segs[0].segment_type = 1;  // SOMA
    segs[0].radius = 12.0f; segs[0].length = 24.0f; segs[0].V_m = -65.0f;
    segs[1].id = 1; segs[1].neuron_id = 0; segs[1].segment_type = 2;  // AIS прокс
    segs[1].radius = 0.5f; segs[1].length = 20.0f; segs[1].V_m = -65.0f;
    segs[2].id = 2; segs[2].neuron_id = 0; segs[2].segment_type = 2;  // AIS дист
    segs[2].radius = 0.5f; segs[2].length = 20.0f; segs[2].V_m = -65.0f;
    segs[3].id = 3; segs[3].neuron_id = 0; segs[3].segment_type = 2;  // AXON
    segs[3].radius = 0.8f; segs[3].length = 50.0f; segs[3].V_m = -65.0f;
    for (uint32_t i = 0; i < N; i++) init_ions(segs[i]);

    // ── Сома: Na/K-спайк + leak + Cl⁻-утечка + насос (калиброванные плотности, как CPG) ──
    add_mod(segs[0], MOD_AXON_NAV12, 15.6f);
    add_mod(segs[0], MOD_KV21, 3.84f);
    add_mod(segs[0], MOD_K2P_LEAK, 12.5f);  // G≈1.5 мС/см² (TREK-1) — баланс NALCN (G=0.3) без n⁴ покой-K
    add_mod(segs[0], MOD_NALCN, 0.11f);      // G=0.3 мС/см² (γ=27 пС)
    add_mod(segs[0], MOD_IONOTROPIC_CL, 0.1f);  // Cl⁻-утечка (баланс заряда: Na/K насос + K2P выносят катионы)
    add_mod(segs[0], MOD_PUMP_NAK, 0.6f);    // 3 µA/см² (BIOREF 2-5)

    // ── AIS: Nav1.6/Nav1.2 + Kv1.1/Kv1.2/Kv7 + leak, с градиентами ──
    // #34: базовая ΣNa=500 (G≈0.5 С/см², верх Kole & Stuart 2012), сегрегация Tian 2014:
    // Nav1.2 проксимально (350), Nav1.6 дистально (150→450 = ×3, Lorincz & Nusser 2008).
    add_mod(segs[1], MOD_AXON_NAV12, 350.0f);
    add_mod(segs[1], MOD_AXON_NAV16, 150.0f);
    add_mod(segs[1], MOD_KV11, 9.0f);
    add_mod(segs[1], MOD_KV12, 5.6f);
    add_mod(segs[1], MOD_KV7_MCURRENT, 7.0f);
    add_mod(segs[1], MOD_K2P_LEAK, 8.0f);    // AIS K2P (гиперполяр. покой)
    add_mod(segs[1], MOD_PUMP_NAK, 0.6f);    // Na/K-АТФаза (равномерно по мембране, вкл. AIS)
    // AIS-дист: Nav1.2↓ (50), Nav1.6↑ (450), Kv1.1×3=27, Kv1.2×3=16.8, Kv7=7
    add_mod(segs[2], MOD_AXON_NAV12, 50.0f);
    add_mod(segs[2], MOD_AXON_NAV16, 450.0f);
    add_mod(segs[2], MOD_KV11, 27.0f);
    add_mod(segs[2], MOD_KV12, 16.8f);
    add_mod(segs[2], MOD_KV7_MCURRENT, 7.0f);
    add_mod(segs[2], MOD_K2P_LEAK, 8.0f);
    add_mod(segs[2], MOD_PUMP_NAK, 0.6f);    // Na/K-АТФаза (равномерно по мембране, вкл. AIS)

    // ── Аксон: Na/K + leak ──
    add_mod(segs[3], MOD_AXON_NAV16, 50.0f);
    add_mod(segs[3], MOD_KV11, 5.0f);
    add_mod(segs[3], MOD_K2P_LEAK, 0.03f);
    add_mod(segs[3], MOD_NALCN, 0.02f);
    add_mod(segs[3], MOD_PUMP_NAK, 0.6f);    // Na/K-АТФаза (равномерно по мембране, вкл. аксон)

    // ── Связи: сома→AIS-прокс→AIS-дист→аксон ──
    for (uint32_t i = 0; i < N-1; i++) {
        conns[i].segment_a = i; conns[i].segment_b = i+1; conns[i].is_intracellular = 1;
    }

    uint64_t tc = 0;
    uint32_t nseg = N, nconn = N-1;

    printf("[DEBUG] membrane currents at V=-65 (segment_area):\n");
    for (int i = 0; i < (int)N; i++) {
        float area = (segs[i].segment_type == 1 || segs[i].segment_type == 3 || segs[i].segment_type == 4)
            ? 4.0f * 3.14159265f * segs[i].radius * segs[i].radius
            : 2.0f * 3.14159265f * segs[i].radius * segs[i].length;
        float Cm = area * 0.01f;
        float Itot = 0.0f;
        for (int j = 0; j < segs[i].modifier_count; j++) {
            float I = dispatch_modifier_current(0, segs[i].modifiers[j].type, &segs[i].modifiers[j], &segs[i], 0.1f);
            Itot += I;
            printf("  seg%d[%d] type=%d I=%.3f pA\n", i, j, segs[i].modifiers[j].type, I);
        }
        printf("[DEBUG] seg%d area=%.1f Cm=%.3fpF I_total=%.2f pA  dV/dt=%.1f mV/ms\n",
               i, area, Cm, Itot, Itot / Cm);
    }

    // 1) Settle: 3000 тиков (300 мс) — релаксация гейтов к покою.
    for (int t = 0; t < 3000; t++) {
        core_run_tick(segs, nseg, conns, nconn, nullptr, 0, 0, &tc, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        if (t < 5 || t % 100 == 0) {
            printf("[TRACE] t=%d soma=%.2f AISp=%.2f AISd=%.2f axon=%.2f | Na_i soma=%.3f AISp=%.3f AISd=%.3f axon=%.3f\n",
                   t, segs[0].V_m, segs[1].V_m, segs[2].V_m, segs[3].V_m,
                   segs[0].intra_ions[0], segs[1].intra_ions[0], segs[2].intra_ions[0], segs[3].intra_ions[0]);
        }
        for (int i = 0; i < (int)N; i++)
            for (int ion = 0; ion < ION_COUNT; ion++)
                if (std::isnan(segs[i].intra_ions[ion]) || std::isnan(segs[i].extra_ions[ion])) {
                    printf("[DEBUG] NaN at tick %d seg=%d ion=%d (intra=%.4f extra=%.4f)\n",
                           t, i, ion, segs[i].intra_ions[ion], segs[i].extra_ions[ion]);
                    goto done_settle;
                }
    }
done_settle:;
    printf("[DEBUG] tc after settle = %llu, soma V_m=%.4f\n", (unsigned long long)tc, segs[0].V_m);
    printf("[DEBUG] soma ions after settle: intra Na=%.4f K=%.4f Ca=%.4f Cl=%.4f Mg=%.4f | extra Na=%.4f K=%.4f\n",
           segs[0].intra_ions[0], segs[0].intra_ions[1], segs[0].intra_ions[2], segs[0].intra_ions[3], segs[0].intra_ions[4],
           segs[0].extra_ions[0], segs[0].extra_ions[1]);
    for (int j = 0; j < segs[0].modifier_count; j++) {
        float I = dispatch_modifier_current(0, segs[0].modifiers[j].type, &segs[0].modifiers[j], &segs[0], 0.1f);
        printf("[DEBUG] soma mod type=%d dens=%.2f I=%.6f\n", segs[0].modifiers[j].type, segs[0].modifiers[j].density, I);
    }
    float rest[4];
    for (int i = 0; i < 4; i++) rest[i] = segs[i].V_m;
    printf("[AIS-AP] rest: soma=%.2f AISprox=%.2f AISdist=%.2f axon=%.2f mV\n",
           rest[0], rest[1], rest[2], rest[3]);

    // 2) Электрод в сому: 400 пА, 50 мс.
    CurrentStimulus stim = {0, 400.0f, 50.0f, tc};
    uint32_t stim_cnt = 1;
    float peak[4] = {-200, -200, -200, -200};
    int spike_tick[4] = {0, 0, 0, 0};
    bool first_spike = true;
    float threshold = -200.0f;   // V_m в момент превышения dV/dt порога
    float prev_v = segs[0].V_m;
    for (int t = 0; t < 500; t++) {
        core_run_tick(segs, nseg, conns, nconn, nullptr, 0, 0, &tc, 0.1f,
                      &stim, &stim_cnt, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        if (t == 1 || t == 10 || t == 100 || t == 499)
            printf("[DEBUG] t=%d soma=%.3f AISprox=%.3f I_buf_soma=%.4f\n",
                   t, segs[0].V_m, segs[1].V_m, segs[0].I_buffer);
        for (int i = 0; i < 4; i++) {
            if (segs[i].V_m > peak[i]) peak[i] = segs[i].V_m;
            if (first_spike && i == 1) {
                // порог по dV/dt AIS (> 10 мВ/0.1мс = 100 мВ/мс)
                if ((segs[1].V_m - prev_v) > 10.0f) { threshold = prev_v; first_spike = false; spike_tick[1] = t; }
            }
        }
        prev_v = segs[1].V_m;
    }
    printf("[AIS-AP] peaks: soma=%.2f AISprox=%.2f AISdist=%.2f axon=%.2f mV\n",
           peak[0], peak[1], peak[2], peak[3]);
    printf("[AIS-AP] AIS threshold~%.2f mV (dV/dt>10mV/0.1ms)\n", threshold);

    // 3) Проверки: покой биологичный, AIS спайкует, ПД доходит до аксона.
    CHECK(rest[1] < -45.0f);       // AIS покой не runaway
    CHECK(peak[1] > 0.0f);         // AIS спайкует (пик > 0)
    CHECK(peak[3] > -20.0f);       // аксон получил ПД
}
