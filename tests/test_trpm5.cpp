// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_trpm5.cpp — верификация MOD_TRPM5 (165).
// TRPM5: Ca²⁺-активируемый неселективный катионный канал (Na⁺/K⁺), γ=25 пС,
// Po = Ca^n/(EC50^n + Ca^n), n=2.4, EC50=0.7 µМ. Моновалентный (не Ca²⁺-проводящий).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double trpm5_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V, float Ca_i_mM) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_NA] = 15.0f;  s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K]  = 4.0f;
    s.intra_ions[ION_CA] = Ca_i_mM; s.extra_ions[ION_CA] = 2.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_TRPM5;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

// ── (а) Ca²⁺-Hill: Po(0.1 µМ)≈0, Po(EC50 0.7 µМ)≈0.5, Po(7 µМ)≈1 ─────────
TEST_CASE("TRPM5: Ca²⁺-Hill (EC50=0.7 µМ, Hill 2.4)", "[trpm5]") {
    // 0.1 µМ Ca → Po≈0.009 (канал практически закрыт)
    Segment s_lo = make_seg(-60.0f, 0.0001f);
    float I_lo = fabsf(trpm5_current(&s_lo.modifiers[0], &s_lo, 0.0f));

    // 0.7 µМ Ca (= EC50) → Po=0.5 (полумаксимум)
    Segment s_half = make_seg(-60.0f, 0.0007f);
    float I_half = fabsf(trpm5_current(&s_half.modifiers[0], &s_half, 0.0f));

    // 7 µМ Ca (10× EC50) → Po≈1 (насыщение)
    Segment s_sat = make_seg(-60.0f, 0.007f);
    float I_sat = fabsf(trpm5_current(&s_sat.modifiers[0], &s_sat, 0.0f));

    REQUIRE(I_lo < 0.02f * I_sat);                                // Po(0.1 µМ) ≈ 0.009
    REQUIRE(I_half == Approx(0.5f * I_sat).margin(0.03f * I_sat)); // Po(EC50) = 0.5
    REQUIRE(I_sat > I_half);                                       // насыщение выше полумаксимума
}

// ── (б) E_rev: неселективный Na⁺/K⁺ → E_rev ≈ −2 мВ ───────────────────────
TEST_CASE("TRPM5: E_rev ≈ −2 мВ (P_Na=P_K)", "[trpm5]") {
    // E_rev = (RT/F)·ln((Na_o+K_o)/(Na_i+K_i)) при P_Na=P_K (неселективный, 1:1)
    float erev = (GAS_CONST * CELSIUS_37 / FARADAY_CONST) *
                 logf((140.0f + 4.0f) / (15.0f + 140.0f)) * 1000.0f;  // ≈ −1.97 мВ

    Segment s = make_seg(erev, 0.007f);   // насыщающий Ca → Po≈1
    float I_rev = trpm5_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(fabsf(I_rev) < 1.0f);         // ток ≈ 0 в точке реверсии

    // Выше E_rev → наружу (return<0), ниже → внутрь (return>0)
    Segment s_hi = make_seg(erev + 40.0f, 0.007f);
    Segment s_lo = make_seg(erev - 40.0f, 0.007f);
    REQUIRE(trpm5_current(&s_hi.modifiers[0], &s_hi, 0.0f) < 0.0f);
    REQUIRE(trpm5_current(&s_lo.modifiers[0], &s_lo, 0.0f) > 0.0f);
}

// ── (в) Консервация Na⁺/K⁺ (внеклеточный объём = 10× внутриклеточного) ────
TEST_CASE("TRPM5: консервация Na⁺/K⁺", "[trpm5]") {
    Segment s = make_seg(-20.0f, 0.007f);   // деполяризация → Na внутрь, K наружу
    float na_i0 = s.intra_ions[ION_NA], na_o0 = s.extra_ions[ION_NA];
    float k_i0  = s.intra_ions[ION_K],  k_o0  = s.extra_ions[ION_K];
    for (int t = 0; t < 100; t++) trpm5_current(&s.modifiers[0], &s, 0.1f);

    float na_i1 = s.intra_ions[ION_NA], na_o1 = s.extra_ions[ION_NA];
    float k_i1  = s.intra_ions[ION_K],  k_o1  = s.extra_ions[ION_K];

    // Внеклеточный объём 10× → концентрация меняется в 10× слабее: intra + 10·extra = const
    REQUIRE(na_i1 + EXTRA_VOLUME_RATIO * na_o1 == Approx(na_i0 + EXTRA_VOLUME_RATIO * na_o0).margin(0.1f));
    REQUIRE(k_i1  + EXTRA_VOLUME_RATIO * k_o1  == Approx(k_i0  + EXTRA_VOLUME_RATIO * k_o0 ).margin(0.1f));

    // Направление: Na⁺ входит (деполяризация), K⁺ выходит
    REQUIRE(na_i1 > na_i0);
    REQUIRE(k_i1  < k_i0);
}
