// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_kv42.cpp — верификация MOD_KV42 (ID 85) против Kv4.2 (KCND2, A-type).
// Kv4.2: γ≈5 пС, активация m³ (V½=−10 мВ), быстрая инактивация V½=−65 мВ.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double kv42_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_KV42;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[2] = 5.0f;  // γ (пС)
    s.modifier_count = 1;
    return s;
}

TEST_CASE("Kv4.2: γ не меняется (state[2])", "[kv42]") {
    Segment s = make_seg(-60.0f);
    for (int t = 0; t < 200; t++) {
        s.V_m = (t % 2 == 0) ? 30.0f : -60.0f;
        kv42_current(&s.modifiers[0], &s, 0.01f);
    }
    REQUIRE(s.modifiers[0].state[2] == Approx(5.0f).epsilon(1e-4));  // γ
}

TEST_CASE("Kv4.2: m-гейт активация (V½=−10)", "[kv42]") {
    // При V=+30 (деполяризация) m_inf ≈ 0.951, m релаксирует к нему.
    Segment s = make_seg(30.0f);
    s.modifiers[0].state[0] = 0.0f;  // m=0 старт
    for (int t = 0; t < 300; t++) kv42_current(&s.modifiers[0], &s, 0.1f);  // 30 мс = 20·tau_m
    float m_inf = 1.0f / (1.0f + expf(-(30.0f + 10.0f) / 13.5f));
    INFO("m=" << s.modifiers[0].state[0] << " m_inf=" << m_inf);
    REQUIRE(s.modifiers[0].state[0] == Approx(m_inf).margin(0.02f));
}

TEST_CASE("Kv4.2: h-гейт инактивация (V½=−65)", "[kv42]") {
    // Гиперполяризация (−90) → h близко к 1; деполяризация (−30) → h → 0.
    Segment s_hyper = make_seg(-90.0f);
    Segment s_depol = make_seg(-30.0f);
    for (int t = 0; t < 10000; t++) {  // 1000 мс ≈ 6·τ_h_slow (τ_h V-зависимый: 15 мс деполяр., 170 мс гиперполяр.)
        kv42_current(&s_hyper.modifiers[0], &s_hyper, 0.1f);
        kv42_current(&s_depol.modifiers[0], &s_depol, 0.1f);
    }
    float h_inf_hyper = 1.0f / (1.0f + expf((-90.0f + 65.0f) / 8.0f));
    float h_inf_depol = 1.0f / (1.0f + expf((-30.0f + 65.0f) / 8.0f));
    INFO("h_hyper=" << s_hyper.modifiers[0].state[1] << " exp=" << h_inf_hyper
         << " h_depol=" << s_depol.modifiers[0].state[1] << " exp=" << h_inf_depol);
    REQUIRE(s_hyper.modifiers[0].state[1] == Approx(h_inf_hyper).margin(0.03f));
    REQUIRE(s_depol.modifiers[0].state[1] == Approx(h_inf_depol).margin(0.03f));
    REQUIRE(s_depol.modifiers[0].state[1] < s_hyper.modifiers[0].state[1]);
}
