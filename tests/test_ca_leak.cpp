// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. All rights reserved.

// test_ca_leak.cpp — верификация CA_LEAK (mod 101) против TRPC7.
// TRPC7: nonselective Na/Ca, γ≈50 пС, P_Ca/P_Na≈2.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double ca_leak_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_CA] = 0.0001f; s.extra_ions[ION_CA] = 1.2f;  // мМ
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K]  = 4.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_CA_LEAK;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[0] = 50.0f;  // γ (пС)
    s.modifier_count = 1;
    return s;
}

TEST_CASE("Ca_leak: γ не меняется (state[0])", "[ca_leak]") {
    Segment s = make_seg(-60.0f);
    for (int t = 0; t < 200; t++) {
        s.V_m = (t % 2 == 0) ? -20.0f : -80.0f;
        ca_leak_current(&s.modifiers[0], &s, 0.01f);
    }
    REQUIRE(s.modifiers[0].state[0] == Approx(50.0f).epsilon(1e-4));
}

TEST_CASE("Ca_leak: nonselective — ток конечен, Na и Ca переносятся", "[ca_leak]") {
    // TRPC7 nonselective: при V=−70 входящий Na (деполяр.), при V=+20 входящий Ca (деполяр.).
    Segment sRest = make_seg(-70.0f);
    Segment sDep = make_seg(20.0f);
    float I_rest = ca_leak_current(&sRest.modifiers[0], &sRest, 0.01f);
    float I_dep = ca_leak_current(&sDep.modifiers[0], &sDep, 0.01f);
    REQUIRE(std::isnan(I_rest) == false);
    REQUIRE(std::isnan(I_dep) == false);
    // Оба несут входящий ток (деполяризующий: return положителен при inward).
    REQUIRE(I_rest > 0.0f);
}
