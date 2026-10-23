// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_cng.cpp — верификация MOD_CNG (ID 155) против CNG (CNGA2/A4/B1b).
// CNG: лиганд cAMP (state[4], Hill n=1.5, Kd=35 µM), Na⁺/Ca²⁺ (PCa/PNa≈3), γ=13 пС.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double cng_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V, float cAMP) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_CA] = 0.0001f; s.extra_ions[ION_CA] = 2.0f;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_CNG;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[4] = cAMP;  // лиганд (µM)
    s.modifier_count = 1;
    return s;
}

TEST_CASE("CNG: Po=Hill(cAMP) — закрыт при cAMP=0, открыт при cAMP≫Kd", "[cng]") {
    Segment s = make_seg(-60.0f, 0.0f);
    for (int t = 0; t < 20; t++) cng_current(&s.modifiers[0], &s, 1.0f);
    REQUIRE(s.modifiers[0].state[0] < 0.01f);  // cAMP=0 → закрыт

    Segment s2 = make_seg(-60.0f, 100.0f);
    for (int t = 0; t < 20; t++) cng_current(&s2.modifiers[0], &s2, 1.0f);
    float Po_inf = powf(100.0f, 1.5f) / (powf(35.0f, 1.5f) + powf(100.0f, 1.5f));
    REQUIRE(s2.modifiers[0].state[0] == Approx(Po_inf).margin(0.02f));
}

TEST_CASE("CNG: Na⁺ входит (деполяризация) при cAMP открытии", "[cng]") {
    Segment s = make_seg(-60.0f, 100.0f);
    s.modifiers[0].state[0] = 1.0f;
    float na_before = s.intra_ions[ION_NA];
    cng_current(&s.modifiers[0], &s, 0.1f);
    REQUIRE(s.intra_ions[ION_NA] > na_before);  // Na входит
}

TEST_CASE("CNG: Ca²⁺ входит (активирует CaCC)", "[cng]") {
    Segment s = make_seg(-60.0f, 100.0f);
    s.modifiers[0].state[0] = 1.0f;
    float ca_before = s.intra_ions[ION_CA];
    cng_current(&s.modifiers[0], &s, 0.1f);
    REQUIRE(s.intra_ions[ION_CA] > ca_before);  // Ca входит
}

TEST_CASE("CNG: E_rev — входящий (положительный) ток при V < E_rev", "[cng]") {
    Segment s = make_seg(-60.0f, 100.0f);
    s.modifiers[0].state[0] = 1.0f;
    float I = cng_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(I > 0.0f);  // Na/Ca входят → положительный (деполяризующий) ток
}

TEST_CASE("CNG: Ca²⁺/CaM ингибиция — высокий Ca²⁺ снижает ток", "[cng]") {
    Segment s_low = make_seg(-60.0f, 100.0f);
    s_low.modifiers[0].state[0] = 1.0f;
    float I_low = cng_current(&s_low.modifiers[0], &s_low, 0.0f);
    Segment s_high = make_seg(-60.0f, 100.0f);
    s_high.modifiers[0].state[0] = 1.0f;
    s_high.intra_ions[ION_CA] = 0.01f;  // 10 µM → CaM активна
    float I_high = cng_current(&s_high.modifiers[0], &s_high, 0.0f);
    REQUIRE(fabsf(I_high) < fabsf(I_low));  // Ca²⁺ ингибирует CNG
}

TEST_CASE("CNG: Na⁺ консервация (intra+extra сохраняется)", "[cng]") {
    Segment s = make_seg(-60.0f, 100.0f);
    s.modifiers[0].state[0] = 1.0f;
    float na_total_before = s.intra_ions[ION_NA] + s.extra_ions[ION_NA] * EXTRA_VOLUME_RATIO;
    cng_current(&s.modifiers[0], &s, 0.1f);
    float na_total_after = s.intra_ions[ION_NA] + s.extra_ions[ION_NA] * EXTRA_VOLUME_RATIO;
    REQUIRE(na_total_after == Approx(na_total_before).margin(0.01f));
}
