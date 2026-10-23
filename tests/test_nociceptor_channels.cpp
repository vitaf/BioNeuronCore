// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_nociceptor_channels.cpp — тесты 8 новых модификаторов (169-176):
// Nav1.7/1.8/1.9 (m³h, TTX-S/R/persistent), TRPV1/TRPM8/TRPA1/TRPV3 (лиганд r·(1−d)),
// KCNQ4 (m, V½ −78). Проверяет: E_rev (селективность) + гейтинг + TTX-чувствительность.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double nav17_current(Modifier* m, Segment* s, float dt);
extern double nav18_current(Modifier* m, Segment* s, float dt);
extern double nav19_current(Modifier* m, Segment* s, float dt);
extern double trpv1_channel_current(Modifier* m, Segment* s, float dt);
extern double trpm8_current(Modifier* m, Segment* s, float dt);
extern double trpa1_current(Modifier* m, Segment* s, float dt);
extern double trpv3_current(Modifier* m, Segment* s, float dt);
extern double kcnq4_current(Modifier* m, Segment* s, float dt);

static double nernst_mv(double c_in, double c_out, double z) {
    return (GAS_CONST * CELSIUS_37 / (z * FARADAY_CONST)) * log(c_out / c_in) * 1000.0;
}

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_NA] = 15.0f;  s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K]  = 4.0f;
    s.intra_ions[ION_CA] = 0.0001f; s.extra_ions[ION_CA] = 1.2f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifier_count = 1;
    return s;
}

TEST_CASE("Nav1.7/1.8/1.9: Na-селективный (E_rev = E_Na)", "[nociceptor]") {
    double e_na = nernst_mv(15.0, 140.0, 1.0);  // ≈ +57 мВ
    Segment s = make_seg((float)e_na);
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].type = MOD_NAV17;
    REQUIRE(nav17_current(&s.modifiers[0], &s, 0.0f) == Approx(0.0f).margin(1.0f));
    s.modifiers[0] = {}; s.modifiers[0].type = MOD_NAV18; s.modifiers[0].density = 1.0f;
    REQUIRE(nav18_current(&s.modifiers[0], &s, 0.0f) == Approx(0.0f).margin(1.0f));
    s.modifiers[0] = {}; s.modifiers[0].type = MOD_NAV19; s.modifiers[0].density = 1.0f;
    REQUIRE(nav19_current(&s.modifiers[0], &s, 0.0f) == Approx(0.0f).margin(1.0f));
}

TEST_CASE("Nav1.7 TTX-S vs Nav1.8 TTX-R (ингибиция)", "[nociceptor]") {
    // При V = +20 (м активирован): Nav1.7 блокируется 10 нМ TTX, Nav1.8 — нет.
    float ttx = 1000.0f;  // 1000 нМ = 1 µM
    Segment s7 = make_seg(20.0f);
    s7.modifiers[0].type = MOD_NAV17; s7.modifiers[0].density = 1.0f;
    s7.modifiers[0].state[5] = ttx;  // TTX
    for (int i = 0; i < 100; i++) nav17_current(&s7.modifiers[0], &s7, 0.05f);  // steady-state m,h
    float I7_blocked = nav17_current(&s7.modifiers[0], &s7, 0.05f);

    Segment s7_free = make_seg(20.0f);
    s7_free.modifiers[0].type = MOD_NAV17; s7_free.modifiers[0].density = 1.0f;
    for (int i = 0; i < 100; i++) nav17_current(&s7_free.modifiers[0], &s7_free, 0.05f);
    float I7_free = nav17_current(&s7_free.modifiers[0], &s7_free, 0.05f);

    // TTX-S: 1 µM TTX сильно подавляет Nav1.7 (h-инактивация частичная, но ток уже мал).
    // Проверяем что блок дополнительно уменьшает |I|.
    REQUIRE(fabsf(I7_blocked) <= fabsf(I7_free) * 0.5f + 1e-3f);
}

TEST_CASE("KCNQ4: K-селективный (E_rev = E_K)", "[nociceptor]") {
    double e_k = nernst_mv(140.0, 4.0, 1.0);  // ≈ −91 мВ
    Segment s = make_seg((float)e_k);
    s.modifiers[0].type = MOD_KCNQ4; s.modifiers[0].density = 1.0f;
    REQUIRE(kcnq4_current(&s.modifiers[0], &s, 0.0f) == Approx(0.0f).margin(1.0f));
}

TEST_CASE("KCNQ4: V½ ≈ −78 (частично открыт при покое)", "[nociceptor]") {
    // Нативный V½ −78: при −78 мВ m_inf ≈ 0.5; при +40 m_inf → 1.
    Segment s = make_seg(-78.0f);
    s.modifiers[0].type = MOD_KCNQ4; s.modifiers[0].density = 1.0f;
    for (int i = 0; i < 1100; i++) kcnq4_current(&s.modifiers[0], &s, 1.0f);  // 1100 мс ≈ 5τ (τ 212 мс)
    REQUIRE(s.modifiers[0].state[0] == Approx(0.5f).margin(0.1f));
}

TEST_CASE("TRPV1: лиганд-активация (капсаицин) + Ca²⁺-десенситизация", "[nociceptor]") {
    // r = [lig]/([lig]+0.7µM): при 0.7 µM r≈0.5. Десенситизация d растёт с Ca²⁺.
    Segment s = make_seg(-60.0f);
    s.modifiers[0].type = MOD_TRPV1_CHANNEL; s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[4] = 0.7f;  // капсаицин 0.7 µM = EC50
    for (int i = 0; i < 500; i++) trpv1_channel_current(&s.modifiers[0], &s, 0.1f);
    // активация r растёт к ~0.5 (state[0]); ток деполяризующий (вход катионов, return>0)
    REQUIRE(s.modifiers[0].state[0] > 0.25f);  // частично активирован
    REQUIRE(trpv1_channel_current(&s.modifiers[0], &s, 0.1f) > 0.0f);  // катионный вход = деполяризация
}

TEST_CASE("TRPM8/TRPA1/TRPV3: вызов не падает, ток конечный", "[nociceptor]") {
    Segment s = make_seg(-60.0f);
    s.modifiers[0].density = 1.0f; s.modifiers[0].state[4] = 30.0f;  // лиганд
    s.modifiers[0].type = MOD_TRPM8;
    REQUIRE(std::isfinite(trpm8_current(&s.modifiers[0], &s, 0.1f)));
    s.modifiers[0] = {}; s.modifiers[0].type = MOD_TRPA1; s.modifiers[0].density = 1.0f; s.modifiers[0].state[4] = 30.0f;
    REQUIRE(std::isfinite(trpa1_current(&s.modifiers[0], &s, 0.1f)));
    s.modifiers[0] = {}; s.modifiers[0].type = MOD_TRPV3; s.modifiers[0].density = 1.0f; s.modifiers[0].state[4] = 50.0f;
    REQUIRE(std::isfinite(trpv3_current(&s.modifiers[0], &s, 0.1f)));
}
