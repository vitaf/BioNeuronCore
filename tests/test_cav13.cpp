// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_cav13.cpp — верификация MOD_CAV13 (ID 1) против Cav1.3 (CACNA1D).
// Cav1.3: γ≈25 пС, активация V½=−35 мВ, инактивация V½=−52 мВ, слабая CDI (Kd=3 µM).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double ca_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V, float Ca_i) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_CA] = Ca_i; s.extra_ions[ION_CA] = 1.2f;  // мМ
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K]  = 4.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_CAV13;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[2] = 25.0f;  // γ (пС)
    s.modifier_count = 1;
    return s;
}

TEST_CASE("Cav1.3: γ не меняется (state[2])", "[cav13]") {
    Segment s = make_seg(-60.0f, 0.0001f);
    for (int t = 0; t < 200; t++) {
        s.V_m = (t % 2 == 0) ? 20.0f : -60.0f;
        ca_current(&s.modifiers[0], &s, 0.01f);
    }
    REQUIRE(s.modifiers[0].state[2] == Approx(25.0f).epsilon(1e-4));  // γ
}

TEST_CASE("Cav1.3: m-гейт активируется деполяризацией (V½=−35)", "[cav13]") {
    // При V=+20 (деполяризация) m_inf ≈ 0.9996, m релаксирует к нему.
    Segment s = make_seg(20.0f, 0.0001f);
    s.modifiers[0].state[0] = 0.0f;  // m=0 старт
    for (int t = 0; t < 300; t++) ca_current(&s.modifiers[0], &s, 0.1f);  // 30 мс = 15·tau_m
    float m_inf = 1.0f / (1.0f + expf(-(20.0f + 35.0f) / 7.0f));
    INFO("m=" << s.modifiers[0].state[0] << " m_inf=" << m_inf);
    REQUIRE(s.modifiers[0].state[0] == Approx(m_inf).margin(0.02f));
}

TEST_CASE("Cav1.3: h-гейт — инактивация при деполяризации (V½=−52)", "[cav13]") {
    // Гиперполяризация (−80) → h близко к 1; деполяризация (−20) → h → 0.
    Segment s_hyper = make_seg(-80.0f, 0.0001f);
    Segment s_depol = make_seg(-20.0f, 0.0001f);
    for (int t = 0; t < 5000; t++) {  // 500 мс = 5·tau_h → h релаксирует к h_inf
        ca_current(&s_hyper.modifiers[0], &s_hyper, 0.1f);
        ca_current(&s_depol.modifiers[0], &s_depol, 0.1f);
    }
    float cdi = 1.0f / (1.0f + (0.0001f * MM_TO_UM) / 3.0f);  // Ca=0.1 µM, Kd=3 µM
    float h_inf_hyper = cdi / (1.0f + expf((-80.0f + 52.0f) / 8.0f));
    float h_inf_depol = cdi / (1.0f + expf((-20.0f + 52.0f) / 8.0f));
    INFO("h_hyper=" << s_hyper.modifiers[0].state[1] << " exp=" << h_inf_hyper
         << " h_depol=" << s_depol.modifiers[0].state[1] << " exp=" << h_inf_depol);
    REQUIRE(s_hyper.modifiers[0].state[1] == Approx(h_inf_hyper).margin(0.03f));
    REQUIRE(s_depol.modifiers[0].state[1] == Approx(h_inf_depol).margin(0.03f));
    REQUIRE(s_depol.modifiers[0].state[1] < s_hyper.modifiers[0].state[1]);
}

TEST_CASE("Cav1.3: слабая CDI (Kd=3 µM)", "[cav13]") {
    // При Ca_i=3 µM (Kd) CDI уменьшает h_inf вдвое относительно низкого Ca_i.
    Segment s_low = make_seg(20.0f, 0.0001f);   // 100 нМ
    Segment s_high = make_seg(20.0f, 0.003f);   // 3 µM (Kd)
    for (int t = 0; t < 100; t++) {
        ca_current(&s_low.modifiers[0], &s_low, 0.1f);
        ca_current(&s_high.modifiers[0], &s_high, 0.1f);
    }
    REQUIRE(s_high.modifiers[0].state[1] < s_low.modifiers[0].state[1]);
}
