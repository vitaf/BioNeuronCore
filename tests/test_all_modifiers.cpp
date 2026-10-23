// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// Проверка всех 80 типов модификаторов: каждый должен возвращать конечный ток (не NaN, не inf).
#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include "types.h"
#include "kernel_dispatch.h"
#include <cstring>
#include <cmath>

extern ModifierFunc biological_library[MOD_TYPE_COUNT];
extern ReleaseModulatorFunc biological_modulators[MOD_TYPE_COUNT];
extern void init_biological_library();

static Segment make_seg() {
    Segment s = {};
    s.id = 1;
    s.length = 100.0f;
    s.radius = 10.0f;
    s.V_m = -65.0f;
    s.intra_ions[ION_NA] = 10.0f; s.intra_ions[ION_K] = 140.0f;
    s.intra_ions[ION_CA] = 0.0001f; s.intra_ions[ION_CL] = 10.0f;
    s.intra_ions[ION_MG] = 0.5f;
    s.extra_ions[ION_NA] = 145.0f; s.extra_ions[ION_K] = 4.0f;
    s.extra_ions[ION_CA] = 2.0f; s.extra_ions[ION_CL] = 110.0f;
    s.extra_ions[ION_MG] = 1.0f;
    s.ATP_level = 3.0f;
    s.temperature = 37.0f;
    for (int i = 0; i < NT_COUNT; i++) s.neurotransmitter[i] = 0.1f;
    return s;
}

static void expect_valid(int type_id, Segment* s) {
    auto fn = biological_library[type_id];
    REQUIRE(fn != nullptr);

    Modifier m = {};
    m.type = (uint8_t)type_id;
    m.density = 1.0f;
    m.state[0] = 0.1f;
    m.state[1] = 0.1f;

    float I = fn(&m, s, 0.1f);
    CHECK_FALSE(std::isnan(I));
    CHECK_FALSE(std::isinf(I));
}

#define TEST_TYPE(id, name) TEST_CASE("AllModifiers: " #name " (" #id ")", "[all_modifiers]") { init_biological_library(); Segment s = make_seg(); expect_valid(id, &s); }

// Базовые (0-7)
TEST_TYPE(142,  AXON_NAV12)
TEST_TYPE(1,  IONOTROPIC_CA)
TEST_TYPE(2,  IONOTROPIC_CL)
TEST_TYPE(3,  METABOTROPIC)
TEST_TYPE(4,  MYELIN)
TEST_TYPE(5,  SYNAPTIC_RELEASE)
TEST_TYPE(6,  PHYSICAL_SENSOR)
TEST_TYPE(7,  PUMP_NAK)

// Ионотропные рецепторы (8-16)
TEST_TYPE(8,  AMPA)
TEST_TYPE(9,  NMDA)
TEST_TYPE(10, KAINATE)
TEST_TYPE(11, GABAA)
TEST_TYPE(12, GLYCINE)
TEST_TYPE(13, NICOTINIC)
TEST_TYPE(14, 5HT3)
TEST_TYPE(15, P2X)
TEST_TYPE(16, TRPV1)

// Метаботропные Gi/o (17-40)
TEST_TYPE(17, GABAB)
TEST_TYPE(18, 5HT1A)
TEST_TYPE(19, 5HT1B)
TEST_TYPE(20, 5HT1D)
TEST_TYPE(21, 5HT1F)
TEST_TYPE(22, D2)
TEST_TYPE(23, D3)
TEST_TYPE(24, D4)
TEST_TYPE(25, M2)
TEST_TYPE(26, M4)
TEST_TYPE(27, A1)
TEST_TYPE(28, A3)
TEST_TYPE(29, mGluR2)
TEST_TYPE(30, mGluR3)
TEST_TYPE(31, mGluR4)
TEST_TYPE(32, mGluR6)
TEST_TYPE(33, mGluR7)
TEST_TYPE(34, mGluR8)
TEST_TYPE(35, ALPHA2)
TEST_TYPE(36, CB1)
TEST_TYPE(37, CB2)
TEST_TYPE(38, H3)
TEST_TYPE(39, MT1)
TEST_TYPE(40, MT2)

// Метаботропные Gs (41-51)
TEST_TYPE(41, 5HT4)
TEST_TYPE(42, 5HT6)
TEST_TYPE(43, 5HT7)
TEST_TYPE(44, D1)
TEST_TYPE(45, D5)
TEST_TYPE(46, A2A)
TEST_TYPE(47, A2B)
TEST_TYPE(48, BETA1)
TEST_TYPE(49, BETA2)
TEST_TYPE(50, H2)
TEST_TYPE(51, TAAR1)

// Метаботропные Gq/11 (52-62)
TEST_TYPE(52, 5HT2A)
TEST_TYPE(53, 5HT2B)
TEST_TYPE(54, 5HT2C)
TEST_TYPE(55, ALPHA1A)
TEST_TYPE(56, ALPHA1B)
TEST_TYPE(57, M1)
TEST_TYPE(58, M3)
TEST_TYPE(59, M5)
TEST_TYPE(60, mGluR1)
TEST_TYPE(61, mGluR5)
TEST_TYPE(62, H1)

// NB_-рецепторы (63-79)
TEST_TYPE(63, NB_NMDA_MORADI_FULL)
TEST_TYPE(64, NB_AMPA_STA)
TEST_TYPE(65, NB_D1_MODULATOR)
TEST_TYPE(66, NB_D2_MODULATOR)
TEST_TYPE(67, NB_ALPHA7_ER_NACHR)
TEST_TYPE(68, NB_BETA2_NACHR)
TEST_TYPE(69, NB_CA_TRPM4R)
TEST_TYPE(70, NB_MUSCARINIC_R)
TEST_TYPE(71, NB_AMPA_DESTEXHE)
TEST_TYPE(72, NB_NMDA_DESTEXHE)
TEST_TYPE(73, NB_GABAA_DESTEXHE)
TEST_TYPE(74, NB_GABAB_DESTEXHE)
TEST_TYPE(75, NB_MSN_NMDAR)
TEST_TYPE(76, NB_MSN_AMPAR)
TEST_TYPE(77, NB_SOMA_ATP)
TEST_TYPE(78, NB_SOMA_THERMAL)
TEST_TYPE(79, NB_CREB_TRANSCRIPTION)

// ═══════════════════════════════════════════════════════════════
// Пресинаптические модуляторы
// ═══════════════════════════════════════════════════════════════
TEST_CASE("AllModifiers: presynaptic modulators", "[all_modulators]") {
    init_biological_library();

    int mod_types[] = {
        MOD_NT_RECEPTOR_5HT1A, MOD_NT_RECEPTOR_5HT1B,
        MOD_NT_RECEPTOR_D2,
        MOD_NT_RECEPTOR_A1, MOD_NT_RECEPTOR_A2A,
        MOD_NT_RECEPTOR_D1,
        MOD_NT_RECEPTOR_MGLUR2, MOD_NT_RECEPTOR_MGLUR3,
        MOD_NT_RECEPTOR_CB1, MOD_NT_RECEPTOR_H3,
        MOD_NT_RECEPTOR_NICOTINIC, MOD_NT_RECEPTOR_ALPHA2
    };
    int num = sizeof(mod_types) / sizeof(mod_types[0]);

    float cleft_nt[NT_COUNT] = {};
    cleft_nt[NT_DOPAMINE] = 0.5f;
    cleft_nt[NT_SEROTONIN] = 0.5f;
    cleft_nt[NT_ACETYLCHOLINE] = 0.5f;
    cleft_nt[NT_NORADRENALINE] = 0.5f;

    for (int i = 0; i < num; i++) {
        auto fn = biological_modulators[mod_types[i]];
        REQUIRE(fn != nullptr);

        Modifier m = {};
        m.type = (uint8_t)mod_types[i];
        m.density = 1.0f;
        m.state[0] = 0.1f;

        float result = fn(&m, cleft_nt, 0.1f);
        CHECK_FALSE(std::isnan(result));
        CHECK(result >= 0.0f);
    }
}
