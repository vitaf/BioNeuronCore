// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_nak_pump_a3.cpp — верификация Na/K-АТФазы α3 (MOD_PUMP_NAK_A3, 197).
// НИЗКОЕ сродство к Na⁺_i (K_Na=25 мМ) → динамический насос usAHP (Zahler 1997, Blanco 2005).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

static Segment make_seg(float Na_i, float K_o) {
    Segment s = {};
    s.V_m = -57.0f;
    s.intra_ions[ION_NA] = Na_i;
    s.intra_ions[ION_K]  = 140.0f;
    s.extra_ions[ION_NA] = 140.0f;
    s.extra_ions[ION_K]  = K_o;
    s.intra_ions[ION_CA] = 0.0001f;
    s.extra_ions[ION_CA] = 2.0f;
    s.intra_ions[ION_CL] = 5.0f;
    s.extra_ions[ION_CL] = 130.0f;
    s.ATP_level = 1.0f;
    s.radius = 5.0f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_PUMP_NAK_A3;
    s.modifiers[0].density = 1000.0f;
    s.modifier_count = 1;
    return s;
}

TEST_CASE("NAK_PUMP_A3: электрогенный (гиперполяризующий, ток наружу)", "[nak_pump_a3]") {
    Segment s = make_seg(15.0f, 3.0f);
    double I = nak_pump_a3_current(&s.modifiers[0], &s, 0.1f);
    REQUIRE(I < 0.0f);   // return -I_pump → гиперполяризующий
}

TEST_CASE("NAK_PUMP_A3: низкое сродство к Na⁺ → usAHP (рост тока при залпе)", "[nak_pump_a3]") {
    // α3 почти неактивна на покое ([Na⁺]_i=15), активируется при залпе ([Na⁺]_i=30).
    Segment s_rest  = make_seg(15.0f, 3.0f);
    Segment s_burst = make_seg(30.0f, 3.0f);
    double I_rest  = nak_pump_a3_current(&s_rest.modifiers[0],  &s_rest,  0.1f);
    double I_burst = nak_pump_a3_current(&s_burst.modifiers[0], &s_burst, 0.1f);
    REQUIRE(fabs(I_burst) > fabs(I_rest));   // usAHP
    // цикл Поста–Альберса насыщается: рост ~1.3× (не пропорционален f_Na)
    REQUIRE(fabs(I_burst) > 1.2f * fabs(I_rest));
}

TEST_CASE("NAK_PUMP_A3: K_Na=25 ниже активности α1 на покое", "[nak_pump_a3]") {
    // α3 (K_Na=25) на покое [Na⁺]_i=15 менее активна, чем α1 (K_Na=10).
    float v_a3 = nak_pump_vnorm(-57.0f, 15.0f, 3.0f, 1.0f, NA_K_PUMP_A3_K_NA_MM, NA_K_PUMP_K_K_MM);
    float v_a1 = nak_pump_vnorm(-57.0f, 15.0f, 3.0f, 1.0f, NA_K_PUMP_K_NA_MM,     NA_K_PUMP_K_K_MM);
    REQUIRE(v_a3 < v_a1);
}

TEST_CASE("NAK_PUMP_A3: уабаин-ингибиция (state[5], IC50=20 нМ)", "[nak_pump_a3]") {
    Segment s = make_seg(30.0f, 3.0f);
    double I0 = nak_pump_a3_current(&s.modifiers[0], &s, 0.1f);
    Segment s2 = make_seg(30.0f, 3.0f);
    s2.modifiers[0].state[5] = 20.0f;   // [уабаин] = IC50 → ток вдвое
    double I_oua = nak_pump_a3_current(&s2.modifiers[0], &s2, 0.1f);
    REQUIRE(fabs(I_oua) < fabs(I0));
    REQUIRE(fabs(I_oua) > 0.3f * fabs(I0));   // частичное, не нулевое блокирование
}

TEST_CASE("NAK_PUMP_A3: стехиометрия 3 Na : 2 K", "[nak_pump_a3]") {
    Segment s = make_seg(30.0f, 3.0f);
    float na0 = s.intra_ions[ION_NA], k0 = s.intra_ions[ION_K];
    nak_pump_a3_current(&s.modifiers[0], &s, 1.0f);
    float dNa = na0 - s.intra_ions[ION_NA];   // Na вышел
    float dK  = s.intra_ions[ION_K] - k0;     // K вошёл
    REQUIRE(dNa > 0.0f);
    REQUIRE(dK  > 0.0f);
    REQUIRE(fabs(dNa / dK - 1.5f) < 0.01f);  // 3:2
}

TEST_CASE("NAK_PUMP_A3: ATP-зависимость (ATP=0 → ток 0)", "[nak_pump_a3]") {
    Segment s = make_seg(30.0f, 3.0f);
    s.ATP_level = 0.0f;
    double I = nak_pump_a3_current(&s.modifiers[0], &s, 0.1f);
    REQUIRE(fabs(I) < 1e-9);   // без ATP фосфорилирование E1→E1P невозможно → ток 0
}
