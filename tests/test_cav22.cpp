// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_cav22.cpp — верификация MOD_CAV22 (ID 168) против Cav2.2 (CACNA1B, N-type).
// Cav2.2: γ=2.76 пС, m² HVA (V½ −20), медленная β-зависимая h, ω-GVIA, GHK Ca²⁺.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double cav22_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_CA] = 0.0001f; s.extra_ions[ION_CA] = 2.0f;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_CAV22;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

static float I_open(float V) {
    Segment s = make_seg(V);
    s.modifiers[0].state[0] = 1.0f;
    s.modifiers[0].state[1] = 1.0f;
    return cav22_current(&s.modifiers[0], &s, 0.0f);
}

TEST_CASE("Cav2.2: активация m² (HVA, V½=−20)", "[cav22]") {
    Segment s = make_seg(20.0f);
    s.modifiers[0].state[0] = 0.0f;
    for (int t = 0; t < 300; t++) cav22_current(&s.modifiers[0], &s, 1.0f);
    float m_inf = 1.0f / (1.0f + expf(-(20.0f + 20.0f) / 6.0f)); // ≈0.998
    REQUIRE(s.modifiers[0].state[0] == Approx(m_inf).margin(0.02f));
}

TEST_CASE("Cav2.2: инактивация h при деполяризации → 0", "[cav22]") {
    Segment s = make_seg(40.0f);
    s.modifiers[0].state[1] = 1.0f;
    for (int t = 0; t < 3000; t++) cav22_current(&s.modifiers[0], &s, 1.0f);
    float h_inf = 1.0f / (1.0f + expf((40.0f + 60.0f) / 6.0f)); // ≈0
    REQUIRE(s.modifiers[0].state[1] == Approx(h_inf).margin(0.02f));
}

TEST_CASE("Cav2.2: E_rev — Ca²⁺ входит при V < E_Ca (положительный ток)", "[cav22]") {
    REQUIRE(I_open(-40.0f) > 0.0f);   // Ca входит → деполяризация
    REQUIRE(I_open(150.0f) < 0.0f);   // V > E_Ca → Ca выходит
}

TEST_CASE("Cav2.2: ω-GVIA блок (state[5]=5 нМ = 10×IC50)", "[cav22]") {
    Segment s_ctrl = make_seg(-20.0f);
    s_ctrl.modifiers[0].state[0] = 1.0f; s_ctrl.modifiers[0].state[1] = 1.0f;
    float I_ctrl = fabsf(cav22_current(&s_ctrl.modifiers[0], &s_ctrl, 0.0f));

    Segment s_gvia = make_seg(-20.0f);
    s_gvia.modifiers[0].state[0] = 1.0f; s_gvia.modifiers[0].state[1] = 1.0f;
    s_gvia.modifiers[0].state[5] = 5.0f;  // 5 нМ = 10×IC50 (0.5 нМ)
    float I_gvia = fabsf(cav22_current(&s_gvia.modifiers[0], &s_gvia, 0.0f));

    // open_prob /= (1 + gvia/IC50) → 1/11
    REQUIRE(I_gvia == Approx(I_ctrl / 11.0f).margin(I_ctrl * 0.01f + 1e-6f));
}

TEST_CASE("Cav2.2: Ca²⁺ консервация", "[cav22]") {
    Segment s = make_seg(0.0f);
    s.modifiers[0].state[0] = 1.0f; s.modifiers[0].state[1] = 1.0f;
    float ca_before = s.intra_ions[ION_CA] + s.extra_ions[ION_CA] * EXTRA_VOLUME_RATIO;
    cav22_current(&s.modifiers[0], &s, 0.1f);
    float ca_after = s.intra_ions[ION_CA] + s.extra_ions[ION_CA] * EXTRA_VOLUME_RATIO;
    REQUIRE(ca_after == Approx(ca_before).margin(0.01f));
}
