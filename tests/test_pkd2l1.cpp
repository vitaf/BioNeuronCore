// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_pkd2l1.cpp — верификация MOD_PKD2L1 (ID 153) против PKD2L1 (TRPP3).
// PKD2L1: Ca²⁺-предпочитающий механосенсор CSF, механо-адаптация, mixed GHK Ca/Na.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double pkd2l1_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_CA] = 0.0001f; s.extra_ions[ION_CA] = 1.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_PKD2L1;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

TEST_CASE("PKD2L1: закрыт без механо-стимула (Po=0)", "[pkd2l1]") {
    Segment s = make_seg(-60.0f);
    s.modifiers[0].state[4] = 0.0f;
    float I = pkd2l1_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(fabsf(I) < 0.01f);
}

TEST_CASE("PKD2L1: открыт при механо-стимуле (Po=1)", "[pkd2l1]") {
    Segment s = make_seg(-60.0f);
    s.modifiers[0].state[4] = 1.0f;
    float I = pkd2l1_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(fabsf(I) > 0.01f);
}

TEST_CASE("PKD2L1: механо-адаптация — ток падает при длительном стимуле", "[pkd2l1]") {
    Segment s = make_seg(-60.0f);
    s.modifiers[0].state[4] = 1.0f;
    s.modifiers[0].state[1] = 0.0f;
    float I_first = pkd2l1_current(&s.modifiers[0], &s, 1.0f);
    for (int t = 0; t < 3000; t++) pkd2l1_current(&s.modifiers[0], &s, 1.0f);  // 3 с → адаптация
    float I_late = pkd2l1_current(&s.modifiers[0], &s, 1.0f);
    REQUIRE(fabsf(I_late) < fabsf(I_first));
}

TEST_CASE("PKD2L1: Ca²⁺ входит (Ca-предпочитающий)", "[pkd2l1]") {
    Segment s = make_seg(-60.0f);
    s.modifiers[0].state[4] = 1.0f;
    float ca_before = s.intra_ions[ION_CA];
    pkd2l1_current(&s.modifiers[0], &s, 0.1f);
    REQUIRE(s.intra_ions[ION_CA] > ca_before);
}
