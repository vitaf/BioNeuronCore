// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_glast.cpp — верификация MOD_GLAST (147) GLAST/GLT-1 (EAAT1/2).
// Глиальный глутаматный транспортер: 3 Na⁺ + Glu (внутрь), 1 K⁺ (наружу), чистый +2.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double glast_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float glu_rel) {
    Segment s = {};
    s.V_m = -80.0f;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K]  = 4.0f;
    s.intra_ions[ION_CL] = 5.0f; s.extra_ions[ION_CL] = 120.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.neurotransmitter[NT_GLUTAMATE] = glu_rel;
    s.modifiers[0].type = MOD_GLAST;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

// ── Константы (K_m отн., I_max, перевод) ───────────────────────────────
TEST_CASE("GLAST: литературные константы", "[glast]") {
    REQUIRE(GLAST_K_M_REL == Approx(10.0f));          // K_m 15 µM → отн.ед.
    REQUIRE(GLAST_IMAX_UA_PER_CM2 == Approx(2.0f));   // калибровка
    REQUIRE(GLAST_GLU_REL_PER_MM == Approx(667.0f));  // 1 мМ = 667 отн.ед.
}

// ── Michaelis-Menten насыщение ────────────────────────────────────────
TEST_CASE("GLAST: Michaelis-Menten насыщение (K_m=10 отн.ед.)", "[glast]") {
    REQUIRE(glast_saturation(10.0f) == Approx(0.5f).margin(1e-3f));  // полумаксимум при K_m
    REQUIRE(glast_saturation(0.0f) == Approx(0.0f));                  // нет глутамата → нет захвата
    REQUIRE(glast_saturation(1000.0f) > 0.98f);                       // насыщение
}

// ── Ток ∝ насыщение ──────────────────────────────────────────────────
TEST_CASE("GLAST: ток зависит от [Glu] (Michaelis-Menten)", "[glast]") {
    Segment s_hi = make_seg(100.0f);  // высокий Glu
    Segment s_lo = make_seg(1.0f);    // низкий Glu
    float i_hi = glast_current(&s_hi.modifiers[0], &s_hi, 0.1f);
    float i_lo = glast_current(&s_lo.modifiers[0], &s_lo, 0.1f);
    REQUIRE(i_hi > i_lo);
}

// ── Стехиометрия: 3 Na⁺ внутрь : 1 K⁺ наружу ──────────────────────────
TEST_CASE("GLAST: стехиометрия 3 Na⁺ внутрь / 1 K⁺ наружу", "[glast]") {
    Segment s = make_seg(100.0f);
    s.modifiers[0].density = 10000.0f;  // большая плотность → детектируемый транспорт
    float na_before = s.intra_ions[ION_NA];
    float k_before  = s.intra_ions[ION_K];
    for (int t = 0; t < 100; t++) glast_current(&s.modifiers[0], &s, 0.1f);
    float dNa = s.intra_ions[ION_NA] - na_before;  // Na⁺ внутрь → +
    float dK  = s.intra_ions[ION_K] - k_before;    // K⁺ наружу → −
    REQUIRE(dNa > 0.0f);
    REQUIRE(dK < 0.0f);
    // Стехиометрия: dNa ≈ −3 · dK
    REQUIRE(dNa == Approx(-3.0f * dK).margin(0.1f * dNa + 1e-9f));
}

// ── Захват глутамата ──────────────────────────────────────────────────
TEST_CASE("GLAST: захватывает глутамат (clears ambient Glu)", "[glast]") {
    Segment s = make_seg(100.0f);
    s.modifiers[0].density = 10000.0f;
    float glu_before = s.neurotransmitter[NT_GLUTAMATE];
    for (int t = 0; t < 100; t++) glast_current(&s.modifiers[0], &s, 0.1f);
    REQUIRE(s.neurotransmitter[NT_GLUTAMATE] < glu_before);
    REQUIRE(s.neurotransmitter[NT_GLUTAMATE] >= 0.0f);
}
