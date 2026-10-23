// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_katp.cpp — верификация MOD_KATP (ID 161) против KATP (Kir6.2/SUR1).
// ATP-зависимость (IC50 15 µМ), E_rev (K⁺), глибенкламид-блок (IC50 3 нМ), K⁺ консервация.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double katp_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_KATP;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[4] = 0.0f;   // [ATP] = 0 → Po≈1
    s.modifier_count = 1;
    return s;
}

static float I_open(float V) {
    Segment s = make_seg(V);
    s.modifiers[0].state[4] = 0.0f;   // Po = 1
    s.modifiers[0].state[5] = 0.0f;   // без глибенкламида
    return katp_current(&s.modifiers[0], &s, 0.0f);
}

TEST_CASE("KATP: ATP-зависимость (state[4]=0 → Po≈1; 150 µМ → Po≈0.09)", "[katp]") {
    Segment s0 = make_seg(40.0f);
    float I0 = katp_current(&s0.modifiers[0], &s0, 0.0f);

    Segment s150 = make_seg(40.0f);
    s150.modifiers[0].state[4] = 150.0f;  // 150 µМ = 10×IC50 → Po≈1/11
    float I150 = katp_current(&s150.modifiers[0], &s150, 0.0f);

    INFO("I(0)=" << I0 << " I(150)=" << I150);
    REQUIRE(fabsf(I0) > 0.0f);
    REQUIRE(fabsf(I150) == Approx(fabsf(I0) * (1.0f / 11.0f)).margin(fabsf(I0) * 0.01f));
}

TEST_CASE("KATP: E_rev — знак меняется у E_K", "[katp]") {
    REQUIRE(I_open(-40.0f) < 0.0f);    // V > E_K → K⁺ выходит → I_K>0 → return<0
    REQUIRE(I_open(-110.0f) > 0.0f);   // V < E_K → K⁺ входит → I_K<0 → return>0
}

TEST_CASE("KATP: глибенкламид блок (state[5]=30 нМ = 10×IC50)", "[katp]") {
    Segment s = make_seg(40.0f);
    float I_no = katp_current(&s.modifiers[0], &s, 0.0f);

    s.modifiers[0].state[5] = 30.0f;   // 30 нМ = 10 × IC50(3 нМ) → Po/11
    float I_glib = katp_current(&s.modifiers[0], &s, 0.0f);

    INFO("I(no)=" << I_no << " I(glib)=" << I_glib);
    REQUIRE(fabsf(I_glib) == Approx(fabsf(I_no) / 11.0f).margin(fabsf(I_no) * 0.01f));
}

TEST_CASE("KATP: K⁺ консервация", "[katp]") {
    Segment s = make_seg(40.0f);
    float k_before = s.intra_ions[ION_K] + s.extra_ions[ION_K] * EXTRA_VOLUME_RATIO;
    katp_current(&s.modifiers[0], &s, 0.1f);
    float k_after = s.intra_ions[ION_K] + s.extra_ions[ION_K] * EXTRA_VOLUME_RATIO;
    REQUIRE(k_after == Approx(k_before).margin(0.01f));
}
