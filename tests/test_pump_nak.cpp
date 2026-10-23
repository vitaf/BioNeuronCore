// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_pump_nak.cpp — верификация Na/K-АТФазы (MOD_PUMP_NAK, 7).
// Цикл Поста–Альберса (Smith & Crampin 2004, Läuger 1991, Heyse 1994):
//   стехиометрия 3 Na⁺ наружу : 2 K⁺ внутрь : 1 ATP, электрогенный (+1 наружу).
//   Единственный V-зависимый шаг — E1P ⇌ E2P (перенос заряда z·δ).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double nak_pump_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V, float Na_i, float K_o, float ATP) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_NA] = Na_i;   // мМ ≡ моль/м³
    s.intra_ions[ION_K]  = 140.0f;
    s.extra_ions[ION_NA] = 140.0f;
    s.extra_ions[ION_K]  = K_o;
    s.intra_ions[ION_CA] = 0.0001f;
    s.extra_ions[ION_CA] = 2.0f;
    s.intra_ions[ION_CL] = 5.0f;
    s.extra_ions[ION_CL] = 130.0f;
    s.ATP_level = ATP;
    s.radius = 5.0f; s.length = 10.0f; s.segment_type = 1;   // сома
    s.modifiers[0].type = MOD_PUMP_NAK;
    s.modifiers[0].density = 1000.0f;   // молекул/µm² (литературная плотность сомы)
    s.modifier_count = 1;
    return s;
}

TEST_CASE("PUMP_NAK: v_norm ограничен [0,1]", "[pump_nak]") {
    REQUIRE(nak_pump_vnorm( 50.0f, 15.0f, 5.0f, 1.0f) > 0.0f);
    REQUIRE(nak_pump_vnorm( 50.0f, 15.0f, 5.0f, 1.0f) < 1.0f);
    REQUIRE(nak_pump_vnorm(-70.0f, 15.0f, 5.0f, 1.0f) > 0.0f);
    REQUIRE(nak_pump_vnorm(-70.0f, 15.0f, 5.0f, 1.0f) < 1.0f);
}

TEST_CASE("PUMP_NAK: v_norm — деполяризация ускоряет (электрогенность)", "[pump_nak]") {
    // Электрогенный шаг E1P→E2P: при +V насос быстрее, при −V медленнее.
    float v_hi  = nak_pump_vnorm( 50.0f, 15.0f, 5.0f, 1.0f);
    float v_mid = nak_pump_vnorm(  0.0f, 15.0f, 5.0f, 1.0f);
    float v_rest= nak_pump_vnorm(-70.0f, 15.0f, 5.0f, 1.0f);
    float v_lo  = nak_pump_vnorm(-120.0f, 15.0f, 5.0f, 1.0f);
    INFO("v(+50)=" << v_hi << " v(0)=" << v_mid << " v(-70)=" << v_rest << " v(-120)=" << v_lo);
    REQUIRE(v_hi  > v_mid);
    REQUIRE(v_mid > v_rest);
    REQUIRE(v_rest > v_lo);
}

TEST_CASE("PUMP_NAK: v_norm — насыщение по Na_i (Hill-3)", "[pump_nak]") {
    float v1   = nak_pump_vnorm(-70.0f, 1.0f,  5.0f, 1.0f);
    float v10  = nak_pump_vnorm(-70.0f, 10.0f, 5.0f, 1.0f);
    float v100 = nak_pump_vnorm(-70.0f, 100.0f, 5.0f, 1.0f);
    INFO("v(Na=1)=" << v1 << " v(Na=10)=" << v10 << " v(Na=100)=" << v100);
    REQUIRE(v100 > v10);
    REQUIRE(v10  > v1);
}

TEST_CASE("PUMP_NAK: v_norm — насыщение по K_o (Hill-2)", "[pump_nak]") {
    float v_lo = nak_pump_vnorm(-70.0f, 15.0f, 0.2f, 1.0f);
    float v_mid= nak_pump_vnorm(-70.0f, 15.0f, 1.5f, 1.0f);
    float v_hi = nak_pump_vnorm(-70.0f, 15.0f, 10.0f, 1.0f);
    INFO("v(K=0.2)=" << v_lo << " v(K=1.5)=" << v_mid << " v(K=10)=" << v_hi);
    REQUIRE(v_hi > v_mid);
    REQUIRE(v_mid > v_lo);
}

TEST_CASE("PUMP_NAK: v_norm — ATP-зависимость (нет ATP = нет насоса)", "[pump_nak]") {
    REQUIRE(nak_pump_vnorm(-70.0f, 15.0f, 5.0f, 0.0f) == Approx(0.0f).margin(1e-6f));
    float v_rest = nak_pump_vnorm(-70.0f, 15.0f, 5.0f, 1.0f);
    float v_high = nak_pump_vnorm(-70.0f, 15.0f, 5.0f, 5.0f);
    INFO("v(ATP=1)=" << v_rest << " v(ATP=5)=" << v_high);
    REQUIRE(v_high > v_rest);
}

TEST_CASE("PUMP_NAK: ток гиперполяризующий (возврат отрицательный)", "[pump_nak]") {
    Segment s = make_seg(-70.0f, 15.0f, 5.0f, 1.0f);
    float I = nak_pump_current(&s.modifiers[0], &s, 0.1f);
    INFO("I_pump=" << I);
    REQUIRE(I < 0.0f);   // электрогенный +1 наружу → гиперполяризация
}

TEST_CASE("PUMP_NAK: стехиометрия 3 Na⁺ наружу : 2 K⁺ внутрь", "[pump_nak]") {
    Segment s = make_seg(-70.0f, 15.0f, 5.0f, 1.0f);
    float Na_i0 = s.intra_ions[ION_NA];
    float K_i0  = s.intra_ions[ION_K];
    float Na_o0 = s.extra_ions[ION_NA];
    float K_o0  = s.extra_ions[ION_K];
    float ATP0  = s.ATP_level;

    // Большой dt: транспорт ионов накапливается (за один тик он ниже точности float).
    nak_pump_current(&s.modifiers[0], &s, 1000.0f);

    float dNa_i = s.intra_ions[ION_NA] - Na_i0;   // < 0 (Na наружу)
    float dK_i  = s.intra_ions[ION_K]  - K_i0;    // > 0 (K внутрь)
    float dNa_o = s.extra_ions[ION_NA] - Na_o0;   // > 0
    float dK_o  = s.extra_ions[ION_K]  - K_o0;    // < 0

    INFO("dNa_i=" << dNa_i << " dK_i=" << dK_i << " dNa_o=" << dNa_o << " dK_o=" << dK_o);
    REQUIRE(dNa_i < 0.0f);
    REQUIRE(dK_i  > 0.0f);
    REQUIRE(dNa_o > 0.0f);
    REQUIRE(dK_o  < 0.0f);

    // Стехиометрия: |ΔNa| : |ΔK| = 3 : 2 = 1.5
    REQUIRE(fabsf(dNa_i) == Approx(1.5f * fabsf(dK_i)).margin(1e-4f));

    // ATP расходуется (1 ATP на цикл)
    REQUIRE(s.ATP_level < ATP0);
}
