// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_thik2.cpp — верификация MOD_THIK2 (ID 167) — THIK-2 (K2P12.1/KCNK12).
// ⚠️ THIK-2 гомомерно НЕфункционален (задержан в ЭПР, Rajan 2001, PMID 11060316):
// γ не измерена — placeholder γ=5 пС, Po=0.5. GHK K⁺ фоновая утечка.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double thik2_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_THIK2;
    s.modifiers[0].density = 10000.0f;  // γ=5 пС — высокий density для измеримого тока (G не найдена, плейсхолдер)
    s.modifier_count = 1;
    return s;
}

TEST_CASE("THIK-2: фоновая K⁺-утечка при V > E_K", "[thik2]") {
    Segment s = make_seg(-60.0f);
    float I = thik2_current(&s.modifiers[0], &s, 0.1f);
    REQUIRE(I < 0.0f);  // K⁺-эффлюкс → гиперполяризующий (return -I_K < 0)
}

TEST_CASE("THIK-2: E_rev — знак меняется у E_K", "[thik2]") {
    Segment s_out = make_seg(-60.0f);
    Segment s_in = make_seg(-110.0f);
    float I_out = thik2_current(&s_out.modifiers[0], &s_out, 0.0f);
    float I_in = thik2_current(&s_in.modifiers[0], &s_in, 0.0f);
    REQUIRE(I_out < 0.0f);  // выше E_K → K⁺ выходит
    REQUIRE(I_in > 0.0f);   // ниже E_K → K⁺ входит
}

TEST_CASE("THIK-2: K⁺ консервация — intra K падает", "[thik2]") {
    Segment s = make_seg(-60.0f);
    float k_before = s.intra_ions[ION_K];
    thik2_current(&s.modifiers[0], &s, 0.1f);
    REQUIRE(s.intra_ions[ION_K] < k_before);  // эффлюкс снижает внутриклеточный K⁺
}
