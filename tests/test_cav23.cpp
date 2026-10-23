// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_cav23.cpp — верификация MOD_CAV23 (ID 158) против Cav2.3 (CACNA1E, R-type).
// Cav2.3: γ=14 пС, m² HVA (V½ −25), медленная частичная инактивация, слабая CDI, SNX-482, GHK Ca²⁺.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double cav23_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_CA] = 0.0001f; s.extra_ions[ION_CA] = 2.0f;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_CAV23;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

static float I_open(float V) {
    Segment s = make_seg(V);
    s.modifiers[0].state[0] = 1.0f;
    s.modifiers[0].state[1] = 1.0f;
    s.modifiers[0].state[2] = 1.0f;  // h_slow открыт → h_eff=1
    return cav23_current(&s.modifiers[0], &s, 0.0f);
}

TEST_CASE("Cav2.3: активация m² (HVA, V½=−25)", "[cav23]") {
    Segment s = make_seg(20.0f);
    s.modifiers[0].state[0] = 0.0f;
    for (int t = 0; t < 300; t++) cav23_current(&s.modifiers[0], &s, 1.0f);
    float m_inf = 1.0f / (1.0f + expf(-(20.0f + 25.0f) / 6.0f));
    REQUIRE(s.modifiers[0].state[0] == Approx(m_inf).margin(0.02f));
}

TEST_CASE("Cav2.3: E_rev — Ca²⁺ входит при V < E_Ca (положительный ток)", "[cav23]") {
    REQUIRE(I_open(-40.0f) > 0.0f);   // Ca входит → деполяризация
    REQUIRE(I_open(150.0f) < 0.0f);   // V > E_Ca → Ca выходит
}

TEST_CASE("Cav2.3: слабая CDI — высокий Ca²⁺ снижает ток", "[cav23]") {
    Segment s_low = make_seg(0.0f);
    s_low.modifiers[0].state[0] = 1.0f; s_low.modifiers[0].state[1] = 1.0f;
    float I_low = cav23_current(&s_low.modifiers[0], &s_low, 0.0f);
    Segment s_high = make_seg(0.0f);
    s_high.modifiers[0].state[0] = 1.0f; s_high.modifiers[0].state[1] = 1.0f;
    s_high.intra_ions[ION_CA] = 0.01f;  // 10 µM → CaM
    float I_high = cav23_current(&s_high.modifiers[0], &s_high, 0.0f);
    REQUIRE(fabsf(I_high) < fabsf(I_low));
}

TEST_CASE("Cav2.3: SNX-482 сдвиг V½ +70 мВ → меньше m", "[cav23]") {
    Segment s = make_seg(-25.0f);
    s.modifiers[0].state[1] = 1.0f;
    for (int t = 0; t < 50; t++) cav23_current(&s.modifiers[0], &s, 1.0f);
    float m_no = s.modifiers[0].state[0];  // m_inf ≈ 0.5 при V=−25

    Segment s2 = make_seg(-25.0f);
    s2.modifiers[0].state[1] = 1.0f;
    s2.modifiers[0].state[5] = 300.0f;  // SNX-482 → V½_eff ≈ +38 мВ
    for (int t = 0; t < 50; t++) cav23_current(&s2.modifiers[0], &s2, 1.0f);
    float m_snx = s2.modifiers[0].state[0];  // m_inf ≈ 0 (сдвиг)
    REQUIRE(m_snx < m_no * 0.3f);
}

TEST_CASE("Cav2.3: Ca²⁺ консервация", "[cav23]") {
    Segment s = make_seg(0.0f);
    s.modifiers[0].state[0] = 1.0f; s.modifiers[0].state[1] = 1.0f;
    float ca_before = s.intra_ions[ION_CA] + s.extra_ions[ION_CA] * EXTRA_VOLUME_RATIO;
    cav23_current(&s.modifiers[0], &s, 0.1f);
    float ca_after = s.intra_ions[ION_CA] + s.extra_ions[ION_CA] * EXTRA_VOLUME_RATIO;
    REQUIRE(ca_after == Approx(ca_before).margin(0.01f));
}

TEST_CASE("Cav2.3: двухкомпонентная инактивация (fast 25 + slow 100)", "[cav23]") {
    // V=−80 → h_inf ≈ 1; оба гейта стартуют с 0, fast (τ=25) сходится быстрее slow (τ=100).
    Segment s = make_seg(-80.0f);
    s.modifiers[0].state[1] = 0.0f;
    s.modifiers[0].state[2] = 0.0f;
    for (int t = 0; t < 500; t++) cav23_current(&s.modifiers[0], &s, 0.1f);  // 50 мс
    REQUIRE(s.modifiers[0].state[1] > s.modifiers[0].state[2]);  // fast сошёлся дальше
}
