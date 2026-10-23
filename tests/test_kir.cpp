// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. All rights reserved. All rights reserved.

// test_kir.cpp — верификация Kir (mod 106) против Kir2.1.
// Kir2.1: γ≈30 пС, inward-rectification = Woodhull Mg²⁺/spermine блок.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double kir_channel_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 4.0f;  // мМ
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_KIR_CHANNEL;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[3] = 30.0f;  // γ (пС)
    s.modifier_count = 1;
    return s;
}

TEST_CASE("Kir: параметры — γ не меняется (нет гейтинга)", "[kir]") {
    Segment s = make_seg(-80.0f);
    for (int t = 0; t < 200; t++) {
        s.V_m = (t % 2 == 0) ? -120.0f : 40.0f;
        kir_channel_current(&s.modifiers[0], &s, 0.01f);
    }
    REQUIRE(s.modifiers[0].state[3] == Approx(30.0f).epsilon(1e-4));  // γ
}

TEST_CASE("Kir: динамика — inward rectification (f_open(−120) >> f_open(+40))", "[kir]") {
    // Woodhull блок: при гиперполяризации блок снимается (f_open≈1),
    // при деполяризации блок сильный (f_open мал).
    float f_hyper = kir_f_open(-120.0f);
    float f_depol = kir_f_open(40.0f);
    INFO("f_open(-120)=" << f_hyper << " f_open(+40)=" << f_depol);
    REQUIRE(f_hyper > 0.9f);          // inward почти свободен
    REQUIRE(f_depol < 0.15f);         // outward сильно подавлен
    REQUIRE(f_hyper > f_depol * 5.0f);
}

TEST_CASE("Kir: динамика — знак (return = −I_K, гиперполяризация при outward)", "[kir]") {
    // При деполяризации (outward K) return отрицателен (гиперполяризация).
    Segment sOut = make_seg(40.0f);
    float I_out = kir_channel_current(&sOut.modifiers[0], &sOut, 0.01f);
    REQUIRE(I_out < 0.0f);  // гиперполяризация (return = −I_K)
}
