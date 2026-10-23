// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_task.cpp — верификация MOD_TASK (ID 151) против TASK (KCNK3/9, фоновая K⁺-утечка).
// TASK: γ≈14–38 пС, voltage-independent Po~0.3, pH-ингибиция (IC50 pH 6.7), GHK K⁺.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double task_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_TASK;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

TEST_CASE("TASK: фоновый K-выход при V > E_K", "[task]") {
    Segment s = make_seg(-60.0f);
    float I = task_current(&s.modifiers[0], &s, 0.1f);
    REQUIRE(I < 0.0f);
}

TEST_CASE("TASK: pH-ингибиция — кислый pH снижает ток", "[task]") {
    Segment s_phys = make_seg(-60.0f);  // pH 7.4 (по умолчанию)
    Segment s_acid = make_seg(-60.0f);
    s_acid.modifiers[0].state[4] = 6.0f;  // кислый pH
    float I_phys = task_current(&s_phys.modifiers[0], &s_phys, 0.0f);
    float I_acid = task_current(&s_acid.modifiers[0], &s_acid, 0.0f);
    INFO("I_phys=" << I_phys << " I_acid=" << I_acid);
    REQUIRE(fabsf(I_acid) < fabsf(I_phys));  // кислый → меньше ток
}

TEST_CASE("TASK: E_rev — знак меняется у E_K", "[task]") {
    Segment s_out = make_seg(-60.0f);
    Segment s_in = make_seg(-110.0f);
    float I_out = task_current(&s_out.modifiers[0], &s_out, 0.0f);
    float I_in = task_current(&s_in.modifiers[0], &s_in, 0.0f);
    REQUIRE(I_out < 0.0f);
    REQUIRE(I_in > 0.0f);
}

TEST_CASE("TASK: GHK выпрямление — |I| больше при деполяризации", "[task]") {
    float E_K = 26.7f * logf(4.0f / 140.0f);
    Segment s_plus = make_seg(E_K + 40.0f);
    Segment s_minus = make_seg(E_K - 40.0f);
    float I_plus = task_current(&s_plus.modifiers[0], &s_plus, 0.0f);
    float I_minus = task_current(&s_minus.modifiers[0], &s_minus, 0.0f);
    REQUIRE(fabsf(I_plus) > fabsf(I_minus));
}

TEST_CASE("TASK: K⁺ консервация", "[task]") {
    Segment s = make_seg(-60.0f);
    float k_before = s.intra_ions[ION_K];
    task_current(&s.modifiers[0], &s, 0.1f);
    REQUIRE(s.intra_ions[ION_K] < k_before);
}
