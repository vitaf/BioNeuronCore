// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "../include/modifier_helpers.h"
#include "../include/types.h"

// Пересверка изоформ вкус/сенсор (177-182) — γ/Po/IC50 против литературы.

TEST_CASE("Nav1.3 (177): γ≈20 пС (оценка-аналогия Nav1.2, Cummins 2001)", "[mod177]") {
    REQUIRE(NAV13_GAMMA_PS == Approx(20.0f).epsilon(1e-3f));
}

TEST_CASE("Kir7.1 (178): γ=0.05 пС (Krapivinsky 1998)", "[mod178]") {
    REQUIRE(KIR71_GAMMA_PS == Approx(0.05f).epsilon(1e-3f));
}

TEST_CASE("TRPC6/7 (179): γ=35 пС (Perez-Leighton 2011)", "[mod179]") {
    REQUIRE(TRPC67_GAMMA_PS == Approx(35.0f).epsilon(1e-3f));
}

TEST_CASE("CALHM1/3 (180): γ=25 пС (Ma 2012 PNAS)", "[mod180]") {
    REQUIRE(CALHM13_GAMMA_PS == Approx(25.0f).epsilon(1e-3f));
}

TEST_CASE("OTOP1 (181): H⁺-канал, γ=20 пС, Zn²⁺ IC50 0.19 мМ (Tu 2018)", "[mod181]") {
    REQUIRE(OTOP1_GAMMA_PS == Approx(20.0f).epsilon(1e-3f));
    REQUIRE(OTOP1_ZN_IC50_UM == Approx(190.0f).epsilon(1e-3f));
}

TEST_CASE("ENaC (182): γ=4.5 пС, Po 0.5 (Kellenberger 2002)", "[mod182]") {
    REQUIRE(ENAC_GAMMA_PS == Approx(4.5f).epsilon(1e-3f));
    REQUIRE(ENAC_PO == Approx(0.5f).epsilon(1e-3f));
}
