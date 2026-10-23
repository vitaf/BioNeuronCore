// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "../include/modifier_helpers.h"
#include "../include/types.h"

TEST_CASE("Cav3.3: γ = 11 пС (Perez-Reyes 2003)", "[cav33]") {
    REQUIRE(CAV33_GAMMA_PS == Approx(11.0f).epsilon(1e-4f));
}

TEST_CASE("Cav3.3: V½_m=−43.8, V½_h=−72.5 (Klöckner 1999)", "[cav33]") {
    REQUIRE(CAV33_VHALF_M == Approx(-43.8f).epsilon(1e-4f));
    REQUIRE(CAV33_VHALF_H == Approx(-72.5f).epsilon(1e-4f));
    REQUIRE(boltzmann_act(CAV33_VHALF_M, CAV33_VHALF_M, CAV33_K_M) == Approx(0.5f).margin(1e-3f));
    REQUIRE(boltzmann_inact(CAV33_VHALF_H, CAV33_VHALF_H, CAV33_K_H) == Approx(0.5f).margin(1e-3f));
}

TEST_CASE("Cav3.3: τ_m bell-shape (деактивация 1.25 мс, пик 30 мс)", "[cav33]") {
    REQUIRE(cav33_m_tau(-100.0f) == Approx(1.25f).margin(0.1f));  // деактивация Lee 1999
    REQUIRE(cav33_m_tau(-20.0f) == Approx(30.0f).margin(0.1f));   // пик Klöckner 1999
    REQUIRE(cav33_m_tau(30.0f) < 5.0f);                            // быстрая при деполяр.
    REQUIRE(cav33_m_tau(-20.0f) > cav33_m_tau(-100.0f));           // bell-shape (пик > деактивация)
}

TEST_CASE("Cav3.3: медленнее Cav3.2 (τ_h)", "[cav33]") {
    REQUIRE(CAV33_TAU_H_FAST_MS > TCA_TAU_H_FAST_MS);   // 97 vs 15 мс
    REQUIRE(CAV33_TAU_H_SLOW_MS > TCA_TAU_H_SLOW_MS);   // 400 vs 150 мс
}

TEST_CASE("Cav3.3: фармакология IC50 (Concise Guide 2023/24)", "[cav33]") {
    REQUIRE(CAV33_NI_IC50_UM == Approx(200.0f).epsilon(1e-3f));
    REQUIRE(CAV33_MIBEFRADIL_IC50_UM == Approx(1.6f).epsilon(1e-3f));
    REQUIRE(CAV33_TTAP2_IC50_NM == Approx(82.0f).epsilon(1e-3f));
    REQUIRE(CAV33_Z944_IC50_NM == Approx(100.0f).epsilon(1e-3f));
}
