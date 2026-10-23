// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. All rights reserved.

// test_ca_l_type.cpp — верификация CA_L_TYPE (mod 84) против Cav1.2.
// Cav1.2: γ≈25 пС, активация m² (V½=−11 мВ), инактивация V½=−37.3 мВ, CDI.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double ca_l_type_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V, float Ca_i) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_CA] = Ca_i; s.extra_ions[ION_CA] = 1.2f;  // мМ
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K]  = 4.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_CA_L_TYPE;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[2] = 25.0f;  // γ (пС)
    s.modifier_count = 1;
    return s;
}

TEST_CASE("Ca_L_type: γ не меняется (state[2])", "[ca_l_type]") {
    Segment s = make_seg(-60.0f, 0.0001f);
    for (int t = 0; t < 200; t++) {
        s.V_m = (t % 2 == 0) ? 20.0f : -60.0f;
        ca_l_type_current(&s.modifiers[0], &s, 0.01f);
    }
    REQUIRE(s.modifiers[0].state[2] == Approx(25.0f).epsilon(1e-4));  // γ
}

TEST_CASE("Ca_L_type: динамика — m-гейт активируется деполяризацией (V½=−11)", "[ca_l_type]") {
    // При V=+20 (деполяризация) m_inf ≈ 0.989, m релаксирует к нему.
    Segment s = make_seg(20.0f, 0.0001f);
    s.modifiers[0].state[0] = 0.0f;  // m=0 старт
    for (int t = 0; t < 300; t++) ca_l_type_current(&s.modifiers[0], &s, 0.1f);  // 30 мс = 10·tau_m
    float m_inf = 1.0f / (1.0f + expf(-(20.0f + 11.0f) / 6.9f));
    INFO("m=" << s.modifiers[0].state[0] << " m_inf=" << m_inf);
    REQUIRE(s.modifiers[0].state[0] == Approx(m_inf).margin(0.02f));
}

TEST_CASE("Ca_L_type: h_VDI — инактивация при деполяризации (V½=−37.3)", "[ca_l_type]") {
    // Гиперполяризация (−80) → h_VDI близко к 1; деполяризация (−10) → h_VDI → 0 (только VDI, без CDI).
    Segment s_hyper = make_seg(-80.0f, 0.0001f);
    Segment s_depol = make_seg(-10.0f, 0.0001f);
    for (int t = 0; t < 15000; t++) {  // 1500 мс = 10·τ_VDI (150 мс)
        ca_l_type_current(&s_hyper.modifiers[0], &s_hyper, 0.1f);
        ca_l_type_current(&s_depol.modifiers[0], &s_depol, 0.1f);
    }
    float vdi_hyper = 1.0f / (1.0f + expf((-80.0f + 37.3f) / 8.0f));
    float vdi_depol = 1.0f / (1.0f + expf((-10.0f + 37.3f) / 8.0f));
    INFO("h_VDI_hyper=" << s_hyper.modifiers[0].state[1] << " exp=" << vdi_hyper
         << " h_VDI_depol=" << s_depol.modifiers[0].state[1] << " exp=" << vdi_depol);
    REQUIRE(s_hyper.modifiers[0].state[1] == Approx(vdi_hyper).margin(0.03f));
    REQUIRE(s_depol.modifiers[0].state[1] == Approx(vdi_depol).margin(0.03f));
    REQUIRE(s_depol.modifiers[0].state[1] < s_hyper.modifiers[0].state[1]);
}

TEST_CASE("Ca_L_type: h_CDI — Ca²⁺-зависимая инактивация (state[5])", "[ca_l_type]") {
    // V=−80 (m закрыт → нет Ca-тока → Ca фиксирован), проверяем только CDI-гейт.
    Segment s_low = make_seg(-80.0f, 0.0001f);   // 0.1 µM
    Segment s_high = make_seg(-80.0f, 0.001f);   // 1 µM
    for (int t = 0; t < 5000; t++) {  // 500 мс = ~14·τ_CDI (35 мс)
        ca_l_type_current(&s_low.modifiers[0], &s_low, 0.1f);
        ca_l_type_current(&s_high.modifiers[0], &s_high, 0.1f);
    }
    float cdi_low  = 1.0f / (1.0f + (0.0001f * MM_TO_UM) / 1.0f);
    float cdi_high = 1.0f / (1.0f + (0.001f * MM_TO_UM) / 1.0f);
    INFO("h_CDI_low=" << s_low.modifiers[0].state[5] << " exp=" << cdi_low
         << " h_CDI_high=" << s_high.modifiers[0].state[5] << " exp=" << cdi_high);
    REQUIRE(s_low.modifiers[0].state[5] == Approx(cdi_low).margin(0.03f));
    REQUIRE(s_high.modifiers[0].state[5] == Approx(cdi_high).margin(0.03f));
    REQUIRE(s_high.modifiers[0].state[5] < s_low.modifiers[0].state[5]);
}
