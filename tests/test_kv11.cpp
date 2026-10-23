// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_kv11.cpp — верификация MOD_KV11 (ID 156) против Kv1.1 (KCNA1, delayed-rectifier).
// Kv1.1: γ=9–14 пС, m⁴ (V½ −30), C-type медленная инактивация (τ~3 с), α-DTX/TEA, GHK K⁺.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double kv11_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_KV11;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

static float I_open(float V) {
    Segment s = make_seg(V);
    s.modifiers[0].state[0] = 1.0f;
    s.modifiers[0].state[1] = 1.0f;
    return kv11_current(&s.modifiers[0], &s, 0.0f);
}

TEST_CASE("Kv1.1: активация m⁴ (V½=−30)", "[kv11]") {
    Segment s = make_seg(20.0f);
    s.modifiers[0].state[0] = 0.0f;
    for (int t = 0; t < 300; t++) kv11_current(&s.modifiers[0], &s, 1.0f);
    float m_inf = 1.0f / (1.0f + expf(-(20.0f + 30.0f) / 6.0f));
    REQUIRE(s.modifiers[0].state[0] == Approx(m_inf).margin(0.02f));
}

TEST_CASE("Kv1.1: C-type медленная инактивация", "[kv11]") {
    Segment s = make_seg(0.0f);
    s.modifiers[0].state[1] = 1.0f;
    for (int t = 0; t < 20000; t++) kv11_current(&s.modifiers[0], &s, 1.0f);
    float h_inf = 1.0f / (1.0f + expf((0.0f + 40.0f) / 6.0f));
    REQUIRE(s.modifiers[0].state[1] == Approx(h_inf).margin(0.03f));
}

TEST_CASE("Kv1.1: E_rev — знак меняется у E_K", "[kv11]") {
    REQUIRE(I_open(-60.0f) < 0.0f);
    REQUIRE(I_open(-110.0f) > 0.0f);
}

TEST_CASE("Kv1.1: ВАХ — K выходит во всём диапазоне V > E_K", "[kv11]") {
    float Vs[] = {-80, -60, -40, -20, 0, 20, 40};
    for (float V : Vs) REQUIRE(I_open(V) < 0.0f);
}

TEST_CASE("Kv1.1: GHK выпрямление", "[kv11]") {
    float E_K = 26.7f * logf(4.0f / 140.0f);
    REQUIRE(fabsf(I_open(E_K + 40.0f)) > fabsf(I_open(E_K - 40.0f)));
}

TEST_CASE("Kv1.1: α-DTX блок (IC50 ~2 нМ)", "[kv11]") {
    Segment s = make_seg(30.0f);
    s.modifiers[0].state[0] = 1.0f; s.modifiers[0].state[1] = 1.0f;
    float I_no = kv11_current(&s.modifiers[0], &s, 0.0f);
    s.modifiers[0].state[5] = 20.0f;  // 20 нМ = 10×IC50
    float I_dtx = kv11_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(fabsf(I_dtx) < fabsf(I_no) * 0.2f);
}

TEST_CASE("Kv1.1: TEA блок (IC50 ~170 µМ)", "[kv11]") {
    Segment s = make_seg(30.0f);
    s.modifiers[0].state[0] = 1.0f; s.modifiers[0].state[1] = 1.0f;
    float I_no = kv11_current(&s.modifiers[0], &s, 0.0f);
    s.modifiers[0].state[6] = 1700.0f;  // 1700 µМ = 10×IC50
    float I_tea = kv11_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(fabsf(I_tea) < fabsf(I_no) * 0.2f);
}

TEST_CASE("Kv1.1: K⁺ консервация", "[kv11]") {
    Segment s = make_seg(30.0f);
    s.modifiers[0].state[0] = 1.0f; s.modifiers[0].state[1] = 1.0f;
    float k_before = s.intra_ions[ION_K] + s.extra_ions[ION_K] * EXTRA_VOLUME_RATIO;
    kv11_current(&s.modifiers[0], &s, 0.1f);
    float k_after = s.intra_ions[ION_K] + s.extra_ions[ION_K] * EXTRA_VOLUME_RATIO;
    REQUIRE(k_after == Approx(k_before).margin(0.01f));
}
