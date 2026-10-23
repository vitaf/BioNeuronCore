// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_girk.cpp — верификация метаботропного Gi/o → GIRK (Kir3.x).
// Инвард-ректификатор, γ≈35пС, двухфазная кинетика, RGS-десенситизация.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double gabab_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V, float gaba) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;   // мМ
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_CA] = 0.0001f; s.extra_ions[ION_CA] = 2.0f;
    s.intra_ions[ION_CL] = 5.0f; s.extra_ions[ION_CL] = 130.0f;
    s.neurotransmitter[NT_GABA] = gaba;
    s.ATP_level = 1.0f;
    s.radius = 5.0f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_NT_RECEPTOR_GABAB;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

TEST_CASE("GIRK: гиперполяризующий (K⁺-ток, return < 0)", "[girk]") {
    Segment s = make_seg(-40.0f, 1.0f);   // V=-40 > E_K → K⁺ выходит
    float I = 0.0f;
    for (int i = 0; i < 200; i++) I = gabab_current(&s.modifiers[0], &s, 1.0f);   // активация
    INFO("I(-40)=" << I);
    REQUIRE(I < 0.0f);
}

TEST_CASE("GIRK: инвард-ректификация (Woodhull-блок)", "[girk]") {
    // GIRK = Kir3: при гиперполяризации блок снимается, при деполяризации — сильный.
    float f_hyper = kir_f_open(-120.0f);
    float f_depol = kir_f_open(40.0f);
    INFO("f_open(-120)=" << f_hyper << " f_open(+40)=" << f_depol);
    REQUIRE(f_hyper > 0.9f);
    REQUIRE(f_depol < 0.15f);
}

TEST_CASE("GIRK: без GABA ток ≈ 0", "[girk]") {
    Segment s = make_seg(-40.0f, 0.0f);
    float I = 0.0f;
    for (int i = 0; i < 200; i++) I = gabab_current(&s.modifiers[0], &s, 1.0f);
    REQUIRE(fabsf(I) < 1e-3f);
}

TEST_CASE("GIRK: десенситизация (state[1] растёт при длительном агонисте)", "[girk]") {
    Segment s = make_seg(-40.0f, 1.0f);
    for (int i = 0; i < 1000; i++) gabab_current(&s.modifiers[0], &s, 1.0f);   // ~1с
    float d_early = s.modifiers[0].state[1];
    for (int i = 0; i < 40000; i++) gabab_current(&s.modifiers[0], &s, 1.0f);  // +40с
    float d_late = s.modifiers[0].state[1];
    INFO("d(early)=" << d_early << " d(late)=" << d_late);
    REQUIRE(d_late > d_early);   // десенситизация нарастает
}
