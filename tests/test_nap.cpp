// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_nap.cpp — верификация NaP (mod 105) против Nav1.6 persistent Na.
// Nav1.6: single-channel γ=20.7 пС, persistent-активация V½=−58.7 мВ, фракция 1.6%.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double nap_channel_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;  // мМ
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K]  = 4.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_NAP_CHANNEL;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[3] = 20.7f;   // γ (пС)
    s.modifiers[0].state[5] = 0.016f;  // Po (persistent-фракция)
    s.modifier_count = 1;
    return s;
}

TEST_CASE("NaP: m_inf активация (V½=−58.7, Nav1.6 persistent)", "[nap]") {
    REQUIRE(nap_m_inf(-58.7f) == Approx(0.5f).epsilon(1e-3));    // V½
    REQUIRE(nap_m_inf(-40.0f) == Approx(0.958f).margin(0.01f));  // активирован
    REQUIRE(nap_m_inf(-80.0f) == Approx(0.028f).margin(0.002f)); // закрыт
}

TEST_CASE("NaP: параметры — γ/Po не меняются (нет h-инактивации)", "[nap]") {
    Segment s = make_seg(-60.0f);
    for (int t = 0; t < 200; t++) {
        s.V_m = (t % 2 == 0) ? -40.0f : -80.0f;
        nap_channel_current(&s.modifiers[0], &s, 0.01f);
    }
    REQUIRE(s.modifiers[0].state[3] == Approx(20.7f).epsilon(1e-4));   // γ
    REQUIRE(s.modifiers[0].state[5] == Approx(0.016f).epsilon(1e-4));  // Po
}

TEST_CASE("NaP: динамика — m-гейт релаксирует к m_inf (tau=5 мс)", "[nap]") {
    Segment s = make_seg(-40.0f);  // деполяризация → m_inf ≈ 0.958
    s.modifiers[0].state[0] = 0.0f;  // m=0 старт
    for (int t = 0; t < 1000; t++) {  // 1000 × 0.05 мс = 50 мс >> 5·tau
        nap_channel_current(&s.modifiers[0], &s, 0.05f);
    }
    REQUIRE(s.modifiers[0].state[0] == Approx(nap_m_inf(-40.0f)).margin(0.02f));  // m → m_inf
}

TEST_CASE("NaP: I-V — Na входит при деполяризации (persistent)", "[nap]") {
    Segment sA = make_seg(-80.0f);
    Segment sB = make_seg(-50.0f);
    // persistent Na: при деполяризации (ближе к порогу) активация растёт → Na входит сильнее.
    float IA = nap_channel_current(&sA.modifiers[0], &sA, 0.01f);
    float IB = nap_channel_current(&sB.modifiers[0], &sB, 0.01f);
    // return = −I_Na; I_Na (inward) отрицательный → return положительный (деполяризация).
    REQUIRE(IB > 0.0f);   // деполяризующий ток при активации
    REQUIRE(IB > IA);     // при деполяризации активация выше → ток больше
}
