// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_gating_dynamics.cpp — динамические тесты гейтинга: ИНАКТИВАЦИЯ (h V½),
// ДЕАКТИВАЦИЯ (закрытие на реполяризации), ДЕСЕНСИТИЗАЦИЯ (d, Ca²⁺-зав.), RECOVERY.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double nav17_current(Modifier* m, Segment* s, float dt);
extern double nav13_current(Modifier* m, Segment* s, float dt);
extern double trpv1_channel_current(Modifier* m, Segment* s, float dt);

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

// ИНАКТИВАЦИЯ: h_inf ≈ 0.5 при V½_инакт (Nav1.7 = −71, Nav1.3 = −51.8).
TEST_CASE("Инактивация: h_inf≈0.5 при V½_инакт", "[gating]") {
    Segment s = make_seg(-71.0f);  // Nav1.7 V½_инакт
    s.modifiers[0].type = MOD_NAV17; s.modifiers[0].density = 1.0f;
    // m при −71 закрыт (m_inf→0), но h_inf проверяем: при −71 h_inf≈0.5
    // запускаем от состояния h=1 (state[1]=1), при −71 m→0 → open→0, но h спадает к 0.5
    s.modifiers[0].state[1] = 1.0f;  // h стартует открытым
    for (int i = 0; i < 100; i++) nav17_current(&s.modifiers[0], &s, 0.1f);  // 10 мс >> τ_h 1.2
    REQUIRE(s.modifiers[0].state[1] == Approx(0.5f).margin(0.1f));  // h_inf ≈ 0.5 при V½
}

// ДЕАКТИВАЦИЯ: после деполяризации (m→1) реполяризация к −100 закрывает m (m→0).
TEST_CASE("Деактивация: m закрывается при реполяризации", "[gating]") {
    Segment s = make_seg(20.0f);
    s.modifiers[0].type = MOD_NAV17; s.modifiers[0].density = 1.0f;
    for (int i = 0; i < 20; i++) nav17_current(&s.modifiers[0], &s, 0.5f);  // m→1 (актив.)
    float m_open = s.modifiers[0].state[0];
    REQUIRE(m_open > 0.8f);  // m активирован при +20
    // реполяризация к −100
    s.V_m = -100.0f;
    for (int i = 0; i < 20; i++) nav17_current(&s.modifiers[0], &s, 0.5f);  // m закрывается
    REQUIRE(s.modifiers[0].state[0] < 0.3f);  // m деактивировался
}

// ДЕСЕНСИТИЗАЦИЯ: TRPV1 d растёт с внутриклеточным Ca²⁺.
TEST_CASE("Десенситизация: TRPV1 d растёт с Ca²⁺", "[gating]") {
    Segment s = make_seg(-60.0f);
    s.modifiers[0].type = MOD_TRPV1_CHANNEL; s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[4] = 10.0f;  // капсаицин (насыщающий)
    // поднимаем Ca²⁺ внутриклеточно
    s.intra_ions[ION_CA] = 0.001f;  // 1 µM
    for (int i = 0; i < 2000; i++) trpv1_channel_current(&s.modifiers[0], &s, 0.1f);  // τ_desens 150 мс → 200 мс
    REQUIRE(s.modifiers[0].state[1] > 0.5f);  // d выросло (десенситизация)
}

// RECOVERY: Nav1.3 h восстанавливается при гиперполяризации.
TEST_CASE("Recovery: h восстанавливается при гиперполяризации", "[gating]") {
    Segment s = make_seg(0.0f);
    s.modifiers[0].type = MOD_NAV13; s.modifiers[0].density = 1.0f;
    for (int i = 0; i < 100; i++) nav13_current(&s.modifiers[0], &s, 0.5f);  // h→0 при 0 мВ
    REQUIRE(s.modifiers[0].state[1] < 0.3f);  // h инактивирован
    s.V_m = -100.0f;  // гиперполяризация
    for (int i = 0; i < 100; i++) nav13_current(&s.modifiers[0], &s, 0.5f);  // h→1
    REQUIRE(s.modifiers[0].state[1] > 0.7f);  // h восстановился
}
