// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_ncx.cpp — верификация NCX (MOD_NCX, 99) против Na⁺/Ca²⁺ обменника.
// Weber et al. (2001), Shannon et al. (2004): 3 Na⁺ : 1 Ca²⁺, реверсивный,
// термодинамически согласованный, аллостерическая Ca²⁺-активация (K_m=125нМ).
// forward (V < E_NaCa): 3 Na внутрь, 1 Ca наружу, +1 внутрь (деполяризующий).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double ncx_current(Modifier* m, Segment* s, float dt);

// Физиологические концентрации: E_NaCa = 3·E_Na − 2·E_Ca ≈ −53 мВ.
static Segment make_seg(float V, float Na_i, float Ca_i) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_NA] = Na_i;  s.extra_ions[ION_NA] = 140.0f;   // мМ
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_CA] = Ca_i;  s.extra_ions[ION_CA] = 2.0f;
    s.intra_ions[ION_CL] = 5.0f; s.extra_ions[ION_CL] = 130.0f;
    s.ATP_level = 1.0f;
    s.radius = 5.0f; s.length = 10.0f; s.segment_type = 1;   // сома
    s.modifiers[0].type = MOD_NCX;
    s.modifiers[0].density = 300.0f;   // молекул/µm² (NCX ~250-400)
    s.modifier_count = 1;
    return s;
}

TEST_CASE("NCX: реверсивность — знак меняется при E_NaCa (~−53мВ)", "[ncx]") {
    // V < E_NaCa → forward (Ca наружу); V > E_NaCa → reverse (Ca внутрь).
    float f_lo  = ncx_flux_norm(-120.0f, 10.0f, 140.0f, 0.0001f, 2.0f);
    float f_mid = ncx_flux_norm(-53.0f,  10.0f, 140.0f, 0.0001f, 2.0f);
    float f_hi  = ncx_flux_norm( 60.0f,  10.0f, 140.0f, 0.0001f, 2.0f);
    INFO("flux(-120)=" << f_lo << " flux(-53)=" << f_mid << " flux(+60)=" << f_hi);
    REQUIRE(f_lo  > 0.0f);   // forward (гиперполяризация)
    REQUIRE(f_hi  < 0.0f);   // reverse (деполяризация)
    REQUIRE(fabsf(f_mid) < fabsf(f_lo));   // вблизи E_NaCa поток мал
    REQUIRE(fabsf(f_mid) < fabsf(f_hi));
}

TEST_CASE("NCX: V-зависимость — деполяризация сдвигает к reverse", "[ncx]") {
    float f_neg = ncx_flux_norm(-70.0f, 10.0f, 140.0f, 0.0001f, 2.0f);  // forward
    float f_zero= ncx_flux_norm(  0.0f, 10.0f, 140.0f, 0.0001f, 2.0f);  // reverse
    float f_pos = ncx_flux_norm( 60.0f, 10.0f, 140.0f, 0.0001f, 2.0f);  // reverse сильнее
    INFO("flux(-70)=" << f_neg << " flux(0)=" << f_zero << " flux(+60)=" << f_pos);
    REQUIRE(f_neg > 0.0f);    // forward при гиперполяризации
    REQUIRE(f_zero < 0.0f);   // reverse при деполяризации
    REQUIRE(f_pos < f_zero);  // сильнее reverse
}

TEST_CASE("NCX: аллостерическая Ca²⁺-активация (Hill-2)", "[ncx]") {
    // При низком Ca_i (ниже K_mCaAct=125нМ) аллостерически подавлен.
    float f_low  = ncx_flux_norm(60.0f, 10.0f, 140.0f, 0.00003f, 2.0f);   // 30 нМ
    float f_high = ncx_flux_norm(60.0f, 10.0f, 140.0f, 0.001f,   2.0f);   // 1 µM
    INFO("flux(Ca=30nM)=" << f_low << " flux(Ca=1uM)=" << f_high);
    REQUIRE(fabsf(f_high) > fabsf(f_low));
}

TEST_CASE("NCX: стехиометрия 3 Na⁺ : 1 Ca²⁺ (forward)", "[ncx]") {
    Segment s = make_seg(-70.0f, 10.0f, 0.001f);   // forward (V < E_NaCa)
    float Na_i0 = s.intra_ions[ION_NA];
    float Ca_i0 = s.intra_ions[ION_CA];

    ncx_current(&s.modifiers[0], &s, 1000.0f);   // большой dt — транспорт накапливается

    float dNa_i = s.intra_ions[ION_NA] - Na_i0;   // > 0 (Na внутрь)
    float dCa_i = s.intra_ions[ION_CA] - Ca_i0;   // < 0 (Ca наружу)
    INFO("dNa_i=" << dNa_i << " dCa_i=" << dCa_i);
    REQUIRE(dNa_i > 0.0f);
    REQUIRE(dCa_i < 0.0f);
    // 3 Na : 1 Ca → |ΔNa| = 3·|ΔCa|
    REQUIRE(fabsf(dNa_i) == Approx(3.0f * fabsf(dCa_i)).margin(1e-4f));
}

TEST_CASE("NCX: reverse-режим меняет направление транспорта", "[ncx]") {
    Segment s = make_seg(0.0f, 10.0f, 0.0001f);   // V > E_NaCa → reverse
    float Na_i0 = s.intra_ions[ION_NA];
    float Ca_i0 = s.intra_ions[ION_CA];

    ncx_current(&s.modifiers[0], &s, 1000.0f);

    float dNa_i = s.intra_ions[ION_NA] - Na_i0;   // < 0 (Na наружу) в reverse
    float dCa_i = s.intra_ions[ION_CA] - Ca_i0;   // > 0 (Ca внутрь) в reverse
    INFO("reverse dNa_i=" << dNa_i << " dCa_i=" << dCa_i);
    REQUIRE(dNa_i < 0.0f);
    REQUIRE(dCa_i > 0.0f);
}
