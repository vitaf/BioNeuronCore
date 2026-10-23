// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_cav21.cpp — верификация MOD_CAV21 (140) против Cav2.1 (P/Q-type, CACNA1A).
// Gambeta 2021 (Mol Brain 14:4): V½_акт=−15 мВ, V½_инакт=−50 мВ, τ_m≈1 мс,
// τ_h≈300 мс, γ=15 пС, слабая CDI. Пресинаптический — Ca²⁺ в бутон (bouton_ca_step).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double cav21_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_CA] = 0.0001f;  // 100 нМ
    s.extra_ions[ION_CA] = 2.0f;     // 2 мМ
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_CAV21;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

TEST_CASE("Cav2.1: m_inf V½=−15 мВ (Gambeta 2021)", "[cav21]") {
    REQUIRE(cav21_m_inf(-15.0f) == Approx(0.5f).margin(1e-3f));
    REQUIRE(cav21_m_inf(30.0f) > 0.99f);    // деполяр. → открыт
    REQUIRE(cav21_m_inf(-60.0f) < 0.01f);   // гиперполяр. → закрыт
}

TEST_CASE("Cav2.1: h_inf V½=−50 мВ (Gambeta 2021)", "[cav21]") {
    REQUIRE(cav21_h_inf(-50.0f, 0.0f) == Approx(0.5f).margin(1e-3f));
    REQUIRE(cav21_h_inf(-100.0f, 0.0f) > 0.99f);  // гиперполяр. → доступен
    REQUIRE(cav21_h_inf(0.0f, 0.0f) < 0.01f);     // деполяр. → инактивирован
}

TEST_CASE("Cav2.1: CDI — Ca²⁺ снижает h", "[cav21]") {
    REQUIRE(cav21_h_inf(-100.0f, 3.0f) < cav21_h_inf(-100.0f, 0.0f));
}

TEST_CASE("Cav2.1: γ/τ константы (τ фикс — слабая V-зависимость)", "[cav21]") {
    REQUIRE(CAV21_GAMMA_PS == Approx(15.0f));
    REQUIRE(CAV21_TAU_M_MS == Approx(1.0f));
    REQUIRE(CAV21_TAU_H_MS == Approx(300.0f));
}

TEST_CASE("Cav2.1: m-релаксация к m_inf (τ_m=1 мс)", "[cav21]") {
    Segment s = make_seg(30.0f);
    s.modifiers[0].state[0] = 0.0f;
    for (int t = 0; t < 100; t++) cav21_current(&s.modifiers[0], &s, 0.1f);  // 10 мс = 10·τ_m
    float m_inf = cav21_m_inf(30.0f);
    INFO("m=" << s.modifiers[0].state[0] << " m_inf=" << m_inf);
    REQUIRE(s.modifiers[0].state[0] == Approx(m_inf).margin(0.01f));
}
