// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_ampa.cpp — верификация AMPA (MOD_NT_RECEPTOR_AMPA, 8).
// GluA2-содержащий (Traynelis 2010): P_Na:P_K ≈ 1:1 → E_rev ≈ 0 мВ, Ca²⁺-непроницаем.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double ampa_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V, float glu) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;   // мМ
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_CA] = 0.0001f; s.extra_ions[ION_CA] = 2.0f;
    s.intra_ions[ION_CL] = 5.0f; s.extra_ions[ION_CL] = 130.0f;
    s.neurotransmitter[NT_GLUTAMATE] = glu;
    s.ATP_level = 1.0f;
    s.radius = 5.0f; s.length = 10.0f; s.segment_type = 1;   // сома
    s.modifiers[0].type = MOD_NT_RECEPTOR_AMPA;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

TEST_CASE("AMPA: E_rev ≈ 0 мВ (P_Na:P_K ≈ 1:1)", "[ampa]") {
    Segment s_neg  = make_seg(-70.0f, 1.0f);
    Segment s_zero = make_seg(0.0f,   1.0f);
    Segment s_pos  = make_seg(40.0f,  1.0f);

    float I_neg = 0.0f, I_zero = 0.0f, I_pos = 0.0f;
    for (int i = 0; i < 20; i++) {   // активация r → r_inf
        I_neg  = ampa_current(&s_neg.modifiers[0],  &s_neg,  1.0f);
        I_zero = ampa_current(&s_zero.modifiers[0], &s_zero, 1.0f);
        I_pos  = ampa_current(&s_pos.modifiers[0],  &s_pos,  1.0f);
    }

    INFO("I(-70)=" << I_neg << " I(0)=" << I_zero << " I(+40)=" << I_pos);
    REQUIRE(I_neg > 0.0f);    // возбуждающий (внутрь) при гиперполяризации
    REQUIRE(I_pos < 0.0f);    // наружу при деполяризации
    REQUIRE(fabsf(I_zero) < 0.1f * fabsf(I_neg));   // реверс ~0 мВ
}

TEST_CASE("AMPA: без глутамата ток ≈ 0", "[ampa]") {
    Segment s = make_seg(-70.0f, 0.0f);
    float I = 0.0f;
    for (int i = 0; i < 20; i++) {
        I = ampa_current(&s.modifiers[0], &s, 1.0f);
    }
    INFO("I(нет глутамата)=" << I);
    REQUIRE(fabsf(I) < 1e-3f);
}

TEST_CASE("AMPA: десенситизация при длительном агонисте", "[ampa]") {
    Segment s = make_seg(-70.0f, 1.0f);   // длительный глутамат
    float I_early = 0.0f, I_late = 0.0f;
    for (int i = 0; i < 10; i++) I_early = ampa_current(&s.modifiers[0], &s, 1.0f);    // ~10мс
    for (int i = 0; i < 100; i++) I_late  = ampa_current(&s.modifiers[0], &s, 1.0f);   // +100мс
    INFO("I(early)=" << I_early << " I(late)=" << I_late);
    REQUIRE(fabsf(I_late) < fabsf(I_early));   // десенситизация затухает ток
}
