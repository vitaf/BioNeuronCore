// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_ghk_reference.cpp — верификация GHK-флюкса против учебной формулы.
// Стандартный GHK-ток: J = P·z²·F²/(RT)·V·(c_i−c_o·e⁻ᶻᶠᵛ/ʳᵗ)/(1−e⁻ᶻᶠᵛ/ʳᵗ).
// Проверяет, что ghk_monovalent/divalent дают точный flux-терм (без P и площади).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "run_tick_common.h"

// Учебный flux-терм: z²·F²/(RT)·V·(c_i−c_o·e⁻ᶻᶠᵛ/ʳᵗ)/(1−e⁻ᶻᶠᵛ/ʳᵗ)
static float flux_reference(float V_mV, float c_in, float c_out, float z) {
    float V = V_mV * 0.001f;  // V
    float Vn = z * FARADAY_CONST * V / (GAS_CONST * CELSIUS_37);
    float e = std::exp(-Vn);
    float k = z * z * FARADAY_CONST * FARADAY_CONST / (GAS_CONST * CELSIUS_37);
    if (std::fabs(Vn) < 1e-6f)
        return z * FARADAY_CONST * (c_in - c_out);  // V=0 предел: z·F·(c_i−c_o)
    if (Vn > 20.0f)
        return k * V * c_in;
    if (Vn < -20.0f)
        return k * V * c_out;
    return k * V * (c_in - c_out * e) / (1.0f - e);
}

TEST_CASE("GHK: monovalent K⁺ flux matches textbook", "[ghk]") {
    // K_i=140мМ, K_o=5мМ (mol/cm³ = M × 1e-3)
    float Ki = 140.0f * 1e-3f, Ko = 5.0f * 1e-3f;
    for (float V : {-100.0f, -70.0f, -40.0f, 0.0f, 40.0f}) {
        float code = ghk_monovalent(V, Ki, Ko);
        float ref  = flux_reference(V, Ki, Ko, 1.0f);
        INFO("V=" << V << " mV, K⁺");
        REQUIRE(code == Approx(ref).epsilon(1e-4));
    }
}

TEST_CASE("GHK: monovalent Na⁺ flux matches textbook", "[ghk]") {
    float Nai = 12.0f * 1e-3f, Nao = 140.0f * 1e-3f;
    for (float V : {-100.0f, -70.0f, -40.0f, 0.0f, 40.0f}) {
        float code = ghk_monovalent(V, Nai, Nao);
        float ref  = flux_reference(V, Nai, Nao, 1.0f);
        INFO("V=" << V << " mV, Na⁺");
        REQUIRE(code == Approx(ref).epsilon(1e-4));
    }
}

TEST_CASE("GHK: divalent Ca²⁺ flux matches textbook", "[ghk]") {
    float Cai = 0.0001f * 1e-3f, Cao = 1.2f * 1e-3f;  // 100нМ / 1.2мМ
    for (float V : {-100.0f, -70.0f, -40.0f, 0.0f, 40.0f}) {
        float code = ghk_divalent(V, Cai, Cao);
        float ref  = flux_reference(V, Cai, Cao, 2.0f);
        INFO("V=" << V << " mV, Ca²⁺");
        REQUIRE(code == Approx(ref).epsilon(1e-4));
    }
}

TEST_CASE("GHK: anion Cl⁻ flux matches textbook (c_in↔c_out)", "[ghk]") {
    float Cli = 5.0f * 1e-3f, Clo = 130.0f * 1e-3f;  // Cl_i=5мМ, Cl_o=130мМ
    for (float V : {-100.0f, -70.0f, -40.0f, 0.0f, 40.0f}) {
        float code = ghk_anion(V, Cli, Clo);
        float ref  = flux_reference(V, Clo, Cli, 1.0f);  // anion: c_out↔c_in
        INFO("V=" << V << " mV, Cl⁻");
        REQUIRE(code == Approx(ref).epsilon(1e-4));
    }
}

TEST_CASE("GHK: знак потока — K⁺ наружу (>0), Na⁺ внутрь (<0)", "[ghk]") {
    float Ki = 140.0f * 1e-3f, Ko = 5.0f * 1e-3f;
    float Nai = 12.0f * 1e-3f, Nao = 140.0f * 1e-3f;
    float V = -70.0f;  // V > E_K (K⁺ out), V < E_Na (Na⁺ in)
    REQUIRE(ghk_monovalent(V, Ki, Ko) > 0.0f);   // K⁺ выходит
    REQUIRE(ghk_monovalent(V, Nai, Nao) < 0.0f); // Na⁺ входит
}
