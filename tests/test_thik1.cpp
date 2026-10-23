// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_thik1.cpp — верификация THIK-1 (MOD_THIK1=166).
// K2P13.1/KCNK13: фоновая K⁺-утечка. γ≈5 пС; Po=0.5 (⚠️ не опубликована);
// галотан (state[5], мМ) блокирует по IC50=2.8 мМ; GHK K⁺; return -I_K.
// Источники: PMID 24081450, 11060316.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double thik1_current(Modifier* m, Segment* s, float dt);

// Нернст-потенциал (мВ) для одного иона.
static double nernst_mv(double c_in, double c_out, double z) {
    return (GAS_CONST * CELSIUS_37 / (z * FARADAY_CONST)) * log(c_out / c_in) * 1000.0;
}

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_K] = 140.0f;
    s.extra_ions[ION_K] = 4.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_THIK1;
    s.modifiers[0].density = 10000.0f;  // γ=5 пС — высокий density для измеримого тока (G не найдена, плейсхолдер)
    s.modifier_count = 1;
    return s;
}

TEST_CASE("THIK-1: фоновая утечка — K выходит при V > E_K", "[thik1]") {
    double e_k = nernst_mv(140.0, 4.0, 1.0);  // ≈ −91 мВ
    Segment s = make_seg(-60.0f);             // V > E_K
    float I = thik1_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(std::isnan(I) == false);
    // K-селективная утечка: ток наружу (гиперполяризующий, return < 0) при V > E_K.
    REQUIRE(I < 0.0f);
    REQUIRE(e_k < -60.0);  // предпосылка теста
}

TEST_CASE("THIK-1: E_rev = E_K (K-селективный)", "[thik1]") {
    double e_k = nernst_mv(140.0, 4.0, 1.0);
    Segment s = make_seg((float)e_k);
    float I_at_ek = thik1_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(I_at_ek == Approx(0.0f).margin(1.0f));
}

TEST_CASE("THIK-1: галотан блок (state[5]=28 мМ = 10×IC50)", "[thik1]") {
    Segment ctrl = make_seg(-60.0f);
    ctrl.modifiers[0].state[5] = 0.0f;         // без галотана
    float I_ctrl = thik1_current(&ctrl.modifiers[0], &ctrl, 0.0f);

    Segment hal = make_seg(-60.0f);
    hal.modifiers[0].state[5] = 28.0f;         // 10×IC50 (IC50=2.8 мМ)
    float I_hal = thik1_current(&hal.modifiers[0], &hal, 0.0f);

    // Po /= (1 + hal/IC50) → при 10×IC50 ток падает в 11 раз.
    REQUIRE(I_ctrl < 0.0f);
    REQUIRE(I_hal < 0.0f);
    REQUIRE(I_hal / I_ctrl == Approx(1.0f / 11.0f).epsilon(1e-3));
}

TEST_CASE("THIK-1: консервация K (intra→extra, extra-объём 10×)", "[thik1]") {
    Segment s = make_seg(-60.0f);
    float intra_before = s.intra_ions[ION_K];
    float extra_before = s.extra_ions[ION_K];

    thik1_current(&s.modifiers[0], &s, 0.1f);

    // K покидает клетку.
    REQUIRE(s.intra_ions[ION_K] < intra_before);
    REQUIRE(s.extra_ions[ION_K] > extra_before);

    // Молярная консервация: Δintra + EXTRA_VOLUME_RATIO·Δextra = 0.
    float d_intra = s.intra_ions[ION_K] - intra_before;
    float d_extra = s.extra_ions[ION_K] - extra_before;
    REQUIRE((d_intra + EXTRA_VOLUME_RATIO * d_extra) == Approx(0.0f).margin(1e-3f));
}
