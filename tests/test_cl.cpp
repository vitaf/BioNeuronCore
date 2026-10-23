// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_cl.cpp — верификация Cl-канала (mod 2) против CLC-2 утечки.
// OHM: I_Cl = g[С/см²]·(V−E_Cl), E_Cl = nernst(Cl_in, Cl_out, z=−1), всегда открыт.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double cl_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V, float density) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_CL] = 5.0f; s.extra_ions[ION_CL] = 130.0f;  // мМ
    s.radius = 5.0f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_IONOTROPIC_CL;
    s.modifiers[0].density = density;
    s.modifiers[0].state[1] = CLC2_GAMMA_PS;  // gCl (С/см²)
    s.modifier_count = 1;
    return s;
}

TEST_CASE("Cl: E_Cl реверс (nernst z=−1)", "[cl]") {
    float E_Cl = nernst(5.0f, 130.0f, -1.0f);
    REQUIRE(E_Cl == Approx(-87.0f).margin(1.0f));  // ≈ −87 мВ

    // При V = E_Cl ток ≈ 0 (канал всегда открыт).
    Segment s = make_seg(E_Cl, 1.0f);
    s.modifiers[0].state[0] = 1.0f;
    float I = cl_current(&s.modifiers[0], &s, 0.1f);
    REQUIRE(I == Approx(0.0f).margin(1e-4f));
}

TEST_CASE("Cl: знак — гиперполяризация при V > E_Cl", "[cl]") {
    // V=−65 > E_Cl=−87 → Cl⁻ входит (шунтирование) → return < 0 (гиперполяр.).
    Segment s = make_seg(-65.0f, 1.0f);
    s.modifiers[0].state[0] = 1.0f;
    REQUIRE(cl_current(&s.modifiers[0], &s, 0.1f) < 0.0f);
    // V=−95 < E_Cl → Cl⁻ выходит → return > 0 (деполяр.).
    Segment s2 = make_seg(-95.0f, 1.0f);
    s2.modifiers[0].state[0] = 1.0f;
    REQUIRE(cl_current(&s2.modifiers[0], &s2, 0.1f) > 0.0f);
}

TEST_CASE("Cl: OHM-масштаб — 2× density → 2× ток", "[cl]") {
    Segment s1 = make_seg(-65.0f, 1.0f);
    Segment s2 = make_seg(-65.0f, 2.0f);
    s1.modifiers[0].state[0] = 1.0f; s2.modifiers[0].state[0] = 1.0f;
    float I1 = cl_current(&s1.modifiers[0], &s1, 0.001f);
    float I2 = cl_current(&s2.modifiers[0], &s2, 0.001f);
    INFO("I(d=1)=" << I1 << " I(d=2)=" << I2);
    REQUIRE(I2 == Approx(2.0f * I1).margin(0.02f * fabsf(I1) + 1e-12f));
}

TEST_CASE("Cl: парадигма — всегда открыт (state[0]=1)", "[cl]") {
    Segment s = make_seg(-65.0f, 1.0f);
    for (int t = 0; t < 100; t++) cl_current(&s.modifiers[0], &s, 0.1f);
    REQUIRE(s.modifiers[0].state[0] == Approx(1.0f).margin(1e-6f));
}

TEST_CASE("Cl: ion flux — Cl⁻ внутрь при V>E_Cl (intra_Cl растёт)", "[cl]") {
    Segment s = make_seg(-65.0f, 1000.0f);  // большой density → заметный поток
    s.modifiers[0].state[0] = 1.0f;
    float cl0 = s.intra_ions[ION_CL];
    for (int t = 0; t < 20; t++) cl_current(&s.modifiers[0], &s, 0.1f);
    REQUIRE(s.intra_ions[ION_CL] > cl0);  // Cl⁻ вошёл
}
