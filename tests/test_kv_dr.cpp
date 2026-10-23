// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_kv_dr.cpp — верификация KV_DR (mod 123) против Kv1.1 (KCNA1) delayed rectifier.
// Kv1.1 (Stühmer 1989 / Gutman 2005 / Zerr 1998): V½=−30 мВ, k≈6 мВ, τ_m(V)=14.6·exp(−(V+30)/30.1) мс.
// OHM-парадигма: I_K = g[С/см²]·m·(V−E_K).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double kv_delayed_rectifier_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 4.0f;   // мМ
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_KV_DELAYED_RECTIFIER;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[1] = KDR_VHALF_MV;
    s.modifiers[0].state[2] = KDR_K_MV;
    s.modifiers[0].state[3] = KDR_GAMMA_PS;
    s.modifiers[0].state[4] = 0.0f;   // 0 = V-зависимый τ_m (Zerr 1998)
    s.modifier_count = 1;
    return s;
}

TEST_CASE("KV_DR: гейтинг — V½=−30 мВ, полуоткрытие", "[kv_dr]") {
    REQUIRE(kv_dr_m_inf(-30.0f) == Approx(0.5f).margin(1e-3f));
    // Закрыт при гиперполяризации, открыт при деполяризации.
    REQUIRE(kv_dr_m_inf(-80.0f) < 0.01f);
    REQUIRE(kv_dr_m_inf(30.0f) > 0.99f);
    // Монотонность по V.
    REQUIRE(kv_dr_m_inf(-80.0f) < kv_dr_m_inf(-30.0f));
    REQUIRE(kv_dr_m_inf(-30.0f) < kv_dr_m_inf(30.0f));
}

TEST_CASE("KV_DR: реверс при V=E_K (nernst)", "[kv_dr]") {
    float E_K = nernst(140.0f, 4.0f, 1.0f);   // ≈ −95 мВ
    Segment s = make_seg(E_K);
    s.modifiers[0].state[0] = 1.0f;            // m = 1 (полностью открыт)
    float I = kv_delayed_rectifier_current(&s.modifiers[0], &s, 0.001f);  // малый dt → m≈1
    INFO("E_K=" << E_K << " I=" << I);
    REQUIRE(I == Approx(0.0f).margin(1e-3f));   // float-неточность ghk_monovalent при V≈E_K
    // Выше E_K — outward (return<0), ниже — inward (return>0).
    Segment s_hi = make_seg(E_K + 30.0f);
    s_hi.modifiers[0].state[0] = 1.0f;
    REQUIRE(kv_delayed_rectifier_current(&s_hi.modifiers[0], &s_hi, 0.001f) < 0.0f);
    Segment s_lo = make_seg(E_K - 30.0f);
    s_lo.modifiers[0].state[0] = 1.0f;
    REQUIRE(kv_delayed_rectifier_current(&s_lo.modifiers[0], &s_lo, 0.001f) > 0.0f);
}

TEST_CASE("KV_DR: динамика — m релаксирует к m_inf (τ(V=30)≈2 мс)", "[kv_dr]") {
    Segment s = make_seg(30.0f);   // m_inf ≈ 1
    s.modifiers[0].state[0] = 0.0f;  // старт закрыт
    for (int t = 0; t < 400; t++) {  // 400 × 0.01 мс = 4 мс = 2τ
        kv_delayed_rectifier_current(&s.modifiers[0], &s, 0.01f);
    }
    // После 2τ: m ≈ 1 − e⁻² ≈ 0.865.
    REQUIRE(s.modifiers[0].state[0] == Approx(0.865f).margin(0.03f));
}

TEST_CASE("KV_DR: параметр g_max (state[3]) не меняется", "[kv_dr]") {
    Segment s = make_seg(30.0f);
    for (int t = 0; t < 200; t++) {
        kv_delayed_rectifier_current(&s.modifiers[0], &s, 0.01f);
    }
    REQUIRE(s.modifiers[0].state[3] == Approx(KDR_GAMMA_PS).epsilon(1e-4f));
}

TEST_CASE("KV_DR: OHM-масштаб — 2× density → 2× ток (m фиксирован)", "[kv_dr]") {
    Segment s1 = make_seg(30.0f);   // V=+30, m_inf≈1
    Segment s2 = make_seg(30.0f);
    s2.modifiers[0].density = 2.0f;
    s1.modifiers[0].state[0] = 1.0f; s2.modifiers[0].state[0] = 1.0f;
    float I1 = kv_delayed_rectifier_current(&s1.modifiers[0], &s1, 0.001f);
    float I2 = kv_delayed_rectifier_current(&s2.modifiers[0], &s2, 0.001f);
    INFO("I(d=1)=" << I1 << " I(d=2)=" << I2);
    REQUIRE(I2 == Approx(2.0f * I1).margin(0.02f * fabsf(I1) + 1e-12f));
}
