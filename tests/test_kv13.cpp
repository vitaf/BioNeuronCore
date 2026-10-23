// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_kv13.cpp — верификация MOD_KV13 (ID 154) против Kv1.3 (KCNA3, delayed-rectifier).
// Kv1.3: γ=14–24 пС, активация m⁴ (V½≈−30, типовое), GHK K⁺ (z=1). Без быстрой инактивации.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double kv13_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_KV13;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

// Полностью открытый канал (m=1), гейтинг заморожен (dt=0)
static float I_open(float V) {
    Segment s = make_seg(V);
    s.modifiers[0].state[0] = 1.0f;
    s.modifiers[0].state[1] = 1.0f;
    return kv13_current(&s.modifiers[0], &s, 0.0f);
}

TEST_CASE("Kv1.3: активация m⁴ (V½=−30)", "[kv13]") {
    Segment s = make_seg(20.0f);
    s.modifiers[0].state[0] = 0.0f;
    for (int t = 0; t < 300; t++) kv13_current(&s.modifiers[0], &s, 1.0f);  // 300 мс = 60·τ_m
    float m_inf = 1.0f / (1.0f + expf(-(20.0f + 30.0f) / 6.0f));
    REQUIRE(s.modifiers[0].state[0] == Approx(m_inf).margin(0.02f));
}

TEST_CASE("Kv1.3: delayed rectifier — при −100 мВ канал закрыт", "[kv13]") {
    Segment s = make_seg(-100.0f);
    s.modifiers[0].state[0] = 0.5f;
    for (int t = 0; t < 2000; t++) kv13_current(&s.modifiers[0], &s, 1.0f);  // 2000 мс ≫ τ_m(−100)≈149 мс
    REQUIRE(s.modifiers[0].state[0] < 0.01f);  // m → 0 при гиперполяризации
}

TEST_CASE("Kv1.3: C-type медленная инактивация (деполяризация закрывает)", "[kv13]") {
    Segment s = make_seg(0.0f);  // деполяризация → h_slow → 0
    s.modifiers[0].state[1] = 1.0f;
    for (int t = 0; t < 20000; t++) kv13_current(&s.modifiers[0], &s, 1.0f);  // 20000 мс = 6.7·τ_h
    float h_inf = 1.0f / (1.0f + expf((0.0f + 40.0f) / 6.0f));
    REQUIRE(s.modifiers[0].state[1] == Approx(h_inf).margin(0.03f));
}

TEST_CASE("Kv1.3: ShK-186 блок (IC50 ~11 пМ)", "[kv13]") {
    Segment s = make_seg(30.0f);
    s.modifiers[0].state[0] = 1.0f; s.modifiers[0].state[1] = 1.0f;
    float I_no = kv13_current(&s.modifiers[0], &s, 0.0f);
    s.modifiers[0].state[5] = 110.0f;  // 110 пМ = 10×IC50
    float I_shk = kv13_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(fabsf(I_shk) < fabsf(I_no) * 0.2f);
}

TEST_CASE("Kv1.3: E_rev — знак тока меняется у E_K (~−90 мВ)", "[kv13]") {
    REQUIRE(I_open(-60.0f) < 0.0f);   // V > E_K → K выходит
    REQUIRE(I_open(-110.0f) > 0.0f);  // V < E_K → K входит
}

TEST_CASE("Kv1.3: ВАХ — K выходит во всём диапазоне V > E_K", "[kv13]") {
    float Vs[] = {-80, -60, -40, -20, 0, 20, 40};
    for (float V : Vs) {
        float I = I_open(V);
        INFO("V=" << V << " I=" << I);
        REQUIRE(I < 0.0f);
    }
}

TEST_CASE("Kv1.3: GHK выпрямление — |I| больше при деполяризации", "[kv13]") {
    float E_K = 26.7f * logf(4.0f / 140.0f);
    float I_plus  = I_open(E_K + 40.0f);
    float I_minus = I_open(E_K - 40.0f);
    INFO("I_plus=" << I_plus << " I_minus=" << I_minus);
    REQUIRE(fabsf(I_plus) > fabsf(I_minus));
}

TEST_CASE("Kv1.3: направление переноса — K⁺ выходит при деполяризации", "[kv13]") {
    Segment s = make_seg(30.0f);
    s.modifiers[0].state[0] = 1.0f;
    s.modifiers[0].state[1] = 1.0f;
    float k_intra_before = s.intra_ions[ION_K];
    kv13_current(&s.modifiers[0], &s, 0.1f);
    REQUIRE(s.intra_ions[ION_K] < k_intra_before);  // intra K убывает (выходит)
}

TEST_CASE("Kv1.3: K⁺ консервация (intra+extra сохраняется)", "[kv13]") {
    Segment s = make_seg(30.0f);
    s.modifiers[0].state[0] = 1.0f;
    s.modifiers[0].state[1] = 1.0f;
    float k_total_before = s.intra_ions[ION_K] + s.extra_ions[ION_K] * EXTRA_VOLUME_RATIO;
    kv13_current(&s.modifiers[0], &s, 0.1f);
    float k_total_after = s.intra_ions[ION_K] + s.extra_ions[ION_K] * EXTRA_VOLUME_RATIO;
    REQUIRE(k_total_after == Approx(k_total_before).margin(0.01f));
}
