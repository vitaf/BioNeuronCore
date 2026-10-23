// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_axon_nav12.cpp — верификация NAV12 (142) против Nav1.2/SCN2A (JGP 2022).
// Эталон: аксональный Na⁺ головастика — активация V½≈−23 мВ, fast inactivation V½=−63.5 мВ, γ≈20 пС.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double nav12_current(Modifier* m, Segment* s, float dt);

TEST_CASE("NAV12: Na m_inf — активация (JGP 2022 α/β)", "[nav12]") {
    REQUIRE(nav12_m_inf(-60.0f) == Approx(0.003f).margin(0.002f)); // покой — закрыт (деполяризованнее Nav1.6)
    REQUIRE(nav12_m_inf(-23.0f) == Approx(0.595f).margin(0.05f)); // V½ активации
    REQUIRE(nav12_m_inf(-5.0f)  == Approx(0.967f).margin(0.03f)); // открыт
}

TEST_CASE("NAV12: Na h_inf — быстрая инактивация (V½=−63.5)", "[nav12]") {
    REQUIRE(nav12_h_inf(-63.5f) == Approx(0.50f).margin(0.02f)); // V½ инактивации
    REQUIRE(nav12_h_inf(-80.0f) == Approx(0.935f).margin(0.03f)); // доступен при гиперполяр.
    REQUIRE(nav12_h_inf(-62.0f) == Approx(0.44f).margin(0.03f)); // доступнее Nav1.6 (0.166)
}

TEST_CASE("NAV12: γ ≈ 20 пС (single-channel Nav1.2)", "[nav12]") {
    REQUIRE(NAV12_GAMMA_PS == Approx(20.8f).epsilon(1e-3));
}

TEST_CASE("NAV12 vs NAV16: Nav1.2 менее инактивирован в покое AIS (−62 мВ)", "[nav12]") {
    REQUIRE(nav12_h_inf(-62.0f) > nav16_h_inf(-62.0f)); // 0.44 > 0.166
}

TEST_CASE("NAV12: релаксация m к m_inf за τ (экспоненциальный Эйлер)", "[nav12]") {
    Segment s = {};
    s.V_m = -60.0f;
    s.intra_ions[ION_NA] = 15.0f;  s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K]  = 5.0f;
    s.radius = 1.0f; s.length = 2.0f; s.segment_type = 2; // аксон (цилиндр)
    s.modifiers[0].type = MOD_AXON_NAV12;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;

    float m_inf_rest = nav12_m_inf(-60.0f);
    s.modifiers[0].state[0] = 1.0f;             // старт с открытым m
    for (int t = 0; t < 200; t++)               // 200 × 0.01 мс = 2 мс
        nav12_current(&s.modifiers[0], &s, 0.01f);
    REQUIRE(s.modifiers[0].state[0] == Approx(m_inf_rest).margin(0.02f));
}

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 5.0f;
    s.radius = 1.0f; s.length = 2.0f; s.segment_type = 2;
    s.modifiers[0].type = MOD_AXON_NAV12;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

static float I_open(float V) {
    Segment s = make_seg(V);
    s.modifiers[0].state[0] = 1.0f;
    s.modifiers[0].state[1] = 1.0f;
    return nav12_current(&s.modifiers[0], &s, 0.0f);
}

TEST_CASE("NAV12: E_rev — Na входит при V < E_Na", "[nav12]") {
    REQUIRE(I_open(-20.0f) > 0.0f);
    REQUIRE(I_open(100.0f) < 0.0f);
}

TEST_CASE("NAV12: TTX-блок (IC50 ~10 нМ)", "[nav12]") {
    Segment s = make_seg(-20.0f);
    s.modifiers[0].state[0] = 1.0f; s.modifiers[0].state[1] = 1.0f;
    float I_no = nav12_current(&s.modifiers[0], &s, 0.0f);
    s.modifiers[0].state[5] = 100.0f;  // 10×IC50
    float I_ttx = nav12_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(fabsf(I_ttx) < fabsf(I_no) * 0.2f);
}

TEST_CASE("NAV12: PKA/PKC ингибиция (сильная — Ser573)", "[nav12]") {
    Segment s = make_seg(-20.0f);
    s.modifiers[0].state[0] = 1.0f; s.modifiers[0].state[1] = 1.0f;
    float I_no = nav12_current(&s.modifiers[0], &s, 0.0f);
    s.modifiers[1].type = MOD_PKC; s.modifiers[1].state[0] = 1.0f;
    s.modifiers[2].type = MOD_CAMP_PKA; s.modifiers[2].state[1] = 1.0f;
    s.modifier_count = 3;
    float I_pkc = nav12_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(fabsf(I_pkc) < fabsf(I_no) * 0.5f);  // сильная ингибиция (>50%)
}

TEST_CASE("NAV12: Na⁺ консервация (intra+extra)", "[nav12]") {
    Segment s = make_seg(-20.0f);
    s.modifiers[0].state[0] = 1.0f; s.modifiers[0].state[1] = 1.0f;
    float na_before = s.intra_ions[ION_NA] + s.extra_ions[ION_NA] * EXTRA_VOLUME_RATIO;
    nav12_current(&s.modifiers[0], &s, 0.1f);
    float na_after = s.intra_ions[ION_NA] + s.extra_ions[ION_NA] * EXTRA_VOLUME_RATIO;
    REQUIRE(na_after == Approx(na_before).margin(0.01f));
}
