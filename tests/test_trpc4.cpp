// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_trpc4.cpp — верификация TRPC4 (MOD_TRPC4=162).
// TRPC4: Gq/PLC-зависимый неселективный катионный (Na⁺/Ca²⁺, PCa/PNa≈4).
// γ=40 пС; Po=state[4] (Gq 0–1); ML204 (state[5], µМ) блокирует по IC50=2 µМ;
// GHK Na⁺ (z=1) + Ca²⁺ (z=2, ×4); return -(I_Na + I_Ca).
// Источники: PMID 21795696, 29385160.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double trpc4_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_CA] = 0.0001f; s.extra_ions[ION_CA] = 2.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_TRPC4;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[4] = 1.0f;  // Gq=1 → канал открыт
    s.modifier_count = 1;
    return s;
}

TEST_CASE("TRPC4: Po=state[4] — Gq=1 входящий ток, Gq=0 → 0", "[trpc4]") {
    Segment on = make_seg(-60.0f);
    float I_on = trpc4_current(&on.modifiers[0], &on, 0.0f);
    REQUIRE(std::isnan(I_on) == false);
    // Входящий (деполяризующий) ток: return -(I_Na+I_Ca) > 0 при Na/Ca-входе.
    REQUIRE(I_on > 0.01f);

    Segment off = make_seg(-60.0f);
    off.modifiers[0].state[4] = 0.0f;
    float I_off = trpc4_current(&off.modifiers[0], &off, 0.0f);
    REQUIRE(I_off == Approx(0.0f).margin(1e-3f));
}

TEST_CASE("TRPC4: E_rev — Na/Ca входят при V < E_rev (положительный ток)", "[trpc4]") {
    // E_Na ≈ +60 мВ, E_Ca ≈ +132 мВ → E_rev смешанного Na/Ca канала сильно положителен.
    Segment below = make_seg(-80.0f);
    float I_below = trpc4_current(&below.modifiers[0], &below, 0.0f);
    REQUIRE(I_below > 0.0f);  // при V < E_rev Na/Ca входят → возврат положителен

    Segment above = make_seg(200.0f);  // V ≫ E_rev → ток наружу
    float I_above = trpc4_current(&above.modifiers[0], &above, 0.0f);
    REQUIRE(I_above < 0.0f);
}

TEST_CASE("TRPC4: ML204 блок (state[5]=20 µМ = 10×IC50)", "[trpc4]") {
    Segment ctrl = make_seg(-60.0f);
    ctrl.modifiers[0].state[5] = 0.0f;   // без ML204
    float I_ctrl = trpc4_current(&ctrl.modifiers[0], &ctrl, 0.0f);

    Segment ml = make_seg(-60.0f);
    ml.modifiers[0].state[5] = 20.0f;    // 10×IC50 (IC50=2 µМ)
    float I_ml = trpc4_current(&ml.modifiers[0], &ml, 0.0f);

    // Po /= (1 + ml/IC50) → при 10×IC50 ток падает в 11 раз.
    REQUIRE(I_ctrl > 0.0f);
    REQUIRE(I_ml > 0.0f);
    REQUIRE(I_ml / I_ctrl == Approx(1.0f / 11.0f).epsilon(1e-3));
}

TEST_CASE("TRPC4: консервация Na⁺ (intra↔extra, extra-объём 10×)", "[trpc4]") {
    Segment s = make_seg(-60.0f);
    float intra_before = s.intra_ions[ION_NA];
    float extra_before = s.extra_ions[ION_NA];

    trpc4_current(&s.modifiers[0], &s, 0.1f);

    // Na входит в клетку (входящий ток).
    REQUIRE(s.intra_ions[ION_NA] > intra_before);
    REQUIRE(s.extra_ions[ION_NA] < extra_before);

    // Молярная консервация: Δintra + 10·Δextra = 0 (extra-объём = 10× intra).
    float d_intra = s.intra_ions[ION_NA] - intra_before;
    float d_extra = s.extra_ions[ION_NA] - extra_before;
    REQUIRE((d_intra + EXTRA_VOLUME_RATIO * d_extra) == Approx(0.0f).margin(1e-3f));
}
