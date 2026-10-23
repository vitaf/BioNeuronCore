// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_trpv1.cpp — верификация TRPV1 (MOD_NT_RECEPTOR_TRPV1, 16).
// Caterina (1997): температурный (>43°C) + капсаициновый сенсор, P_Ca/P_Na≈6.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double trpv1_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V, float T, float cap) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;   // мМ
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_CA] = 0.0001f; s.extra_ions[ION_CA] = 2.0f;
    s.intra_ions[ION_CL] = 5.0f; s.extra_ions[ION_CL] = 130.0f;
    s.temperature = T;
    s.neurotransmitter[NT_CAPSAICIN] = cap;
    s.ATP_level = 1.0f;
    s.radius = 5.0f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_NT_RECEPTOR_TRPV1;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

TEST_CASE("TRPV1: температурная активация (>43°C)", "[trpv1]") {
    Segment s_hot = make_seg(-70.0f, 45.0f, 0.0f);   // жарко → открыт
    Segment s_cold = make_seg(-70.0f, 37.0f, 0.0f);  // холодно → закрыт

    float I_hot = 0.0f, I_cold = 0.0f;
    for (int i = 0; i < 200; i++) {   // медленная активация (~200мс)
        I_hot  = trpv1_current(&s_hot.modifiers[0],  &s_hot,  1.0f);
        I_cold = trpv1_current(&s_cold.modifiers[0], &s_cold, 1.0f);
    }
    INFO("I(45°C)=" << I_hot << " I(37°C)=" << I_cold);
    REQUIRE(fabsf(I_hot) > fabsf(I_cold));   // тепло открывает TRPV1
    REQUIRE(I_hot > 0.0f);                   // возбуждающий
}

TEST_CASE("TRPV1: капсаиновая активация при 37°C", "[trpv1]") {
    Segment s_cap = make_seg(-70.0f, 37.0f, 1.0f);   // капсаицин → открыт
    Segment s_no  = make_seg(-70.0f, 37.0f, 0.0f);   // без капсаицина → закрыт

    float I_cap = 0.0f, I_no = 0.0f;
    for (int i = 0; i < 200; i++) {
        I_cap = trpv1_current(&s_cap.modifiers[0], &s_cap, 1.0f);
        I_no  = trpv1_current(&s_no.modifiers[0],  &s_no,  1.0f);
    }
    INFO("I(cap)=" << I_cap << " I(no)=" << I_no);
    REQUIRE(fabsf(I_cap) > fabsf(I_no));   // капсаицин открывает при физиол. температуре
    REQUIRE(I_cap > 0.0f);
}

TEST_CASE("TRPV1: без стимула ток ≈ 0", "[trpv1]") {
    Segment s = make_seg(-70.0f, 37.0f, 0.0f);
    float I = 0.0f;
    for (int i = 0; i < 20; i++) I = trpv1_current(&s.modifiers[0], &s, 1.0f);
    // Хвост Больцмана тепловой активации при 37°C (≈6°C ниже порога 43°C) даёт ~2 фА.
    REQUIRE(fabsf(I) < 5e-3f);
}
