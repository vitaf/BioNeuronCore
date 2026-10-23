// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_si_golden.cpp — ОРАКУЛ биологической достоверности (SI-рефакторинг).
// Ассерты сверяют поведение сети с эталоном notes/SI_REFACTORING_TABLE.md.
// Это «definition of done»: после финальной калибровки (этап 6) всё зелёное.

#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include "types.h"
#include "modifier_interface.h"
#include "units.h"
#include "modifier_helpers.h"
#include <cmath>

// ── Биологические эталоны (notes/SI_REFACTORING_TABLE.md) ─────────────
static const float BIO_NA_I_LO = 10.0f;    // мМ
static const float BIO_NA_I_HI = 15.0f;    // мМ
static const float BIO_K_I     = 140.0f;   // мМ
static const float BIO_V_REST_LO = -70.0f; // мВ
static const float BIO_V_REST_HI = -65.0f; // мВ
static const float BIO_CA_I    = 0.0001f;  // 100 нМ (в мМ)

TEST_CASE("Golden: ghk_current_pA согласован с учебным GHK", "[si_units]") {
    // P=1e-4 см/с, K⁺: c_in=140 мМ, c_out=4 мМ, V=-70 мВ, area=100 µm², open=1
    float P = 1e-4f, z = 1.0f, V = -70.0f;
    float ci = 140.0f, co = 4.0f, area = 100.0f, open = 1.0f;

    float I_code = ghk_current_pA(P, z, V, ci, co, area, open);

    // Учебная формула: I = P·z²F²/(RT)·V·(ci−co·e)/(1−e)·area·open
    float V_V = V * 1e-3f;
    float Vn = z * FARADAY_CONST * V_V / (GAS_CONST * CELSIUS_37);
    float flux = z*z*FARADAY_CONST*FARADAY_CONST/(GAS_CONST*CELSIUS_37)
               * V_V * (ci - co*std::exp(-Vn)) / (1.0f - std::exp(-Vn));
    float I_ref_pA = (P * 1e-2f) * flux * (area * 1e-12f) * open * 1e12f;

    REQUIRE(I_code == Approx(I_ref_pA).epsilon(1e-3f));
}

TEST_CASE("Golden: единицы units.h согласованы", "[si_units]") {
    // Точные переводы единиц (проверка, что слой констант корректен)
    REQUIRE(UM2_TO_CM2 == Approx(1e-8f));
    REQUIRE(MV_TO_V     == Approx(1e-3f));
    REQUIRE(A_TO_PA     == Approx(1e12f));
    REQUIRE(NA_TO_A     == Approx(1e-9f));
    REQUIRE(UM3_TO_L    == Approx(1e-15f));
    REQUIRE(MM_IS_MOL_M3 == Approx(1.0f));   // 1 мМ ≡ 1 моль/м³
}

TEST_CASE("Golden: ионный гомеостаз покоя (HH+NaK)", "[si_golden][.]") {
    init_biological_library();

    Segment seg = {};
    seg.V_m = -65.0f;
    seg.intra_ions[ION_NA] = 12.0f;   seg.extra_ions[ION_NA] = 140.0f;
    seg.intra_ions[ION_K]  = 140.0f;  seg.extra_ions[ION_K]  = 4.0f;
    seg.length = 100.0f;
    seg.radius = 10.0f;
    seg.ATP_level = 1.0f;
    seg.modifiers[0] = {MOD_AXON_NAV12, {0,0,0}, 15.6f, {0.05f,0.6f}, 2, 0, {0,0}};
    seg.modifiers[1] = {MOD_KV21, {0,0,0}, 3.84f, {0.05f,1.0f}, 2, 0, {0,0}};
    seg.modifiers[2] = {MOD_PUMP_NAK, {0,0,0}, 1.0f, {}, 0, 0, {0,0}};
    seg.modifier_count = 3;

    float area = 2.0f * PI * seg.radius * seg.length;

    for (int t = 0; t < 2000; t++) {
        float I_mod = biological_library[MOD_AXON_NAV12](&seg.modifiers[0], &seg, 1.0f)
                    + biological_library[MOD_KV21](&seg.modifiers[1], &seg, 1.0f);
        float I_pump = biological_library[MOD_PUMP_NAK](&seg.modifiers[2], &seg, 1.0f);
        float dV = (I_mod + I_pump) / (area * C_SPECIFIC_PF_PER_UM2) * 1.0f;
        if (dV > 20.0f) dV = 20.0f;
        if (dV < -20.0f) dV = -20.0f;
        seg.V_m += dV;
        if (seg.V_m < -200.0f) seg.V_m = -200.0f;
        if (seg.V_m > 200.0f)  seg.V_m = 200.0f;
    }

    INFO("V_m=" << seg.V_m << " Na_i=" << seg.intra_ions[ION_NA]
         << " K_i=" << seg.intra_ions[ION_K]);
    // Базовый HH+NaK покой ≈ −65 мВ (полно-нейронный V_rest −65..−70 — этап 6)
    REQUIRE(seg.V_m == Approx(-65.0f).margin(3.0f));
    REQUIRE(seg.intra_ions[ION_NA] >= BIO_NA_I_LO);
    REQUIRE(seg.intra_ions[ION_NA] <= BIO_NA_I_HI);
    REQUIRE(seg.intra_ions[ION_K] == Approx(BIO_K_I).margin(5.0f));
}

// ── TODO (этап 6, финальная калибровка) ────────────────────────────────
// Полно-нейронные ассерты: V_rest dIN/cIN/aIN = −65..−70 (сейчас −20..−27),
// Ca_i = 100 нМ (сейчас 50 мкМ), спайк палит с полной архитектурой.
