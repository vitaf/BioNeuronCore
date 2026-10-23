// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "../include/modifier_helpers.h"

// Родопсин фотокаскад (192) — константы каскада против литературы.

TEST_CASE("Родопсин: τ_R* 1.5 с, τ_PDE 200 мс (Leskov 2000; Whitlock&Lamb 1999)", "[rod]") {
    REQUIRE(RHODOPSIN_TAU_R_MS == Approx(1500.0f).epsilon(1e-3f));
    REQUIRE(RHODOPSIN_TAU_PDE_MS == Approx(200.0f).epsilon(1e-3f));
    REQUIRE(RHODOPSIN_TAU_GT_MS == Approx(10.0f).epsilon(1e-3f));
}

TEST_CASE("Родопсин: CNG K1/2 0.3 норм., синтез/гидролиз cGMP", "[rod]") {
    REQUIRE(RHODOPSIN_CNG_KHALF == Approx(0.3f).epsilon(1e-3f));
    REQUIRE(RHODOPSIN_TAU_CGMP_SYN == Approx(500.0f).epsilon(1e-3f));
    REQUIRE(RHODOPSIN_TAU_CGMP_HYD == Approx(50.0f).epsilon(1e-3f));
    // гидролиз быстрее синтеза → свет надёжно закрывает CNG
    REQUIRE(RHODOPSIN_TAU_CGMP_HYD < RHODOPSIN_TAU_CGMP_SYN);
}
