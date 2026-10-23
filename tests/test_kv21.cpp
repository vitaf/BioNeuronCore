// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_kv21.cpp — верификация MOD_KV21 (Kv2.1/KCNB1) — медленный delayed rectifier (I_Ks).
// Kv2.1: V½_акт=0 (k 9.4), m⁴, U-type инактивация, TEA IC50=10 мМ, γ=8 пС, GHK K⁺.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double kv21_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_KV21;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

TEST_CASE("Kv2.1: активация m⁴ (V½=0)", "[kv21]") {
    Segment s = make_seg(40.0f);
    s.modifiers[0].state[0] = 0.0f;
    for (int t = 0; t < 5000; t++) kv21_current(&s.modifiers[0], &s, 0.1f);  // 500 мс
    float m_inf = kv21_m_inf(40.0f);
    REQUIRE(s.modifiers[0].state[0] == Approx(m_inf).margin(0.02f));
}

TEST_CASE("Kv2.1: U-type инактивация (min при ~0, восстановление при деполяр.)", "[kv21]") {
    float h_hyper = kv21_h_inf(-100.0f);
    float h_mid   = kv21_h_inf(0.0f);
    float h_depol = kv21_h_inf(80.0f);
    REQUIRE(h_mid < h_hyper);
    REQUIRE(h_mid < h_depol);
    REQUIRE(h_hyper == Approx(1.0f).margin(0.02f));
    REQUIRE(h_depol == Approx(1.0f).margin(0.02f));
    // V½_left=−26.5 → h ≈ 0.5 при −26.5
    REQUIRE(kv21_h_inf(-26.5f) == Approx(0.6f).margin(0.05f));
}

TEST_CASE("Kv2.1: τ_m V-зависимая (15 @+60, 72 @−40)", "[kv21]") {
    REQUIRE(kv21_m_tau(60.0f) == Approx(15.0f).margin(1.0f));
    REQUIRE(kv21_m_tau(-40.0f) == Approx(72.0f).margin(3.0f));
}

TEST_CASE("Kv2.1: TEA-блок (IC50=10 мМ)", "[kv21]") {
    Segment s = make_seg(40.0f);
    s.modifiers[0].state[0] = 1.0f; s.modifiers[0].state[1] = 1.0f;
    float I_no = kv21_current(&s.modifiers[0], &s, 0.0f);
    s.modifiers[0].state[5] = 10.0f;  // 10 мМ = IC50
    float I_tea = kv21_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(fabsf(I_tea) == Approx(fabsf(I_no) * 0.5f).margin(fabsf(I_no) * 0.05f));
}

TEST_CASE("Kv2.1: E_rev — K выходит при деполяризации", "[kv21]") {
    Segment s = make_seg(40.0f);
    s.modifiers[0].state[0] = 1.0f; s.modifiers[0].state[1] = 1.0f;
    REQUIRE(kv21_current(&s.modifiers[0], &s, 0.0f) < 0.0f);
}

TEST_CASE("Kv2.1: K⁺ консервация", "[kv21]") {
    Segment s = make_seg(40.0f);
    s.modifiers[0].state[0] = 1.0f; s.modifiers[0].state[1] = 1.0f;
    float k_before = s.intra_ions[ION_K] + s.extra_ions[ION_K] * EXTRA_VOLUME_RATIO;
    kv21_current(&s.modifiers[0], &s, 0.1f);
    float k_after = s.intra_ions[ION_K] + s.extra_ions[ION_K] * EXTRA_VOLUME_RATIO;
    REQUIRE(k_after == Approx(k_before).margin(0.01f));
}
