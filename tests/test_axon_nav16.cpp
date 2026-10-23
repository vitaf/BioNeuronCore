// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_axon_nav16.cpp — верификация NAV16 (141) против Nav1.6/SCN8A (PNAS 2018).
// Эталон: аксональный Na⁺ — активация V½≈−43 мВ, fast inactivation V½=−72 мВ, γ=20.7 пС.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double nav16_current(Modifier* m, Segment* s, float dt);

TEST_CASE("NAV16: Na m_inf — активация (PNAS 2018 α/β)", "[nav16]") {
    REQUIRE(nav16_m_inf(-60.0f) == Approx(0.079f).margin(0.02f)); // покой — закрыт
    REQUIRE(nav16_m_inf(-43.0f) == Approx(0.595f).margin(0.05f)); // V½ активации
    REQUIRE(nav16_m_inf(-20.0f) == Approx(0.985f).margin(0.02f)); // открыт
}

TEST_CASE("NAV16: Na h_inf — быстрая инактивация (V½=−72)", "[nav16]") {
    REQUIRE(nav16_h_inf(-72.0f) == Approx(0.50f).margin(0.02f)); // V½ инактивации
    REQUIRE(nav16_h_inf(-80.0f) == Approx(0.784f).margin(0.03f)); // доступен при гиперполяр.
    REQUIRE(nav16_h_inf(-60.0f) == Approx(0.126f).margin(0.02f)); // частично инактивирован
}

TEST_CASE("NAV16: γ = 20.7 пС (single-channel Nav1.6)", "[nav16]") {
    REQUIRE(NAV16_GAMMA_PS == Approx(20.7f).epsilon(1e-3));
}

TEST_CASE("NAV16: релаксация m к m_inf за τ (экспоненциальный Эйлер)", "[nav16]") {
    Segment s = {};
    s.V_m = -60.0f;
    s.intra_ions[ION_NA] = 15.0f;  s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K]  = 5.0f;
    s.radius = 1.0f; s.length = 2.0f; s.segment_type = 2; // аксон (цилиндр)
    s.modifiers[0].type = MOD_AXON_NAV16;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;

    float m_inf_rest = nav16_m_inf(-60.0f);
    s.modifiers[0].state[0] = 1.0f;             // старт с открытым m
    for (int t = 0; t < 200; t++)               // 200 × 0.01 мс = 2 мс > 3τ
        nav16_current(&s.modifiers[0], &s, 0.01f);
    REQUIRE(s.modifiers[0].state[0] == Approx(m_inf_rest).margin(0.02f));
}

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 5.0f;
    s.radius = 1.0f; s.length = 2.0f; s.segment_type = 2;
    s.modifiers[0].type = MOD_AXON_NAV16;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

static float I_open(float V) {
    Segment s = make_seg(V);
    s.modifiers[0].state[0] = 1.0f;
    s.modifiers[0].state[1] = 1.0f;
    return nav16_current(&s.modifiers[0], &s, 0.0f);
}

TEST_CASE("NAV16: E_rev — Na входит при V < E_Na", "[nav16]") {
    REQUIRE(I_open(-20.0f) > 0.0f);
    REQUIRE(I_open(100.0f) < 0.0f);
}

TEST_CASE("NAV16: ВАХ — Na входит во всём диапазоне V < E_Na", "[nav16]") {
    float Vs[] = {-60, -40, -20, 0, 20};
    for (float V : Vs) REQUIRE(I_open(V) > 0.0f);
}

TEST_CASE("NAV16: TTX-блок (IC50 ~5 нМ)", "[nav16]") {
    Segment s = make_seg(-20.0f);
    s.modifiers[0].state[0] = 1.0f; s.modifiers[0].state[1] = 1.0f;
    float I_no = nav16_current(&s.modifiers[0], &s, 0.0f);
    s.modifiers[0].state[5] = 50.0f;  // 10×IC50
    float I_ttx = nav16_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(fabsf(I_ttx) < fabsf(I_no) * 0.2f);
}

TEST_CASE("NAV16: PKA/PKC ингибиция (слабая — нет Ser573)", "[nav16]") {
    Segment s = make_seg(-20.0f);
    s.modifiers[0].state[0] = 1.0f; s.modifiers[0].state[1] = 1.0f;
    float I_no = nav16_current(&s.modifiers[0], &s, 0.0f);
    s.modifiers[1].type = MOD_PKC; s.modifiers[1].state[0] = 1.0f;
    s.modifiers[2].type = MOD_CAMP_PKA; s.modifiers[2].state[1] = 1.0f;
    s.modifier_count = 3;
    float I_pkc = nav16_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(fabsf(I_pkc) < fabsf(I_no));
}

TEST_CASE("NAV16: Na⁺ консервация (intra+extra)", "[nav16]") {
    Segment s = make_seg(-20.0f);
    s.modifiers[0].state[0] = 1.0f; s.modifiers[0].state[1] = 1.0f;
    float na_before = s.intra_ions[ION_NA] + s.extra_ions[ION_NA] * EXTRA_VOLUME_RATIO;
    nav16_current(&s.modifiers[0], &s, 0.1f);
    float na_after = s.intra_ions[ION_NA] + s.extra_ions[ION_NA] * EXTRA_VOLUME_RATIO;
    REQUIRE(na_after == Approx(na_before).margin(0.01f));
}
