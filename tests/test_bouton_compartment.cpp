// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_bouton_compartment.cpp — #9: проверка стабильности бутона как ОТДЕЛЬНОГО V_m-компартмента.
// Гипотеза: после #42 (Scharfetter-Gummel + NP_SUBSTEPS=20) бутон (C_m ~0.03 пФ) стабилен,
// т.е. аттенюацию пресинаптического ПД можно выводить из ионной динамики, а не множителем.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstdio>
#include <cstring>
#include <cmath>
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

static void add_mod(Segment& s, uint8_t type, float density) {
    if (s.modifier_count < MAX_MODIFIERS_PER_SEGMENT) {
        Modifier& m = s.modifiers[s.modifier_count++];
        m.type = type; m.density = density;
    }
}

static void init_ions(Segment& s) {
    s.intra_ions[ION_NA] = 10.0f;
    s.intra_ions[ION_K]  = 140.0f;
    s.intra_ions[ION_CA] = 0.0001f;
    s.intra_ions[ION_CL] = 10.0f;
    s.intra_ions[ION_MG] = 1.0f;
    s.extra_ions[ION_NA] = 120.0f;
    s.extra_ions[ION_K]  = 5.0f;
    s.extra_ions[ION_CA] = 1.0f;
    s.extra_ions[ION_CL] = 120.0f;
    s.extra_ions[ION_MG] = 1.0f;
    s.temperature = 37.0f;
    s.ATP_level = 1.0f;
    s.ER_Ca2 = 0.5f;
}

TEST_CASE("Bouton compartment: V_m stability (no runaway)", "[bouton-compartment]") {
    const uint32_t N = 3;   // сома(0), аксон(1), бутон(2)
    Segment segs[N] = {};
    IntraConnection conns[N-1] = {};

    // Геометрия: сома r=12/L=24, аксон r=0.8/L=50, бутон (сфера) r=0.5
    segs[0].id = 0; segs[0].neuron_id = 0; segs[0].segment_type = 1;  // SOMA
    segs[0].radius = 12.0f; segs[0].length = 24.0f; segs[0].V_m = -65.0f;
    segs[1].id = 1; segs[1].neuron_id = 0; segs[1].segment_type = 2;  // AXON
    segs[1].radius = 0.8f; segs[1].length = 50.0f; segs[1].V_m = -65.0f;
    segs[2].id = 2; segs[2].neuron_id = 0; segs[2].segment_type = 3;  // PRESYNAPSE (сфера)
    segs[2].radius = 0.5f; segs[2].length = 1.0f; segs[2].V_m = -65.0f;
    for (uint32_t i = 0; i < N; i++) init_ions(segs[i]);

    // Сома (как test_ais_single_ap)
    add_mod(segs[0], MOD_AXON_NAV12, 15.6f);
    add_mod(segs[0], MOD_KV21, 3.84f);
    add_mod(segs[0], MOD_K2P_LEAK, 4.17f);
    add_mod(segs[0], MOD_NALCN, 0.11f);
    add_mod(segs[0], MOD_IONOTROPIC_CL, 0.1f);
    add_mod(segs[0], MOD_PUMP_NAK, 0.6f);
    // Аксон
    add_mod(segs[1], MOD_AXON_NAV16, 50.0f);
    add_mod(segs[1], MOD_KV11, 5.0f);
    add_mod(segs[1], MOD_K2P_LEAK, 0.03f);
    add_mod(segs[1], MOD_NALCN, 0.02f);
    add_mod(segs[1], MOD_PUMP_NAK, 0.6f);
    // Бутон: Cav2.1 (Ca²⁺-вход) + PMCA/буфер (Ca²⁺-спад) + K2P (leak). БЕЗ Na — en passant.
    add_mod(segs[2], MOD_CAV21, 1.0f);
    add_mod(segs[2], MOD_PMCA, 1.0f);       // Ca²⁺-выкачка плазмалеммы (иначе Ca²⁺ копится)
    add_mod(segs[2], MOD_CA_BUFFER, 0.15f); // Ca²⁺-буфер
    add_mod(segs[2], MOD_K2P_LEAK, 0.1f);

    conns[0].segment_a = 0; conns[0].segment_b = 1; conns[0].is_intracellular = 1;
    conns[1].segment_a = 1; conns[1].segment_b = 2; conns[1].is_intracellular = 1;

    uint64_t tc = 0;
    uint32_t nseg = N, nconn = N-1;

    bool nan_seen = false;
    for (int t = 0; t < 3000; t++) {
        core_run_tick(segs, nseg, conns, nconn, nullptr, 0, 0, &tc, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        if (t < 5 || t % 500 == 0) {
            printf("[TRACE] t=%d soma=%.2f axon=%.2f bouton=%.3f | Ca_bouton=%.4f\n",
                   t, segs[0].V_m, segs[1].V_m, segs[2].V_m, segs[2].intra_ions[ION_CA]);
        }
        for (uint32_t i = 0; i < N; i++)
            for (int ion = 0; ion < ION_COUNT; ion++)
                if (std::isnan(segs[i].intra_ions[ion]) || std::isnan(segs[i].extra_ions[ion])) {
                    printf("[DEBUG] NaN at tick %d seg=%u ion=%d\n", t, i, ion);
                    nan_seen = true; goto done_settle;
                }
    }
done_settle:;

    printf("[RESULT] bouton V_m=%.3f mV, Ca_i=%.5f mM, nan=%d\n",
           segs[2].V_m, segs[2].intra_ions[ION_CA], nan_seen ? 1 : 0);

    CHECK_FALSE(nan_seen);                 // нет NaN
    CHECK(segs[2].V_m > -200.0f);          // нет runaway вниз
    CHECK(segs[2].V_m < 200.0f);           // нет runaway вверх
}
