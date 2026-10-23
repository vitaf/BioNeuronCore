// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_pmca.cpp — верификация PMCA (MOD_PMCA, 98) против Ca²⁺-АТФазы плазмалеммы.
// Carafoli (1991): Hill-2, K_m=300нМ, 1 Ca²⁺ наружу : 1 ATP, электрогенный (+2 наружу).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double pmca_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float Ca_i_mM, float ATP) {
    Segment s = {};
    s.V_m = -70.0f;
    s.intra_ions[ION_CA] = Ca_i_mM;   // мМ ≡ моль/м³
    s.extra_ions[ION_CA] = 2.0f;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_CL] = 5.0f; s.extra_ions[ION_CL] = 130.0f;
    s.ATP_level = ATP;
    s.radius = 5.0f; s.length = 10.0f; s.segment_type = 1;   // сома
    s.modifiers[0].type = MOD_PMCA;
    s.modifiers[0].density = 300.0f;   // молекул/µm² (PMCA ~300)
    s.modifier_count = 1;
    return s;
}

TEST_CASE("PMCA: Hill-2 Ca²⁺-зависимость (K_m=300нМ)", "[pmca]") {
    // 100 нМ (базальный) → 300 нМ (K_m) → 1 µM (насыщение).
    Segment s100 = make_seg(0.0001f, 5.0f);
    Segment s300 = make_seg(0.0003f, 5.0f);
    Segment s1u  = make_seg(0.001f,  5.0f);
    float I100nM = pmca_current(&s100.modifiers[0], &s100, 0.0f);
    float I300nM = pmca_current(&s300.modifiers[0], &s300, 0.0f);
    float I1uM   = pmca_current(&s1u.modifiers[0],  &s1u,  0.0f);
    INFO("I(100nM)=" << I100nM << " I(300nM)=" << I300nM << " I(1uM)=" << I1uM);
    // Модуль тока растёт с Ca²⁺ (гиперполяризующий — знак отрицательный).
    REQUIRE(fabsf(I1uM)   > fabsf(I300nM));
    REQUIRE(fabsf(I300nM) > fabsf(I100nM));
    // K_m = 300 нМ: при Ca=K_m активность ≈ половина от насыщения.
    REQUIRE(fabsf(I300nM) == Approx(0.5f * fabsf(I1uM)).margin(0.06f * fabsf(I1uM)));
}

TEST_CASE("PMCA: ATP-зависимость (Michaelis, K_m=0.2мМ)", "[pmca]") {
    Segment s_lo = make_seg(0.001f, 0.2f);   // ATP = K_m → f_ATP ≈ 0.5
    Segment s_hi = make_seg(0.001f, 5.0f);   // ATP насыщающий
    float I_lo = pmca_current(&s_lo.modifiers[0], &s_lo, 0.0f);
    float I_hi = pmca_current(&s_hi.modifiers[0], &s_hi, 0.0f);
    INFO("I(ATP=0.2)=" << I_lo << " I(ATP=5)=" << I_hi);
    REQUIRE(fabsf(I_hi) > fabsf(I_lo));
}

TEST_CASE("PMCA: ток гиперполяризующий (возврат отрицательный)", "[pmca]") {
    Segment s = make_seg(0.001f, 5.0f);
    float I = pmca_current(&s.modifiers[0], &s, 0.0f);
    INFO("I_pmca=" << I);
    REQUIRE(I < 0.0f);   // электрогенный +2 наружу → гиперполяризация
}

TEST_CASE("PMCA: транспорт 1 Ca²⁺ наружу + расход ATP", "[pmca]") {
    Segment s = make_seg(0.001f, 5.0f);
    float Ca_i0 = s.intra_ions[ION_CA];
    float Ca_o0 = s.extra_ions[ION_CA];
    float ATP0  = s.ATP_level;

    pmca_current(&s.modifiers[0], &s, 1000.0f);   // большой dt — транспорт накапливается

    float dCa_i = s.intra_ions[ION_CA] - Ca_i0;   // < 0 (Ca наружу)
    float dCa_o = s.extra_ions[ION_CA] - Ca_o0;   // > 0
    INFO("dCa_i=" << dCa_i << " dCa_o=" << dCa_o << " dATP=" << (s.ATP_level - ATP0));
    REQUIRE(dCa_i < 0.0f);
    REQUIRE(dCa_o > 0.0f);
    REQUIRE(s.ATP_level < ATP0);   // 1 ATP на 1 Ca²⁺
}
