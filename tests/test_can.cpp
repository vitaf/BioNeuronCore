// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_can.cpp — верификация MOD_CAN (ID 152) против CAN (TRPM4/5).
// CAN: Ca²⁺-активируемый неселективный Na⁺/K⁺, Ca²⁺-десенситизация, mixed GHK Na/K.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double can_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V, float ca) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_CA] = ca;   s.extra_ions[ION_CA] = 1.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_CAN;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

TEST_CASE("CAN: Ca²⁺-зависимый Po — без Ca²⁺ нет тока", "[can]") {
    Segment s = make_seg(-60.0f, 0.0f);
    float I = can_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(fabsf(I) < 0.01f);
}

TEST_CASE("CAN: Ca²⁺ открывает канал — есть ток", "[can]") {
    Segment s = make_seg(-60.0f, 0.1f);
    float I = can_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(fabsf(I) > 0.01f);
}

TEST_CASE("CAN: Ca²⁺-десенситизация — ток падает при длительном Ca²⁺", "[can]") {
    Segment s = make_seg(-60.0f, 0.1f);
    s.modifiers[0].state[1] = 0.0f;
    float I_first = can_current(&s.modifiers[0], &s, 1.0f);
    for (int t = 0; t < 5000; t++) can_current(&s.modifiers[0], &s, 1.0f);  // 5 с → десенситизация
    float I_late = can_current(&s.modifiers[0], &s, 1.0f);
    REQUIRE(fabsf(I_late) < fabsf(I_first));
}

TEST_CASE("CAN: Na⁺ входит при деполяризации (неселективный)", "[can]") {
    Segment s = make_seg(-20.0f, 0.1f);
    float na_before = s.intra_ions[ION_NA];
    can_current(&s.modifiers[0], &s, 0.1f);
    REQUIRE(s.intra_ions[ION_NA] > na_before);
}

TEST_CASE("CAN: НЕ проводит Ca²⁺ (intra_Ca неизменен)", "[can]") {
    Segment s = make_seg(-20.0f, 0.1f);
    float ca_before = s.intra_ions[ION_CA];
    can_current(&s.modifiers[0], &s, 0.1f);
    REQUIRE(s.intra_ions[ION_CA] == Approx(ca_before).margin(1e-6f));
}
