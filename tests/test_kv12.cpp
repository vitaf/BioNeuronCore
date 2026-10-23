// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_kv12.cpp — верификация MOD_KV12 (ID 157) против Kv1.2 (KCNA2, delayed-rectifier).
// Kv1.2: γ=14–18 пС, m⁴ (V½ −27), C-type инактивация (τ~0.4 с), α-DTX/TEA + PKC, GHK K⁺.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double kv12_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_KV12;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

static float I_open(float V) {
    Segment s = make_seg(V);
    s.modifiers[0].state[0] = 1.0f;
    s.modifiers[0].state[1] = 1.0f;
    return kv12_current(&s.modifiers[0], &s, 0.0f);
}

TEST_CASE("Kv1.2: активация m⁴ (V½=−27)", "[kv12]") {
    Segment s = make_seg(20.0f);
    s.modifiers[0].state[0] = 0.0f;
    for (int t = 0; t < 300; t++) kv12_current(&s.modifiers[0], &s, 1.0f);
    float m_inf = 1.0f / (1.0f + expf(-(20.0f + 27.0f) / 6.0f));
    REQUIRE(s.modifiers[0].state[0] == Approx(m_inf).margin(0.02f));
}

TEST_CASE("Kv1.2: C-type инактивация (быстрее)", "[kv12]") {
    Segment s = make_seg(0.0f);
    s.modifiers[0].state[1] = 1.0f;
    for (int t = 0; t < 5000; t++) kv12_current(&s.modifiers[0], &s, 1.0f);
    float h_inf = 1.0f / (1.0f + expf((0.0f + 40.0f) / 6.0f));
    REQUIRE(s.modifiers[0].state[1] == Approx(h_inf).margin(0.03f));
}

TEST_CASE("Kv1.2: E_rev — знак меняется у E_K", "[kv12]") {
    REQUIRE(I_open(-60.0f) < 0.0f);
    REQUIRE(I_open(-110.0f) > 0.0f);
}

TEST_CASE("Kv1.2: GHK выпрямление", "[kv12]") {
    float E_K = 26.7f * logf(4.0f / 140.0f);
    REQUIRE(fabsf(I_open(E_K + 40.0f)) > fabsf(I_open(E_K - 40.0f)));
}

TEST_CASE("Kv1.2: α-DTX блок (IC50 ~5 нМ)", "[kv12]") {
    Segment s = make_seg(30.0f);
    s.modifiers[0].state[0] = 1.0f; s.modifiers[0].state[1] = 1.0f;
    float I_no = kv12_current(&s.modifiers[0], &s, 0.0f);
    s.modifiers[0].state[5] = 50.0f;  // 50 нМ = 10×IC50
    float I_dtx = kv12_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(fabsf(I_dtx) < fabsf(I_no) * 0.2f);
}

TEST_CASE("Kv1.2: PKC-ингибиция (mGluR1→PKC→↓ток)", "[kv12]") {
    Segment s = make_seg(30.0f);
    s.modifiers[0].state[0] = 1.0f; s.modifiers[0].state[1] = 1.0f;
    float I_no = kv12_current(&s.modifiers[0], &s, 0.0f);
    s.modifiers[1].type = MOD_PKC;
    s.modifiers[1].state[0] = 1.0f;  // PKC активна
    s.modifier_count = 2;
    float I_pkc = kv12_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(fabsf(I_pkc) < fabsf(I_no) * 0.3f);  // PKC блок ~80%
}

TEST_CASE("Kv1.2: K⁺ консервация", "[kv12]") {
    Segment s = make_seg(30.0f);
    s.modifiers[0].state[0] = 1.0f; s.modifiers[0].state[1] = 1.0f;
    float k_before = s.intra_ions[ION_K] + s.extra_ions[ION_K] * EXTRA_VOLUME_RATIO;
    kv12_current(&s.modifiers[0], &s, 0.1f);
    float k_after = s.intra_ions[ION_K] + s.extra_ions[ION_K] * EXTRA_VOLUME_RATIO;
    REQUIRE(k_after == Approx(k_before).margin(0.01f));
}
