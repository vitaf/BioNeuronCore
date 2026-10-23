// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_ghk_gating_reference.cpp — верификация гейтинга ритмогенных GHK-каналов.
// Эталон = актуальные константы modifier_helpers.h (после аудита модификаторов):
//   NaP (Nav1.6):  V½=−58.7 мВ, k=6
//   T-Ca (Cav3.2): m V½=−48.4 (k=5), h V½=−75.6 (k=6.5)
//   HCN (Ih):      V½=−82 мВ, k=6 (активация гиперполяризацией)
// Проверяем steady-state (V½/k) из единого источника nap_m_inf/tca_*/hcn_m_inf.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

TEST_CASE("NaP: m_inf активация (Nav1.6, V½=−58.7, k=6)", "[ghk_gating]") {
    REQUIRE(nap_m_inf(-58.7f) == Approx(0.5f).epsilon(1e-4));      // V½
    REQUIRE(nap_m_inf(-70.0f) == Approx(0.1320f).margin(0.002f));  // покой — закрыт
    REQUIRE(nap_m_inf(-40.0f) == Approx(0.9576f).margin(0.005f));  // подпорог — открыт
    // k=6: V½∓k → 0.269/0.731
    REQUIRE(nap_m_inf(-64.7f) == Approx(0.269f).margin(0.005f));
    REQUIRE(nap_m_inf(-52.7f) == Approx(0.731f).margin(0.005f));
}

TEST_CASE("T-Ca: m_inf активация (Cav3.2, V½=−48.4, k=5)", "[ghk_gating]") {
    REQUIRE(tca_m_inf(-48.4f) == Approx(0.5f).epsilon(1e-4));      // V½
    REQUIRE(tca_m_inf(-70.0f) == Approx(0.0131f).margin(0.002f));  // гиперполяр. — закрыт
    REQUIRE(tca_m_inf(-30.0f) == Approx(0.9754f).margin(0.005f));  // деполяр. — открыт
}

TEST_CASE("T-Ca: h_inf инактивация (Cav3.2, V½=−75.6, k=6.5)", "[ghk_gating]") {
    REQUIRE(tca_h_inf(-75.6f) == Approx(0.5f).epsilon(1e-4));      // V½
    REQUIRE(tca_h_inf(-90.0f) == Approx(0.9016f).margin(0.005f));  // гиперполяр. — доступен
    REQUIRE(tca_h_inf(-40.0f) == Approx(0.0042f).margin(0.002f));  // деполяр. — инактивирован
}

TEST_CASE("HCN: m_inf гиперполяр. активация (Ih, V½=−82, k=6)", "[ghk_gating]") {
    REQUIRE(hcn_m_inf(-82.0f) == Approx(0.5f).epsilon(1e-4));      // V½
    REQUIRE(hcn_m_inf(-100.0f) == Approx(0.9526f).margin(0.005f)); // гиперполяр. — открыт
    REQUIRE(hcn_m_inf(-60.0f) == Approx(0.0249f).margin(0.002f));  // деполяр. — закрыт
}
