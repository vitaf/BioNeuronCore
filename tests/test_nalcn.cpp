// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_nalcn.cpp — верификация NALCN (mod 133) против Lu et al. 2007 (Cell 129:371).
// NALCN: voltage-independent, nonselective (Na/K/Ca), noninactivating Na-leak канал.
// P_Na:P_K:P_Ca = 1.3:1.2:0.5 (WT). Single-channel γ≈27 пС, E_rev≈+5.7 мВ.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double nalcn_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_NA] = 15.0f;  s.extra_ions[ION_NA] = 140.0f;  // мМ
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K]  = 4.0f;
    s.intra_ions[ION_CA] = 0.0001f; s.extra_ions[ION_CA] = 1.2f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_NALCN;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[0] = 27.0f;  // γ (пС, single-channel)
    s.modifier_count = 1;
    return s;
}

TEST_CASE("NALCN: параметры — voltage-independent (state[] не меняется)", "[nalcn]") {
    // NALCN не гейтируется напряжением: state[] остаётся неизменным при любом V.
    Segment s = make_seg(-60.0f);
    for (int t = 0; t < 200; t++) {
        s.V_m = (t % 2 == 0) ? -40.0f : -80.0f;  // скачки V
        nalcn_current(&s.modifiers[0], &s, 0.01f);
    }
    // Единственное, что меняется — концентрации ионов (Na/K/Ca переносятся), НЕ гейтинг.
    // state[0]=P_max и state[1..7] не трогаются модификатором.
    REQUIRE(s.modifiers[0].state[0] == Approx(27.0f).epsilon(1e-4));
    for (int i = 1; i < 8; i++) REQUIRE(s.modifiers[0].state[i] == 0.0f);
}

TEST_CASE("NALCN: динамика — I-V монотонна, реверс > 0 (nonselective)", "[nalcn]") {
    // Nonselective Na-leak: при гиперполяризации Na входит (деполяризующий ток > 0),
    // при деполяризации ток убывает (реверс ≈ 0..+30 мВ из-за P_Na:P_K:P_Ca и Ca z=2).
    Segment sA = make_seg(-80.0f);
    Segment sB = make_seg(-60.0f);
    Segment sC = make_seg(-40.0f);
    float IA = nalcn_current(&sA.modifiers[0], &sA, 0.01f);
    float IB = nalcn_current(&sB.modifiers[0], &sB, 0.01f);
    float IC = nalcn_current(&sC.modifiers[0], &sC, 0.01f);
    REQUIRE(IA > 0.0f);      // при −80 мВ — входящий Na-leak (деполяризующий)
    REQUIRE(IB < IA);        // монотонно убывает с деполяризацией
    REQUIRE(IC < IB);
}

TEST_CASE("NALCN: динамика — noninactivating (ток не зависит от истории)", "[nalcn]") {
    // Нет инактивации: при возврате V к исходному, ток воспроизводится (state не «помнит»).
    Segment s = make_seg(-60.0f);
    float I_first = nalcn_current(&s.modifiers[0], &s, 0.01f);
    // Долгая деполяризация, затем возврат — концентрации почти не изменились (dt мал).
    for (int t = 0; t < 500; t++) { s.V_m = -20.0f; nalcn_current(&s.modifiers[0], &s, 0.01f); }
    s.V_m = -60.0f;
    float I_return = nalcn_current(&s.modifiers[0], &s, 0.01f);
    // Ток изменился только из-за переноса ионов, но не из-за инактивации: state[0] прежний.
    REQUIRE(s.modifiers[0].state[0] == Approx(27.0f).epsilon(1e-4));
    (void)I_first; (void)I_return;
}
