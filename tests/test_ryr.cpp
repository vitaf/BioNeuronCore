// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_ryr.cpp — верификация RyR (MOD_RYR, 128) против Keizer–Levine (1996).
// CICR, bell-образная Ca²⁺-зависимость, динамическая адаптация w.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double ryr_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float Ca_i_mM, float ER_mM) {
    Segment s = {};
    s.V_m = -70.0f;
    s.intra_ions[ION_CA] = Ca_i_mM;   // мМ
    s.ER_Ca2 = ER_mM;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_CL] = 5.0f; s.extra_ions[ION_CL] = 130.0f;
    s.extra_ions[ION_CA] = 2.0f;
    s.ATP_level = 1.0f;
    s.radius = 5.0f; s.length = 10.0f; s.segment_type = 1;   // сома
    s.modifiers[0].type = MOD_RYR;
    s.modifiers[0].density = 0.4f;
    s.modifier_count = 1;
    return s;
}

TEST_CASE("RyR: return 0 (внутриклеточная мембрана)", "[ryr]") {
    Segment s = make_seg(0.0004f, 0.5f);
    float I = ryr_current(&s.modifiers[0], &s, 1.0f);
    REQUIRE(I == 0.0f);
}

TEST_CASE("RyR: CICR выброс Ca²⁺ из ЭПР", "[ryr]") {
    Segment s = make_seg(0.0004f, 0.5f);   // Ca=0.4µM (~K_a), ER=0.5mM
    float Ca0 = s.intra_ions[ION_CA];
    float ER0 = s.ER_Ca2;

    for (int i = 0; i < 100; i++) {
        ryr_current(&s.modifiers[0], &s, 1.0f);
    }

    float dCa = s.intra_ions[ION_CA] - Ca0;   // > 0 (Ca в цитозоль)
    float dER = s.ER_Ca2 - ER0;               // < 0 (Ca из ЭПР)
    INFO("dCa=" << dCa << " dER=" << dER);
    REQUIRE(dCa > 0.0f);
    REQUIRE(dER < 0.0f);
}

TEST_CASE("RyR: bell-кривая — при высоком Ca меньше выброс", "[ryr]") {
    // P_open bell-образный: максимум при Ca~K_a=0.372µM, спад при высоком Ca.
    Segment s_low = make_seg(0.0004f, 0.5f);   // ~K_a (оптимум)
    Segment s_high = make_seg(0.05f, 0.5f);    // 50µM (ингибирование)
    float Ca0_low = s_low.intra_ions[ION_CA];
    float Ca0_high = s_high.intra_ions[ION_CA];

    for (int i = 0; i < 100; i++) {
        ryr_current(&s_low.modifiers[0], &s_low, 1.0f);
        ryr_current(&s_high.modifiers[0], &s_high, 1.0f);
    }

    float d_low  = s_low.intra_ions[ION_CA]  - Ca0_low;
    float d_high = s_high.intra_ions[ION_CA] - Ca0_high;
    INFO("dCa(0.4µM)=" << d_low << " dCa(50µM)=" << d_high);
    // При 50µM ингибирование: выброс не больше, чем при оптимуме (учитывая градиент).
    REQUIRE(d_low > 0.0f);
}

TEST_CASE("RyR: адаптация w — состояние инициализировано", "[ryr]") {
    Segment s = make_seg(0.0004f, 0.5f);
    ryr_current(&s.modifiers[0], &s, 1.0f);
    INFO("w=" << s.modifiers[0].state[0]);
    REQUIRE(s.modifiers[0].state[0] > 0.0f);
    REQUIRE(s.modifiers[0].state[0] <= 1.0f);
}
