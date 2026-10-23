// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_trpm4.cpp — верификация TRPM4 (MOD_TRPM4=164).
// TRPM4: Ca²⁺-активируемый неселективный Na⁺/K⁺ (не Ca). γ=25 пС;
// Ca²⁺-зависимый Po (EC50 0.4 µM); 9-phenanthrol (state[5], µM) IC50=18 µM;
// GHK Na/K (z=1, одинаковый P_base); return -(I_Na + I_K).
// Источники: PMID 12015988, 15670874, 23416167.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double trpm4_current(Modifier* m, Segment* s, float dt);

// Нернст-потенциал (мВ) для одного иона.
static double nernst_mv(double c_in, double c_out, double z) {
    return (GAS_CONST * CELSIUS_37 / (z * FARADAY_CONST)) * log(c_out / c_in) * 1000.0;
}

// Единый сегмент: Na/K градиенты, intra_Ca=ca (мМ), extra_Ca=2 мМ.
static Segment make_seg(float V, float ca) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_NA] = 15.0f;  s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K]  = 4.0f;
    s.intra_ions[ION_CA] = ca;     s.extra_ions[ION_CA] = 2.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_TRPM4;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

TEST_CASE("TRPM4: Ca²⁺-зависимый Po (EC50 0.4 µM)", "[trpm4]") {
    // Низкий Ca²⁺ (0.0001 мМ = 0.1 µM) → Po = 0.1/(0.1+0.4) = 0.2 → малый ток.
    Segment s_low = make_seg(-60.0f, 0.0001f);
    float I_low = trpm4_current(&s_low.modifiers[0], &s_low, 0.0f);

    // Высокий Ca²⁺ (0.01 мМ = 10 µM) → Po = 10/(10+0.4) ≈ 0.96 → большой ток.
    Segment s_high = make_seg(-60.0f, 0.01f);
    float I_high = trpm4_current(&s_high.modifiers[0], &s_high, 0.0f);

    REQUIRE(std::isnan(I_low) == false);
    REQUIRE(std::isnan(I_high) == false);
    // Po линейно масштабирует ток: Po(10µM)/Po(0.1µM) = 0.96/0.2 ≈ 4.8.
    REQUIRE(fabsf(I_high) > fabsf(I_low));
    REQUIRE(fabsf(I_high) / (fabsf(I_low) + 1e-9f) > 3.0f);
}

TEST_CASE("TRPM4: E_rev ≈ 0 мВ (Na/K неселективный)", "[trpm4]") {
    // Комбинированный E_rev при P_Na=P_K: E_rev = (RT/F)·ln(([Na]o+[K]o)/([Na]i+[K]i)).
    double e_rev = nernst_mv(15.0 + 140.0, 140.0 + 4.0, 1.0);  // ≈ −2 мВ
    Segment s = make_seg((float)e_rev, 0.01f);
    float I = trpm4_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(I == Approx(0.0f).margin(1.0f));
    // Предпосылка: неселективный Na/K → E_rev близок к 0 мВ.
    REQUIRE(fabs(e_rev) < 5.0);
}

TEST_CASE("TRPM4: 9-phenanthrol блок (state[5]=180 µM = 10×IC50)", "[trpm4]") {
    Segment ctrl = make_seg(-60.0f, 0.01f);
    ctrl.modifiers[0].state[5] = 0.0f;   // без фенантрола
    float I_ctrl = trpm4_current(&ctrl.modifiers[0], &ctrl, 0.0f);

    Segment phen = make_seg(-60.0f, 0.01f);
    phen.modifiers[0].state[5] = 180.0f; // 10×IC50 (IC50=18 µM)
    float I_phen = trpm4_current(&phen.modifiers[0], &phen, 0.0f);

    // Po /= (1 + phen/IC50) → при 10×IC50 ток падает в 11 раз.
    REQUIRE(fabsf(I_ctrl) > fabsf(I_phen));
    REQUIRE(I_phen / I_ctrl == Approx(1.0f / 11.0f).epsilon(1e-3));
}

TEST_CASE("TRPM4: консервация Na/K (intra↔extra, extra-объём 10×)", "[trpm4]") {
    Segment s = make_seg(-60.0f, 0.01f);
    float na_i0 = s.intra_ions[ION_NA], na_o0 = s.extra_ions[ION_NA];
    float k_i0  = s.intra_ions[ION_K],  k_o0  = s.extra_ions[ION_K];

    trpm4_current(&s.modifiers[0], &s, 0.1f);

    // При V=-60: Na входит (V < E_Na≈+60), K выходит (V > E_K≈−95).
    REQUIRE(s.intra_ions[ION_NA] > na_i0);
    REQUIRE(s.extra_ions[ION_NA] < na_o0);
    REQUIRE(s.intra_ions[ION_K] < k_i0);
    REQUIRE(s.extra_ions[ION_K] > k_o0);

    // Молярная консервация: Δintra + 10·Δextra = 0 (extra-объём = 10× intra).
    float d_na_i = s.intra_ions[ION_NA] - na_i0;
    float d_na_o = s.extra_ions[ION_NA] - na_o0;
    float d_k_i  = s.intra_ions[ION_K]  - k_i0;
    float d_k_o  = s.extra_ions[ION_K]  - k_o0;
    REQUIRE((d_na_i + EXTRA_VOLUME_RATIO * d_na_o) == Approx(0.0f).margin(1e-3f));
    REQUIRE((d_k_i  + EXTRA_VOLUME_RATIO * d_k_o)  == Approx(0.0f).margin(1e-3f));
}
