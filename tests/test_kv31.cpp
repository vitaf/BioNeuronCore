// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_kv31.cpp — верификация MOD_KV31 (ID 150) против Kv3.1 (KCNC1, fast-spiking).
// Kv3.1: γ≈27 пС, активация m³ (V½=+10), C-type медленная инактивация (V½≈−30), GHK K⁺.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double kv31_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_KV31;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

// Полностью открытый канал (m=1, h=1), гейтинг заморожен (dt=0)
static float I_open(float V) {
    Segment s = make_seg(V);
    s.modifiers[0].state[0] = 1.0f;
    s.modifiers[0].state[1] = 1.0f;
    return kv31_current(&s.modifiers[0], &s, 0.0f);
}

TEST_CASE("Kv3.1: активация m³ (V½=+10)", "[kv31]") {
    Segment s = make_seg(30.0f);
    s.modifiers[0].state[0] = 0.0f;
    for (int t = 0; t < 300; t++) kv31_current(&s.modifiers[0], &s, 0.1f);
    float m_inf = 1.0f / (1.0f + expf(-(30.0f - 10.0f) / 10.0f));
    REQUIRE(s.modifiers[0].state[0] == Approx(m_inf).margin(0.02f));
}

TEST_CASE("Kv3.1: C-type инактивация (деполяризация закрывает)", "[kv31]") {
    Segment s = make_seg(0.0f);  // деполяризация → h_slow → 0
    s.modifiers[0].state[1] = 1.0f;
    for (int t = 0; t < 20000; t++) kv31_current(&s.modifiers[0], &s, 1.0f);  // 20000 мс = 10·τ_h
    float h_inf = 1.0f / (1.0f + expf((0.0f + 30.0f) / 6.0f));
    REQUIRE(s.modifiers[0].state[1] == Approx(h_inf).margin(0.03f));
}

TEST_CASE("Kv3.1: E_rev — знак тока меняется у E_K (~−90 мВ)", "[kv31]") {
    REQUIRE(I_open(-60.0f) < 0.0f);   // V > E_K → K выходит (return −I_K)
    REQUIRE(I_open(-110.0f) > 0.0f);  // V < E_K → K входит
}

TEST_CASE("Kv3.1: ВАХ — K выходит во всём диапазоне V > E_K", "[kv31]") {
    float Vs[] = {-80, -60, -40, -20, 0, 20, 40};
    for (float V : Vs) {
        float I = I_open(V);
        INFO("V=" << V << " I=" << I);
        REQUIRE(I < 0.0f);  // V > E_K (~−90) → выходящий K
    }
}

TEST_CASE("Kv3.1: GHK выпрямление — |I| больше при деполяризации", "[kv31]") {
    float E_K = 26.7f * logf(4.0f / 140.0f);  // ~−94 мВ
    float I_plus  = I_open(E_K + 40.0f);       // деполяризация
    float I_minus = I_open(E_K - 40.0f);       // гиперполяризация
    INFO("I_plus=" << I_plus << " I_minus=" << I_minus);
    REQUIRE(fabsf(I_plus) > fabsf(I_minus));   // GHK асимметрия
}

TEST_CASE("Kv3.1: K⁺ консервация (intra+extra сохраняется)", "[kv31]") {
    Segment s = make_seg(30.0f);
    s.modifiers[0].state[0] = 1.0f;
    s.modifiers[0].state[1] = 1.0f;
    float k_total_before = s.intra_ions[ION_K] + s.extra_ions[ION_K] * EXTRA_VOLUME_RATIO;
    kv31_current(&s.modifiers[0], &s, 0.1f);
    float k_total_after = s.intra_ions[ION_K] + s.extra_ions[ION_K] * EXTRA_VOLUME_RATIO;
    REQUIRE(k_total_after == Approx(k_total_before).margin(0.01f));
}
