// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. All rights reserved.

// test_pir_ttype.cpp — верификация PIR_TTYPE (mod 96) против Cav3.2 T-type.
// Cav3.2: γ=7.2 пС, активация V½=−48.4 мВ, инактивация V½=−75.6 мВ.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double pir_ttype_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_CA] = 0.0001f; s.extra_ions[ION_CA] = 1.2f;  // мМ
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K]  = 4.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_PIR_TTYPE_CALCIUM;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[2] = 7.2f;  // γ (пС)
    s.modifier_count = 1;
    return s;
}

TEST_CASE("PIR_Ttype: гейтинг V½ (Cav3.2: m −48.4, h −75.6)", "[pir_ttype]") {
    REQUIRE(tca_m_inf(-48.4f) == Approx(0.5f).epsilon(1e-3));
    REQUIRE(tca_h_inf(-75.6f) == Approx(0.5f).epsilon(1e-3));
}

TEST_CASE("PIR_Ttype: γ не меняется (state[2])", "[pir_ttype]") {
    Segment s = make_seg(-60.0f);
    for (int t = 0; t < 200; t++) {
        s.V_m = (t % 2 == 0) ? 0.0f : -80.0f;
        pir_ttype_current(&s.modifiers[0], &s, 0.01f);
    }
    REQUIRE(s.modifiers[0].state[2] == Approx(7.2f).epsilon(1e-4));
}

TEST_CASE("PIR_Ttype: динамика — m активируется деполяризацией (m³h)", "[pir_ttype]") {
    Segment s = make_seg(0.0f);  // деполяризация → m_inf ≈ 1, h_inf ≈ 0
    s.modifiers[0].state[0] = 0.0f;
    for (int t = 0; t < 300; t++) pir_ttype_current(&s.modifiers[0], &s, 0.1f);
    float m_inf = tca_m_inf(0.0f);
    REQUIRE(s.modifiers[0].state[0] == Approx(m_inf).margin(0.02f));
}
