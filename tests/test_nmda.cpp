// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_nmda.cpp — верификация NMDA (MOD_NT_RECEPTOR_NMDA, 9).
// Mayer & Westbrook (1987): P_Ca/P_Na ≈ 10.6, P_K/P_Na ≈ 1, Mg²⁺-блок (Woodhull).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double nmda_current(Modifier* m, Segment* s, float dt);

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
    s.modifiers[0].type = MOD_NT_RECEPTOR_NMDA;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

TEST_CASE("NMDA: Mg²⁺-блок снимается при деполяризации", "[nmda]") {
    Segment s_neg = make_seg(-70.0f, 1.0f);
    Segment s_pos = make_seg(40.0f,  1.0f);

    float I_neg = 0.0f, I_pos = 0.0f;
    for (int i = 0; i < 20; i++) {
        I_neg = nmda_current(&s_neg.modifiers[0], &s_neg, 1.0f);
        I_pos = nmda_current(&s_pos.modifiers[0], &s_pos, 1.0f);
    }

    INFO("I(-70)=" << I_neg << " I(+40)=" << I_pos);
    // Блок силён при гиперполяризации, снят при деполяризации.
    REQUIRE(fabsf(I_pos) > fabsf(I_neg));
}

TEST_CASE("NMDA: Ca²⁺-проницаемость — E_rev > 0", "[nmda]") {
    // Высокая P_Ca тянет E_rev в плюс (~+10 мВ): при V=0 ток внутрь, при V=+40 наружу.
    Segment s_zero = make_seg(0.0f,  1.0f);
    Segment s_pos  = make_seg(40.0f, 1.0f);

    float I_zero = 0.0f, I_pos = 0.0f;
    for (int i = 0; i < 20; i++) {
        I_zero = nmda_current(&s_zero.modifiers[0], &s_zero, 1.0f);
        I_pos  = nmda_current(&s_pos.modifiers[0],  &s_pos,  1.0f);
    }

    INFO("I(0)=" << I_zero << " I(+40)=" << I_pos);
    REQUIRE(I_zero > 0.0f);   // V=0 < E_rev → внутрь
    REQUIRE(I_pos  < 0.0f);   // V=+40 > E_rev → наружу
}

TEST_CASE("NMDA: без глутамата ток ≈ 0", "[nmda]") {
    Segment s = make_seg(-70.0f, 0.0f);
    float I = 0.0f;
    for (int i = 0; i < 20; i++) {
        I = nmda_current(&s.modifiers[0], &s, 1.0f);
    }
    REQUIRE(fabsf(I) < 1e-3f);
}
