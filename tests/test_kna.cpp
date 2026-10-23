// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_kna.cpp — верификация KNa (mod 121) против Slack (KCNT1/Slo2.2).
// Slack (Yuan et al. 2003, Neuron 37:765): γ=180 пС, Na⁺ EC50≈41 мМ, Hill≈3.4,
// чисто Na⁺-гейт (нет S4), медленная активация (slow AHP).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double kna_channel_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V, float Na_mM) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 4.0f;   // мМ
    s.intra_ions[ION_NA] = Na_mM; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_CA] = 0.0001f;                              // мМ (моль/м³)
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_KNA_CHANNEL;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[1] = KNA_NA_EC50_MM;  // EC50 (мМ)
    s.modifiers[0].state[3] = KNA_GAMMA_PS;    // γ (пС)
    s.modifiers[0].state[4] = KNA_TAU_MS;      // τ (мс)
    s.modifier_count = 1;
    return s;
}

TEST_CASE("KNa: Hill steady-state — EC50=41 мМ, полуоткрытие", "[kna]") {
    // При [Na]in = EC50 (41 мМ) → m_inf = 0.5.
    REQUIRE(kna_m_inf(0.041f) == Approx(0.5f).margin(1e-3f));
    // Выше EC50 — открыт, ниже — закрыт.
    REQUIRE(kna_m_inf(0.0f) == Approx(0.0f).margin(1e-6f));
    REQUIRE(kna_m_inf(0.120f) > 0.95f);   // 120 мМ → почти полностью открыт
}

TEST_CASE("KNa: покой закрыт, burst открыт", "[kna]") {
    // Покой [Na]in ≈ 15 мМ → m_inf ≈ 0.03 (закрыт).
    REQUIRE(kna_m_inf(0.015f) < 0.1f);
    // Burst: [Na]in ≈ 40 мМ → m_inf ≈ 0.48 (полуоткрыт).
    REQUIRE(kna_m_inf(0.040f) > 0.4f);
    // Монотонность по Na⁺.
    REQUIRE(kna_m_inf(0.015f) < kna_m_inf(0.040f));
    REQUIRE(kna_m_inf(0.040f) < kna_m_inf(0.080f));
}

TEST_CASE("KNa: P-шкала — γ→P_single→×density (без GHK_P_PER_CM_S)", "[kna]") {
    // P_single = ghk_single_channel_P(γ, z=1, c̄_K) — честный chord-conductance (без суррогата).
    double P_single = ghk_single_channel_P(KNA_GAMMA_PS, 1.0f, GHK_CBAR_K_MM);
    REQUIRE(P_single == Approx(ghk_single_channel_P(180.0f, 1.0f, GHK_CBAR_K_MM)).epsilon(1e-3));

    // Density-масштаб: 2× density → 2× ток (open_prob фиксирован = m_inf при EC50).
    Segment s1 = make_seg(-20.0f, 0.041f);  // [Na]=EC50 → m_inf=0.5, V=−20 outward K
    Segment s2 = make_seg(-20.0f, 0.041f);
    s2.modifiers[0].density = 2.0f;
    s1.modifiers[0].state[0] = 0.5f; s2.modifiers[0].state[0] = 0.5f;
    float I1 = kna_channel_current(&s1.modifiers[0], &s1, 0.01f);
    float I2 = kna_channel_current(&s2.modifiers[0], &s2, 0.01f);
    INFO("I(d=1)=" << I1 << " I(d=2)=" << I2);
    REQUIRE(I2 == Approx(2.0f * I1).margin(0.02f * fabsf(I1) + 1e-12f));
}

TEST_CASE("KNa: динамика — open_prob релаксирует к m_inf (τ=800 мс)", "[kna]") {
    Segment s = make_seg(-65.0f, 0.080f);  // [Na]=80 мМ → m_inf ≈ 0.91
    s.modifiers[0].state[0] = 0.0f;        // open_prob = 0 старт
    float m_inf = kna_m_inf(0.080f);
    for (int t = 0; t < 800; t++) {        // 800 × 1 мс = 1τ
        kna_channel_current(&s.modifiers[0], &s, 1.0f);
    }
    // После 1τ: open_prob ≈ m_inf·(1−e⁻¹) = 0.632·m_inf.
    REQUIRE(s.modifiers[0].state[0] == Approx(0.632f * m_inf).margin(0.06f * m_inf));
}

TEST_CASE("KNa: параметр γ (state[3]) не меняется", "[kna]") {
    Segment s = make_seg(30.0f, 0.080f);
    for (int t = 0; t < 200; t++) {
        kna_channel_current(&s.modifiers[0], &s, 0.01f);
    }
    REQUIRE(s.modifiers[0].state[3] == Approx(KNA_GAMMA_PS).epsilon(1e-4f));
}

TEST_CASE("KNa: знак — return=−I_K (K⁺ выходит → гиперполяризация)", "[kna]") {
    // Деполяризация + высокий Na → канал открыт → K⁺ outward → return < 0.
    Segment s = make_seg(30.0f, 0.080f);
    s.modifiers[0].state[0] = 1.0f;
    float I = kna_channel_current(&s.modifiers[0], &s, 0.01f);
    REQUIRE(I < 0.0f);
}
