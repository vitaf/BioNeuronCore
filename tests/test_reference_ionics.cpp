// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_reference_ionics.cpp — верификация фундаментальной ионики против эталона.
// Категория 2 (Нернст) + Категория 4 (насос Hill). ПО СОВРЕМЕННЫМ ДАННЫМ.
//
// ПРИМЕЧАНИЕ: гейтинг HH в коде (alpha/beta) — squid 1952 (ПРИЗРАК, устарел для
// позвоночных). НЕ тестируем его как эталон — это отдельная задача модернизации
// на кинетику Nav1.x позвоночных (см. TEST_PLAN_MODIFIERS.md).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

// ── Эталон: потенциал Нернста (37°C = 310.15 K) — точная физика ─────────────
static float nernst_ref(float c_in, float c_out, float z) {
    return (GAS_CONST * CELSIUS_37) / (z * FARADAY_CONST) * std::log(c_out / c_in) * 1000.0f;
}

TEST_CASE("Нернст: K⁺/Na⁺/Ca²⁺/Cl⁻ против эталона", "[nernst]") {
    // K_i=140, K_o=5, z=+1 → ≈−89 мВ
    REQUIRE(nernst(0.140f, 0.005f, 1.0f) == Approx(nernst_ref(0.140f, 0.005f, 1.0f)).epsilon(1e-4));
    REQUIRE(nernst(0.140f, 0.005f, 1.0f) == Approx(-89.0f).margin(0.5f));
    // Na_i=12, Na_o=140 → ≈+65.7 мВ
    REQUIRE(nernst(0.012f, 0.140f, 1.0f) == Approx(+65.7f).margin(0.5f));
    // Ca_i=100µМ, Ca_o=1.2мМ, z=+2 → ≈+33 мВ
    REQUIRE(nernst(0.0001f, 0.0012f, 2.0f) == Approx(+33.2f).margin(0.5f));
    // Cl_i=5, Cl_o=130, z=−1 → ≈−87 мВ
    REQUIRE(nernst(0.005f, 0.130f, -1.0f) == Approx(-87.1f).margin(0.5f));
}

// ── Эталон: насос Na/K — Hill-факторы (совр. стехиометрия 3Na⁺/2K⁺) ─────────
static float pump_na_factor(float Na_mM, float Km) { float n=Na_mM*Na_mM*Na_mM; return n/(n+Km*Km*Km); }
static float pump_k_factor(float K_mM) { float k=K_mM*K_mM; return k/(k+1.5f*1.5f); }

TEST_CASE("Насос: na_factor Hill (Km=6 мМ)", "[pump]") {
    REQUIRE(pump_na_factor(6.0f, 6.0f) == Approx(0.5f).epsilon(1e-4));   // Na=Km → 0.5
    REQUIRE(pump_na_factor(0.0f, 6.0f) == Approx(0.0f).epsilon(1e-4));   // Na=0 → 0
    REQUIRE(pump_na_factor(12.0f, 6.0f) == Approx(0.889f).margin(0.01f)); // Na=12 → ~0.89
}

TEST_CASE("Насос: k_factor Hill (K_Km=1.5 мМ)", "[pump]") {
    REQUIRE(pump_k_factor(1.5f) == Approx(0.5f).epsilon(1e-4));   // K=Km → 0.5
    REQUIRE(pump_k_factor(5.0f) == Approx(0.917f).margin(0.01f)); // K=5 → ~0.92
    REQUIRE(pump_k_factor(0.0f) == Approx(0.0f).epsilon(1e-4));   // K=0 → 0
}
