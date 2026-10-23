// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_kv34.cpp — верификация MOD_KV34 (ID 159) против Kv3.4 (KCNC4, A-type).
// Kv3.4: V½_акт +19 (k 11.3), V½_инакт −15 (k −7.4), N-ball, PKC dis-inhibition, 4-AP/TEA, GHK K⁺.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double kv34_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_KV34;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

static float I_open(float V) {
    Segment s = make_seg(V);
    s.modifiers[0].state[0] = 1.0f;
    s.modifiers[0].state[1] = 1.0f;
    s.modifiers[0].state[2] = 1.0f;  // h_slow (C-type) открыт
    return kv34_current(&s.modifiers[0], &s, 0.0f);
}

TEST_CASE("Kv3.4: активация m (V½=+19)", "[kv34]") {
    Segment s = make_seg(40.0f);
    s.modifiers[0].state[0] = 0.0f;
    for (int t = 0; t < 300; t++) kv34_current(&s.modifiers[0], &s, 1.0f);
    float m_inf = 1.0f / (1.0f + expf(-(40.0f - 19.0f) / 11.3f));
    REQUIRE(s.modifiers[0].state[0] == Approx(m_inf).margin(0.02f));
}

TEST_CASE("Kv3.4: N-ball инактивация (V½=−15)", "[kv34]") {
    Segment s = make_seg(20.0f);
    s.modifiers[0].state[1] = 1.0f;
    for (int t = 0; t < 500; t++) kv34_current(&s.modifiers[0], &s, 1.0f);
    float h_inf = 1.0f / (1.0f + expf((20.0f + 15.0f) / 7.4f));
    REQUIRE(s.modifiers[0].state[1] == Approx(h_inf).margin(0.03f));
}

TEST_CASE("Kv3.4: E_rev — K выходит при деполяризации (V½=+19)", "[kv34]") {
    REQUIRE(I_open(40.0f) < 0.0f);   // V > E_K → K выходит
    REQUIRE(I_open(-40.0f) < 0.0f);  // частично открыт, K выходит
}

TEST_CASE("Kv3.4: PKC dis-inhibition — убирает N-ball → больше ток", "[kv34]") {
    Segment s = make_seg(40.0f);
    s.modifiers[0].state[0] = 1.0f;
    s.modifiers[0].state[1] = 0.1f;  // N-ball частично инактивирован
    s.modifiers[0].state[2] = 1.0f;  // h_slow открыт
    float I_no = kv34_current(&s.modifiers[0], &s, 0.0f);
    s.modifiers[1].type = MOD_PKC;
    s.modifiers[1].state[0] = 1.0f;  // PKC → убирает N-ball
    s.modifier_count = 2;
    float I_pkc = kv34_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(fabsf(I_pkc) > fabsf(I_no));
}

TEST_CASE("Kv3.4: 4-AP блок", "[kv34]") {
    Segment s = make_seg(40.0f);
    s.modifiers[0].state[0] = 1.0f; s.modifiers[0].state[1] = 1.0f;
    s.modifiers[0].state[2] = 1.0f;  // h_slow открыт
    float I_no = kv34_current(&s.modifiers[0], &s, 0.0f);
    s.modifiers[0].state[5] = 5000.0f;  // 5 мМ = 10×IC50
    float I_ap4 = kv34_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(fabsf(I_ap4) < fabsf(I_no) * 0.2f);
}

TEST_CASE("Kv3.4: K⁺ консервация", "[kv34]") {
    Segment s = make_seg(40.0f);
    s.modifiers[0].state[0] = 1.0f; s.modifiers[0].state[1] = 1.0f;
    float k_before = s.intra_ions[ION_K] + s.extra_ions[ION_K] * EXTRA_VOLUME_RATIO;
    kv34_current(&s.modifiers[0], &s, 0.1f);
    float k_after = s.intra_ions[ION_K] + s.extra_ions[ION_K] * EXTRA_VOLUME_RATIO;
    REQUIRE(k_after == Approx(k_before).margin(0.01f));
}

TEST_CASE("Kv3.4: τ(V) V-зависимые (Rudy 2014)", "[kv34]") {
    // Активация ускоряется с деполяризацией (τ_m убывает)
    REQUIRE(kv34_m_tau(60.0f) < kv34_m_tau(-10.0f));
    // Инактивация (N-ball) ускоряется с деполяризацией (τ_h убывает)
    REQUIRE(kv34_h_tau(50.0f) < kv34_h_tau(0.0f));
    // WT hKv3.4: τ_inact ≈ 9.4 мс @ +50 мВ; Rudy даёт ~13.9 мс
    REQUIRE(kv34_h_tau(50.0f) == Approx(13.9f).margin(1.0f));
    // Cap: не расходится при глубокой гиперполяризации (recovery)
    REQUIRE(kv34_h_tau(-100.0f) <= 2000.0f);
}
