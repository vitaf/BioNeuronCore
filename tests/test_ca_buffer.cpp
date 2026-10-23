// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_ca_buffer.cpp — верификация Ca²⁺-буфера (MOD_CA_BUFFER, 102).
// Schwaller (2010): обратимое связывание Ca²⁺ + B ⇌ CaB, K_d=500нМ, B_total=150µM.
// density масштабирует ЁМКОСТЬ (B_total), не кинетику.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double ca_buffer_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float Ca_i_mM, float density) {
    Segment s = {};
    s.V_m = -70.0f;
    s.intra_ions[ION_CA] = Ca_i_mM;   // мМ
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_CL] = 5.0f; s.extra_ions[ION_CL] = 130.0f;
    s.extra_ions[ION_CA] = 2.0f;
    s.ATP_level = 1.0f;
    s.radius = 5.0f; s.length = 10.0f; s.segment_type = 1;   // сома
    s.modifiers[0].type = MOD_CA_BUFFER;
    s.modifiers[0].density = density;
    s.modifier_count = 1;
    return s;
}

TEST_CASE("CA_BUFFER: return 0 (не электрогенный)", "[ca_buffer]") {
    Segment s = make_seg(0.001f, 1.0f);
    float I = ca_buffer_current(&s.modifiers[0], &s, 1.0f);
    REQUIRE(I == 0.0f);
}

TEST_CASE("CA_BUFFER: равновесие — Ca_bound = B_total·Ca/(Ca+K_d)", "[ca_buffer]") {
    Segment s = make_seg(0.001f, 1.0f);   // Ca=1µM, density=1
    ca_buffer_current(&s.modifiers[0], &s, 0.1f);

    float B_total = 0.15f;                 // 150 µM (density=1)
    float Ca_free = 0.001f;                // 1 µM
    float K_d = 0.0005f;                   // 500 нМ
    float expected = B_total * Ca_free / (Ca_free + K_d);

    INFO("Ca_bound=" << s.modifiers[0].state[1] << " expected=" << expected);
    REQUIRE(s.modifiers[0].state[1] == Approx(expected).margin(1e-3f));
}

TEST_CASE("CA_BUFFER: ёмкость B_total ∝ density", "[ca_buffer]") {
    Segment s1 = make_seg(0.001f, 1.0f);
    Segment s2 = make_seg(0.001f, 2.0f);
    ca_buffer_current(&s1.modifiers[0], &s1, 0.1f);
    ca_buffer_current(&s2.modifiers[0], &s2, 0.1f);

    INFO("Ca_bound(d=1)=" << s1.modifiers[0].state[1] << " Ca_bound(d=2)=" << s2.modifiers[0].state[1]);
    // density=2 → ёмкость вдвое больше → равновесный Ca_bound ~2×.
    REQUIRE(s2.modifiers[0].state[1] == Approx(2.0f * s1.modifiers[0].state[1]).margin(1e-3f));
}

TEST_CASE("CA_BUFFER: буфер снижает свободный Ca²⁺ при избытке", "[ca_buffer]") {
    Segment s = make_seg(0.0001f, 1.0f);   // Ca=100нМ (базальный)
    ca_buffer_current(&s.modifiers[0], &s, 0.1f);   // инициализация равновесия

    s.intra_ions[ION_CA] = 0.01f;   // спайк Ca до 10µM
    float Ca0 = s.intra_ions[ION_CA];

    // Длительное время — буфер связывает Ca²⁺ (медленный, τ~секунды).
    for (int i = 0; i < 5000; i++) {
        ca_buffer_current(&s.modifiers[0], &s, 1.0f);
    }

    INFO("Ca_free: " << Ca0 << " -> " << s.intra_ions[ION_CA]);
    REQUIRE(s.intra_ions[ION_CA] < Ca0);   // свободный Ca²⁺ снизился (связан)
}
