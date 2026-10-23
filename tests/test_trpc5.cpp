// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_trpc5.cpp — верификация MOD_TRPC5 (ID 163) против TRPC5.
// TRPC5: неселективный катионный (Na⁺/Ca²⁺), Gq-зависимый (state[4]) + Ca²⁺-зависимый
// (EC50 0.5 µМ), фармакология HC-070 (IC50 9.3 нМ, state[5]).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double trpc5_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_CA] = 0.01f; s.extra_ions[ION_CA] = 2.0f;  // 10 µМ → активация
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_TRPC5;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[4] = 1.0f;  // Gq = 1
    s.modifier_count = 1;
    return s;
}

TEST_CASE("TRPC5: Gq+Ca²⁺ зависимость (state[4]=1+высокий Ca → ток; state[4]=0 → 0)", "[trpc5]") {
    Segment s = make_seg(-60.0f);
    float I_on = trpc5_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(fabsf(I_on) > 0.01f);

    Segment s2 = make_seg(-60.0f);
    s2.modifiers[0].state[4] = 0.0f;
    float I_off = trpc5_current(&s2.modifiers[0], &s2, 0.0f);
    REQUIRE(fabsf(I_off) < 0.01f);
}

TEST_CASE("TRPC5: E_rev (вход при V<E_rev, выход при V>E_rev)", "[trpc5]") {
    Segment lo = make_seg(-60.0f);
    Segment hi = make_seg(100.0f);
    float I_lo = trpc5_current(&lo.modifiers[0], &lo, 0.0f);
    float I_hi = trpc5_current(&hi.modifiers[0], &hi, 0.0f);
    REQUIRE(I_lo > 0.0f);   // внутрь (деполяризующий)
    REQUIRE(I_hi < 0.0f);   // наружу (гиперполяризующий)
}

TEST_CASE("TRPC5: HC-070 блок (state[5]=93 нМ = 10×IC50)", "[trpc5]") {
    Segment open = make_seg(-60.0f);
    open.modifiers[0].state[5] = 0.0f;
    float I_open = trpc5_current(&open.modifiers[0], &open, 0.0f);

    Segment blocked = make_seg(-60.0f);
    blocked.modifiers[0].state[5] = 93.0f;  // 10×IC50 → Po/11
    float I_blocked = trpc5_current(&blocked.modifiers[0], &blocked, 0.0f);

    REQUIRE(I_open > 0.01f);
    float ratio = I_blocked / I_open;
    REQUIRE(ratio > 0.05f);   // ≈ 1/11
    REQUIRE(ratio < 0.2f);
}

TEST_CASE("TRPC5: консервация Na⁺ (вход + суммарное сохранение)", "[trpc5]") {
    Segment s = make_seg(-60.0f);
    float intra_before = s.intra_ions[ION_NA];
    float total_before = s.intra_ions[ION_NA] + EXTRA_VOLUME_RATIO * s.extra_ions[ION_NA];
    trpc5_current(&s.modifiers[0], &s, 0.1f);
    float total_after = s.intra_ions[ION_NA] + EXTRA_VOLUME_RATIO * s.extra_ions[ION_NA];
    REQUIRE(s.intra_ions[ION_NA] > intra_before);         // Na⁺ входит (внутрь)
    REQUIRE(fabsf(total_after - total_before) < 0.001f);  // суммарный Na⁺ сохранён (объём ×10)
}
