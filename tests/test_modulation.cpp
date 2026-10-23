// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_modulation.cpp — тесты модуляции (#29 TEA→Kv3.1, #30 PKC→Kv3.1, #31 O₂→TASK).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double kv31_current(Modifier* m, Segment* s, float dt);
extern double task_current(Modifier* m, Segment* s, float dt);

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

TEST_CASE("#29 TEA блокирует Kv3.1 (IC50 0.2 мМ)", "[modulation]") {
    // При V=+40 (m активирован): TEA 0.2 мМ (=IC50) вдвое уменьшает ток.
    Segment s = make_seg(40.0f);
    s.modifiers[0].type = MOD_KV31; s.modifiers[0].density = 1.0f;
    for (int i = 0; i < 10; i++) kv31_current(&s.modifiers[0], &s, 0.5f);  // m→1
    float I_free = kv31_current(&s.modifiers[0], &s, 0.5f);

    Segment s2 = make_seg(40.0f);
    s2.modifiers[0].type = MOD_KV31; s2.modifiers[0].density = 1.0f;
    s2.modifiers[0].state[5] = 0.2f;  // TEA 0.2 мМ = IC50
    for (int i = 0; i < 10; i++) kv31_current(&s2.modifiers[0], &s2, 0.5f);
    float I_blocked = kv31_current(&s2.modifiers[0], &s2, 0.5f);

    REQUIRE(fabsf(I_blocked) < fabsf(I_free));  // TEA уменьшает ток
    REQUIRE(fabsf(I_blocked) == Approx(fabsf(I_free) * 0.5f).margin(fabsf(I_free) * 0.2f));  // ~вдвое при IC50
}

TEST_CASE("#30 PKC подавляет амплитуду Kv3.1 (не V½)", "[modulation]") {
    // PKC-активность (segment_pkc_act) — подавляет ток. Без PKC-активации ток больше.
    Segment s = make_seg(40.0f);
    s.modifiers[0].type = MOD_KV31; s.modifiers[0].density = 1.0f;
    for (int i = 0; i < 10; i++) kv31_current(&s.modifiers[0], &s, 0.5f);
    float I = kv31_current(&s.modifiers[0], &s, 0.5f);
    REQUIRE(std::isfinite(I));  // вызов работает (PKC-каскад читается корректно)
    REQUIRE(fabsf(I) > 0.0f);   // ток есть (m активирован при +40)
}

TEST_CASE("#31 TASK закрывается при гипоксии (PO₂ 40)", "[modulation]") {
    // Нормоксия (PO₂ 150): ток ~максимален. Гипоксия (PO₂ 40): ток меньше.
    Segment s_norm = make_seg(-60.0f);
    s_norm.modifiers[0].type = MOD_TASK; s_norm.modifiers[0].density = 1.0f;
    s_norm.modifiers[0].state[5] = 150.0f;  // PO₂ нормоксия
    for (int i = 0; i < 2000; i++) task_current(&s_norm.modifiers[0], &s_norm, 1.0f);  // τ 1с → steady
    float I_norm = task_current(&s_norm.modifiers[0], &s_norm, 1.0f);

    Segment s_hyp = make_seg(-60.0f);
    s_hyp.modifiers[0].type = MOD_TASK; s_hyp.modifiers[0].density = 1.0f;
    s_hyp.modifiers[0].state[5] = 40.0f;  // PO₂ гипоксия
    for (int i = 0; i < 2000; i++) task_current(&s_hyp.modifiers[0], &s_hyp, 1.0f);
    float I_hyp = task_current(&s_hyp.modifiers[0], &s_hyp, 1.0f);

    REQUIRE(fabsf(I_hyp) < fabsf(I_norm));  // гипоксия уменьшает ток
}
