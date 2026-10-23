// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_transporters_neuronal.cpp — верификация нейрональных транспортеров:
// EAAT3 (183, SLC1A1), GlyT2 (184, SLC6A5), GAT1 (185, SLC6A1).
// Стехиометрии: EAAT3=GLAST (3Na⁺+Glu/1K⁺, net +2), GlyT2 (3Na⁺+1Cl⁻+Gly, net +2,
// Roux & Supplisson 2000), GAT1 (3Na⁺+1Cl⁻+GABA, net +2, Willford 2015).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double eaat3_current(Modifier* m, Segment* s, float dt);
extern double glyt2_current(Modifier* m, Segment* s, float dt);
extern double gat1_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float nt_rel, int nt_idx) {
    Segment s = {};
    s.V_m = -80.0f;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K]  = 4.0f;
    s.intra_ions[ION_CL] = 5.0f; s.extra_ions[ION_CL] = 120.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.neurotransmitter[nt_idx] = nt_rel;
    s.modifiers[0].density = 10000.0f;  // большая плотность → детектируемый транспорт
    s.modifier_count = 1;
    return s;
}

// ── Константы ──────────────────────────────────────────────────────────
TEST_CASE("Neuronal transporters: литературные константы", "[eaat3][glyt2][gat1]") {
    REQUIRE(EAAT3_K_M_REL == Approx(20.0f));   // глутамат K_m ~30 µM → отн.ед.
    REQUIRE(GLYT2_K_M_REL == Approx(55.7f));   // глицин EC50 ~83.5 µM → отн.ед. (Erdem 2019)
    REQUIRE(GAT1_K_M_REL  == Approx(4.7f));    // GABA K_m ~7 µM → отн.ед.
}

// ── EAAT3: стехиометрия 3 Na⁺ внутрь / 1 K⁺ наружу + захват глутамата ──
TEST_CASE("EAAT3: стехиометрия 3 Na⁺ внутрь / 1 K⁺ наружу", "[eaat3]") {
    Segment s = make_seg(100.0f, NT_GLUTAMATE);
    s.modifiers[0].type = MOD_EAAT3;
    float na_before = s.intra_ions[ION_NA];
    float k_before  = s.intra_ions[ION_K];
    float glu_before = s.neurotransmitter[NT_GLUTAMATE];
    for (int t = 0; t < 100; t++) eaat3_current(&s.modifiers[0], &s, 0.1f);
    float dNa = s.intra_ions[ION_NA] - na_before;  // Na⁺ внутрь → +
    float dK  = s.intra_ions[ION_K] - k_before;    // K⁺ наружу → −
    REQUIRE(s.neurotransmitter[NT_GLUTAMATE] < glu_before);  // глутамат захвачен
    REQUIRE(dNa > 0.0f);
    REQUIRE(dK < 0.0f);
    REQUIRE(dNa == Approx(-3.0f * dK).margin(0.1f * dNa + 1e-9f));  // 3 Na⁺ : 1 K⁺
}

// ── GlyT2: стехиометрия 3 Na⁺ + 1 Cl⁻ внутрь + захват глицина ─────────
TEST_CASE("GlyT2: стехиометрия 3 Na⁺ + 1 Cl⁻ внутрь", "[glyt2]") {
    Segment s = make_seg(100.0f, NT_GLYCINE);
    s.modifiers[0].type = MOD_GLYT2;
    float na_before = s.intra_ions[ION_NA];
    float cl_before = s.intra_ions[ION_CL];
    float gly_before = s.neurotransmitter[NT_GLYCINE];
    for (int t = 0; t < 50; t++) glyt2_current(&s.modifiers[0], &s, 0.1f);
    float dNa = s.intra_ions[ION_NA] - na_before;
    float dCl = s.intra_ions[ION_CL] - cl_before;
    REQUIRE(s.neurotransmitter[NT_GLYCINE] < gly_before);  // глицин захвачен
    REQUIRE(dNa > 0.0f);
    REQUIRE(dCl > 0.0f);
    REQUIRE(dNa == Approx(3.0f * dCl).margin(0.1f * dNa + 1e-9f));  // 3 Na⁺ : 1 Cl⁻
}

// ── GAT1: стехиометрия 3 Na⁺ + 1 Cl⁻ внутрь + захват GABA ─────────────
TEST_CASE("GAT1: стехиометрия 3 Na⁺ + 1 Cl⁻ внутрь", "[gat1]") {
    Segment s = make_seg(100.0f, NT_GABA);
    s.modifiers[0].type = MOD_GAT1;
    float na_before = s.intra_ions[ION_NA];
    float cl_before = s.intra_ions[ION_CL];
    float gaba_before = s.neurotransmitter[NT_GABA];
    for (int t = 0; t < 50; t++) gat1_current(&s.modifiers[0], &s, 0.1f);
    float dNa = s.intra_ions[ION_NA] - na_before;
    float dCl = s.intra_ions[ION_CL] - cl_before;
    REQUIRE(s.neurotransmitter[NT_GABA] < gaba_before);  // GABA захвачен
    REQUIRE(dNa > 0.0f);
    REQUIRE(dCl > 0.0f);
    REQUIRE(dNa == Approx(3.0f * dCl).margin(0.1f * dNa + 1e-9f));  // 3 Na⁺ : 1 Cl⁻
}

// ── Ток зависит от [NT] (Michaelis-Menten) ────────────────────────────
TEST_CASE("Neuronal transporters: ток зависит от [NT]", "[eaat3][glyt2][gat1]") {
    Segment hi = make_seg(100.0f, NT_GLUTAMATE);
    Segment lo = make_seg(1.0f, NT_GLUTAMATE);
    hi.modifiers[0].type = MOD_EAAT3; lo.modifiers[0].type = MOD_EAAT3;
    float i_hi = eaat3_current(&hi.modifiers[0], &hi, 0.1f);
    float i_lo = eaat3_current(&lo.modifiers[0], &lo, 0.1f);
    REQUIRE(i_hi > i_lo);
}
