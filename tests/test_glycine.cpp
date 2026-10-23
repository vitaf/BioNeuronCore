// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_glycine.cpp — верификация глицинового рецептора (MOD_NT_RECEPTOR_GLYCINE, 12).
// Cl⁻-анионный канал: тормозной, очень быстрый.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double glycine_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V, float gly) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;   // мМ
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_CA] = 0.0001f; s.extra_ions[ION_CA] = 2.0f;
    s.intra_ions[ION_CL] = 5.0f; s.extra_ions[ION_CL] = 130.0f;    // E_Cl ≈ -87 мВ
    s.neurotransmitter[NT_GLYCINE] = gly;
    s.ATP_level = 1.0f;
    s.radius = 5.0f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_NT_RECEPTOR_GLYCINE;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

TEST_CASE("Glycine: Cl⁻-ток гиперполяризующий (return < 0 при V > E_Cl)", "[glycine]") {
    Segment s = make_seg(-40.0f, 1.0f);
    float I = 0.0f;
    for (int i = 0; i < 20; i++) I = glycine_current(&s.modifiers[0], &s, 1.0f);
    INFO("I(-40)=" << I);
    REQUIRE(I < 0.0f);
}

TEST_CASE("Glycine: без глицина ток ≈ 0", "[glycine]") {
    Segment s = make_seg(-40.0f, 0.0f);
    float I = 0.0f;
    for (int i = 0; i < 20; i++) I = glycine_current(&s.modifiers[0], &s, 1.0f);
    REQUIRE(fabsf(I) < 1e-3f);
}
