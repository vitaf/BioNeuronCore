// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_nav12dev.cpp — верификация NAV12DEV (193) против Nav1.2 developmental (Scn2a).
// Эталон: тадпольный аксональный Na⁺ — активация V½=−23 мВ (JGP 2022), инактивация V½=−35 мВ
// (developmental, Ribera & Spitzer) → доступен на покое головастика −51 мВ (h_inf=0.93). γ≈20 пС.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double nav12dev_current(Modifier* m, Segment* s, float dt);

TEST_CASE("NAV12DEV: Na m_inf — активация (V½=−23, как Nav1.2)", "[nav12dev]") {
    REQUIRE(nav12_m_inf(-60.0f) == Approx(0.003f).margin(0.002f)); // покой — закрыт
    REQUIRE(nav12_m_inf(-23.0f) == Approx(0.595f).margin(0.05f));  // V½ активации
    REQUIRE(nav12_m_inf(-5.0f)  == Approx(0.967f).margin(0.03f));  // открыт
}

TEST_CASE("NAV12DEV: Na h_inf — деполяризованная инактивация (V½=−35)", "[nav12dev]") {
    REQUIRE(nav12dev_h_inf(-35.0f) == Approx(0.50f).margin(0.02f)); // V½ инактивации
    REQUIRE(nav12dev_h_inf(-80.0f) == Approx(1.0f).margin(0.005f)); // полностью доступен
    REQUIRE(nav12dev_h_inf(-20.0f) == Approx(0.082f).margin(0.01f)); // инактивирован при деполяр.
}

TEST_CASE("NAV12DEV: ключевое свойство — доступен на покое головастика (−51 мВ)", "[nav12dev]") {
    // h_inf(−51) ≈ 0.93 (в отличие от Nav1.2: h_inf(−51)=0.117 — закрыт на −51)
    REQUIRE(nav12dev_h_inf(-51.0f) == Approx(0.93f).margin(0.02f));
    REQUIRE(nav12dev_h_inf(-51.0f) > nav12_h_inf(-51.0f)); // доступнее млекопитающего Nav1.2
}

TEST_CASE("NAV12DEV: γ ≈ 20 пС (single-channel)", "[nav12dev]") {
    REQUIRE(NAV12DEV_GAMMA_PS == Approx(20.8f).epsilon(1e-3));
}

TEST_CASE("NAV12DEV: релаксация m к m_inf за τ (экспоненциальный Эйлер)", "[nav12dev]") {
    Segment s = {};
    s.V_m = -60.0f;
    s.intra_ions[ION_NA] = 15.0f;  s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K]  = 5.0f;
    s.radius = 1.0f; s.length = 2.0f; s.segment_type = 2;
    s.modifiers[0].type = MOD_NAV12DEV;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;

    float m_inf_rest = nav12_m_inf(-60.0f);
    s.modifiers[0].state[0] = 1.0f;
    for (int t = 0; t < 200; t++)
        nav12dev_current(&s.modifiers[0], &s, 0.01f);
    REQUIRE(s.modifiers[0].state[0] == Approx(m_inf_rest).margin(0.02f));
}

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 5.0f;
    s.radius = 1.0f; s.length = 2.0f; s.segment_type = 2;
    s.modifiers[0].type = MOD_NAV12DEV;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

static float I_open(float V) {
    Segment s = make_seg(V);
    s.modifiers[0].state[0] = 1.0f;
    s.modifiers[0].state[1] = 1.0f;
    return nav12dev_current(&s.modifiers[0], &s, 0.0f);
}

TEST_CASE("NAV12DEV: E_rev — Na входит при V < E_Na", "[nav12dev]") {
    REQUIRE(I_open(-20.0f) > 0.0f);
    REQUIRE(I_open(100.0f) < 0.0f);
}

TEST_CASE("NAV12DEV: TTX-блок (IC50 ~10 нМ)", "[nav12dev]") {
    Segment s = make_seg(-20.0f);
    s.modifiers[0].state[0] = 1.0f; s.modifiers[0].state[1] = 1.0f;
    float I_no = nav12dev_current(&s.modifiers[0], &s, 0.0f);
    s.modifiers[0].state[5] = 100.0f;
    float I_ttx = nav12dev_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(fabsf(I_ttx) < fabsf(I_no) * 0.2f);
}

TEST_CASE("NAV12DEV: PKA/PKC ингибиция (сильная — Ser573)", "[nav12dev]") {
    Segment s = make_seg(-20.0f);
    s.modifiers[0].state[0] = 1.0f; s.modifiers[0].state[1] = 1.0f;
    float I_no = nav12dev_current(&s.modifiers[0], &s, 0.0f);
    s.modifiers[1].type = MOD_PKC; s.modifiers[1].state[0] = 1.0f;
    s.modifiers[2].type = MOD_CAMP_PKA; s.modifiers[2].state[1] = 1.0f;
    s.modifier_count = 3;
    float I_pkc = nav12dev_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(fabsf(I_pkc) < fabsf(I_no) * 0.5f);
}

TEST_CASE("NAV12DEV: Na⁺ консервация (intra+extra)", "[nav12dev]") {
    Segment s = make_seg(-20.0f);
    s.modifiers[0].state[0] = 1.0f; s.modifiers[0].state[1] = 1.0f;
    float na_before = s.intra_ions[ION_NA] + s.extra_ions[ION_NA] * EXTRA_VOLUME_RATIO;
    nav12dev_current(&s.modifiers[0], &s, 0.1f);
    float na_after = s.intra_ions[ION_NA] + s.extra_ions[ION_NA] * EXTRA_VOLUME_RATIO;
    REQUIRE(na_after == Approx(na_before).margin(0.01f));
}
