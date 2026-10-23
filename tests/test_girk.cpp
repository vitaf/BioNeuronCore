// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_girk.cpp — верификация MOD_GIRK (ID 160) против GIRK (Kir3.x).
// Gβγ-активируемый inward-rectifier K⁺: γ=35 пС, Gβγ-зависимый (state[4]),
// НЕ V-зависимый, GHK K⁺ (z=1), tertiapin-Q (IC50 8 нМ, state[5] нМ).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double girk_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_GIRK;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[4] = 1.0f;  // Gβγ = 1 (полная активация)
    s.modifier_count = 1;
    return s;
}

// Полностью открытый канал (Gβγ=1), без блокара, гейтинг заморожен (dt=0)
static float I_open(float V) {
    Segment s = make_seg(V);
    return girk_current(&s.modifiers[0], &s, 0.0f);
}

TEST_CASE("GIRK: Po = state[4] (Gβγ) — при 1 K⁺ выходит, при 0 ток нулевой", "[girk]") {
    Segment s_on = make_seg(-40.0f);   // V=-40 > E_K → K⁺ выходит
    s_on.modifiers[0].state[4] = 1.0f;
    float I_on = girk_current(&s_on.modifiers[0], &s_on, 0.0f);
    INFO("I(Gβγ=1)=" << I_on);
    REQUIRE(I_on < 0.0f);              // выходящий K (return -I_K < 0)

    Segment s_off = make_seg(-40.0f);
    s_off.modifiers[0].state[4] = 0.0f;
    float I_off = girk_current(&s_off.modifiers[0], &s_off, 0.0f);
    INFO("I(Gβγ=0)=" << I_off);
    REQUIRE(fabsf(I_off) < 1e-3f);     // канал закрыт
}

TEST_CASE("GIRK: E_rev — знак тока меняется у E_K (~−90 мВ)", "[girk]") {
    REQUIRE(I_open(-60.0f) < 0.0f);    // V > E_K → K выходит (return -I_K < 0)
    REQUIRE(I_open(-110.0f) > 0.0f);   // V < E_K → K входит (return -I_K > 0)
}

TEST_CASE("GIRK: tertiapin-Q блок (80 нМ = 10×IC50 → |I| < 0.2×|I0|)", "[girk]") {
    float I0 = I_open(-40.0f);         // без блокара
    Segment s = make_seg(-40.0f);
    s.modifiers[0].state[5] = 80.0f;   // 80 нМ = 10×IC50 (8 нМ)
    float I_blk = girk_current(&s.modifiers[0], &s, 0.0f);
    INFO("I0=" << I0 << " I_blk=" << I_blk);
    REQUIRE(fabsf(I0) > 1e-3f);
    REQUIRE(fabsf(I_blk) < 0.2f * fabsf(I0));
}

TEST_CASE("GIRK: K⁺ консервация (intra + extra×10 сохраняется)", "[girk]") {
    Segment s = make_seg(-40.0f);
    float k_total_before = s.intra_ions[ION_K] + s.extra_ions[ION_K] * 10.0f;
    girk_current(&s.modifiers[0], &s, 0.1f);
    float k_total_after = s.intra_ions[ION_K] + s.extra_ions[ION_K] * 10.0f;
    REQUIRE(k_total_after == Approx(k_total_before).margin(0.01f));
}
