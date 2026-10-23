// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_nicotinic.cpp — верификация никотиновых ACh-рецепторов.
// α4β2 (13): низкий Ca²⁺, E_rev ≈ 0. α7 (97): высокий Ca²⁺ (P_Ca/P_Na≈6), E_rev > 0.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double nicotinic_current(Modifier* m, Segment* s, float dt);
extern double nicotinic_alpha7_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V, float ach, uint8_t type) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;   // мМ
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_CA] = 0.0001f; s.extra_ions[ION_CA] = 2.0f;
    s.intra_ions[ION_CL] = 5.0f; s.extra_ions[ION_CL] = 130.0f;
    s.neurotransmitter[NT_ACETYLCHOLINE] = ach;
    s.ATP_level = 1.0f;
    s.radius = 5.0f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = type;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

TEST_CASE("Nicotinic α4β2: E_rev ≈ 0 (низкий Ca²⁺)", "[nicotinic]") {
    Segment s_neg  = make_seg(-70.0f, 1.0f, MOD_NT_RECEPTOR_NICOTINIC);
    Segment s_zero = make_seg(0.0f,   1.0f, MOD_NT_RECEPTOR_NICOTINIC);
    Segment s_pos  = make_seg(40.0f,  1.0f, MOD_NT_RECEPTOR_NICOTINIC);

    float I_neg = 0.0f, I_zero = 0.0f, I_pos = 0.0f;
    for (int i = 0; i < 20; i++) {
        I_neg  = nicotinic_current(&s_neg.modifiers[0],  &s_neg,  1.0f);
        I_zero = nicotinic_current(&s_zero.modifiers[0], &s_zero, 1.0f);
        I_pos  = nicotinic_current(&s_pos.modifiers[0],  &s_pos,  1.0f);
    }
    INFO("I(-70)=" << I_neg << " I(0)=" << I_zero << " I(+40)=" << I_pos);
    REQUIRE(I_neg > 0.0f);
    REQUIRE(I_pos < 0.0f);
    REQUIRE(fabsf(I_zero) < 0.1f * fabsf(I_neg));   // реверс ~0 мВ
}

TEST_CASE("Nicotinic α7: возбуждающий (E_rev ≈ 0, Ca²⁺ в константах)", "[nicotinic]") {
    Segment s_neg  = make_seg(-70.0f, 1.0f, MOD_NICOTINIC_ALPHA7);
    Segment s_zero = make_seg(0.0f,   1.0f, MOD_NICOTINIC_ALPHA7);
    Segment s_pos  = make_seg(40.0f,  1.0f, MOD_NICOTINIC_ALPHA7);

    float I_neg = 0.0f, I_zero = 0.0f, I_pos = 0.0f;
    for (int i = 0; i < 20; i++) {
        I_neg  = nicotinic_alpha7_current(&s_neg.modifiers[0],  &s_neg,  1.0f);
        I_zero = nicotinic_alpha7_current(&s_zero.modifiers[0], &s_zero, 1.0f);
        I_pos  = nicotinic_alpha7_current(&s_pos.modifiers[0],  &s_pos,  1.0f);
    }
    INFO("I(-70)=" << I_neg << " I(0)=" << I_zero << " I(+40)=" << I_pos);
    REQUIRE(I_neg > 0.0f);
    REQUIRE(I_pos < 0.0f);
    REQUIRE(fabsf(I_zero) < 0.2f * fabsf(I_neg));   // реверс ~0 мВ (Na/K доминируют)
}

TEST_CASE("Nicotinic: без ACh ток ≈ 0", "[nicotinic]") {
    Segment s = make_seg(-70.0f, 0.0f, MOD_NT_RECEPTOR_NICOTINIC);
    float I = 0.0f;
    for (int i = 0; i < 20; i++) I = nicotinic_current(&s.modifiers[0], &s, 1.0f);
    REQUIRE(fabsf(I) < 1e-3f);
}
