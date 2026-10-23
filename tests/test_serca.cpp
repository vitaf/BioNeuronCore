// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_serca.cpp — верификация SERCA (MOD_SERCA, 126) против Ca²⁺-АТФазы ЭПР.
// Verkhratsky (2005), Lytton (1992): 2 Ca²⁺ : 1 ATP, Hill-2, K_m=0.27µM, закачивает в ЭПР.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double serca_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float Ca_i_mM, float ATP) {
    Segment s = {};
    s.V_m = -70.0f;
    s.intra_ions[ION_CA] = Ca_i_mM;   // мМ
    s.ER_Ca2 = 0.1f;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_CL] = 5.0f; s.extra_ions[ION_CL] = 130.0f;
    s.extra_ions[ION_CA] = 2.0f;
    s.ATP_level = ATP;
    s.radius = 5.0f; s.length = 10.0f; s.segment_type = 1;   // сома
    s.modifiers[0].type = MOD_SERCA;
    s.modifiers[0].density = 700.0f;   // молекул/µm² (SERCA ER ~700, Frontiers Immunol 2013)
    s.modifier_count = 1;
    return s;
}

TEST_CASE("SERCA: return 0 (внутриклеточная мембрана, нет тока)", "[serca]") {
    Segment s = make_seg(0.001f, 5.0f);
    float I = serca_current(&s.modifiers[0], &s, 1.0f);
    REQUIRE(I == 0.0f);
}

TEST_CASE("SERCA: транспорт Ca²⁺ из цитозоля в ЭПР", "[serca]") {
    Segment s = make_seg(0.001f, 5.0f);   // Ca=1µM (выше K_m)
    float Ca_i0 = s.intra_ions[ION_CA];
    float ER0   = s.ER_Ca2;

    serca_current(&s.modifiers[0], &s, 1.0f);

    float dCa_i = s.intra_ions[ION_CA] - Ca_i0;   // < 0 (Ca из цитозоля)
    float dER   = s.ER_Ca2 - ER0;                 // > 0 (Ca в ЭПР)
    INFO("dCa_i=" << dCa_i << " dER=" << dER);
    REQUIRE(dCa_i < 0.0f);
    REQUIRE(dER   > 0.0f);
    REQUIRE(fabsf(dCa_i) == Approx(fabsf(dER)).margin(1e-4f));   // сохраняется масса
}

TEST_CASE("SERCA: ATP=0 → нет транспорта", "[serca]") {
    Segment s = make_seg(0.001f, 0.0f);
    float Ca_i0 = s.intra_ions[ION_CA];

    serca_current(&s.modifiers[0], &s, 1.0f);

    REQUIRE(s.intra_ions[ION_CA] == Approx(Ca_i0).margin(1e-6f));   // без ATP не качает
}

TEST_CASE("SERCA: Hill-2 K_m=0.27µM (выше Ca → быстрее)", "[serca]") {
    Segment s_lo = make_seg(0.00005f, 5.0f);   // 50 нМ (ниже K_m)
    Segment s_hi = make_seg(0.001f, 5.0f);     // 1 µM (выше K_m)
    float Ca0_lo = s_lo.intra_ions[ION_CA];
    float Ca0_hi = s_hi.intra_ions[ION_CA];

    serca_current(&s_lo.modifiers[0], &s_lo, 1.0f);
    serca_current(&s_hi.modifiers[0], &s_hi, 1.0f);

    float d_lo = Ca0_lo - s_lo.intra_ions[ION_CA];   // закачано при низком Ca
    float d_hi = Ca0_hi - s_hi.intra_ions[ION_CA];   // закачано при высоком Ca
    INFO("dCa(50nM)=" << d_lo << " dCa(1uM)=" << d_hi);
    REQUIRE(d_hi > d_lo);   // Hill-2: выше Ca → быстрее закачка
}

TEST_CASE("SERCA: ATP-зависимость (выше ATP → быстрее)", "[serca]") {
    Segment s_lo = make_seg(0.001f, 0.1f);
    Segment s_hi = make_seg(0.001f, 5.0f);
    float Ca0_lo = s_lo.intra_ions[ION_CA];
    float Ca0_hi = s_hi.intra_ions[ION_CA];

    serca_current(&s_lo.modifiers[0], &s_lo, 1.0f);
    serca_current(&s_hi.modifiers[0], &s_hi, 1.0f);

    float d_lo = Ca0_lo - s_lo.intra_ions[ION_CA];
    float d_hi = Ca0_hi - s_hi.intra_ions[ION_CA];
    INFO("dCa(ATP=0.1)=" << d_lo << " dCa(ATP=5)=" << d_hi);
    REQUIRE(d_hi > d_lo);
}
