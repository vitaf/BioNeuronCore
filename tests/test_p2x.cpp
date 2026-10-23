// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_p2x.cpp — верификация P2X (MOD_NT_RECEPTOR_P2X, 15).
// ATP-зависимый катионный: P_K≈P_Na, P_Ca/P_Na≈2.5.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double p2x_current(Modifier* m, Segment* s, float dt);
extern double p2x2_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V, float atp, uint8_t type) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;   // мМ
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_CA] = 0.0001f; s.extra_ions[ION_CA] = 2.0f;
    s.intra_ions[ION_CL] = 5.0f; s.extra_ions[ION_CL] = 130.0f;
    s.neurotransmitter[NT_ATP] = atp;
    s.ATP_level = 1.0f;
    s.radius = 5.0f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = type;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

TEST_CASE("P2X: E_rev ≈ 0 (P_K≈P_Na)", "[p2x]") {
    Segment s_neg  = make_seg(-70.0f, 1.0f, MOD_NT_RECEPTOR_P2X);
    Segment s_zero = make_seg(0.0f,   1.0f, MOD_NT_RECEPTOR_P2X);
    Segment s_pos  = make_seg(40.0f,  1.0f, MOD_NT_RECEPTOR_P2X);

    float I_neg = 0.0f, I_zero = 0.0f, I_pos = 0.0f;
    for (int i = 0; i < 20; i++) {
        I_neg  = p2x_current(&s_neg.modifiers[0],  &s_neg,  1.0f);
        I_zero = p2x_current(&s_zero.modifiers[0], &s_zero, 1.0f);
        I_pos  = p2x_current(&s_pos.modifiers[0],  &s_pos,  1.0f);
    }
    INFO("I(-70)=" << I_neg << " I(0)=" << I_zero << " I(+40)=" << I_pos);
    REQUIRE(I_neg > 0.0f);
    REQUIRE(I_pos < 0.0f);
    REQUIRE(fabsf(I_zero) < 0.1f * fabsf(I_neg));
}

TEST_CASE("P2X: без ATP ток ≈ 0", "[p2x]") {
    Segment s = make_seg(-70.0f, 0.0f, MOD_NT_RECEPTOR_P2X);
    float I = 0.0f;
    for (int i = 0; i < 20; i++) I = p2x_current(&s.modifiers[0], &s, 1.0f);
    REQUIRE(fabsf(I) < 1e-3f);
}

TEST_CASE("P2X2: E_rev ≈ 0", "[p2x]") {
    Segment s_neg  = make_seg(-70.0f, 1.0f, MOD_P2X2);
    Segment s_zero = make_seg(0.0f,   1.0f, MOD_P2X2);
    Segment s_pos  = make_seg(40.0f,  1.0f, MOD_P2X2);

    float I_neg = 0.0f, I_zero = 0.0f, I_pos = 0.0f;
    for (int i = 0; i < 20; i++) {
        I_neg  = p2x2_current(&s_neg.modifiers[0],  &s_neg,  1.0f);
        I_zero = p2x2_current(&s_zero.modifiers[0], &s_zero, 1.0f);
        I_pos  = p2x2_current(&s_pos.modifiers[0],  &s_pos,  1.0f);
    }
    INFO("I(-70)=" << I_neg << " I(0)=" << I_zero << " I(+40)=" << I_pos);
    REQUIRE(I_neg > 0.0f);
    REQUIRE(I_pos < 0.0f);
    REQUIRE(fabsf(I_zero) < 0.1f * fabsf(I_neg));
}

TEST_CASE("P2X2: десенситизация медленнее P2X3", "[p2x]") {
    // 200мс: P2X3 (τ_desens=20мс) затух, P2X2 (τ_desens=5с) почти полный.
    Segment s3 = make_seg(-70.0f, 1.0f, MOD_NT_RECEPTOR_P2X);
    Segment s2 = make_seg(-70.0f, 1.0f, MOD_P2X2);
    float I3 = 0.0f, I2 = 0.0f;
    for (int i = 0; i < 200; i++) {
        I3 = p2x_current(&s3.modifiers[0], &s3, 1.0f);
        I2 = p2x2_current(&s2.modifiers[0], &s2, 1.0f);
    }
    INFO("I(P2X3)=" << I3 << " I(P2X2)=" << I2);
    REQUIRE(fabsf(I2) > fabsf(I3));   // P2X2 меньше десенситизировался
}
