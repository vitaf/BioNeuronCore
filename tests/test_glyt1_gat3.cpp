// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_glyt1_gat3.cpp — верификация GlyT1 (148) + GAT3 (149), глиальные SLC6-транспортеры.
// Стехиометрия: 2 Na⁺ + 1 Cl⁻ + субстрат (внутрь), net +1.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double glyt1_current(Modifier* m, Segment* s, float dt);
extern double gat3_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float nt_rel, int nt_idx) {
    Segment s = {};
    s.V_m = -80.0f;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K]  = 4.0f;
    s.intra_ions[ION_CL] = 5.0f; s.extra_ions[ION_CL] = 120.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.neurotransmitter[nt_idx] = nt_rel;
    s.modifiers[0].density = 10000.0f;
    s.modifier_count = 1;
    return s;
}

// ── Константы ──────────────────────────────────────────────────────────
TEST_CASE("GlyT1/GAT3: литературные константы", "[glyt1][gat3]") {
    REQUIRE(GLYT1_K_M_REL == Approx(30.0f));   // глицин K_m 45 µM → отн.ед.
    REQUIRE(GAT3_K_M_REL == Approx(0.5f));     // GABA K_m 0.8 µM → отн.ед.
}

// ── GlyT1: захватывает глицин + 2 Na⁺ + 1 Cl⁻ внутрь ──────────────────
TEST_CASE("GlyT1: стехиометрия 2 Na⁺ + 1 Cl⁻ внутрь", "[glyt1]") {
    Segment s = make_seg(100.0f, NT_GLYCINE);
    s.modifiers[0].type = MOD_GLYT1;
    float na_before = s.intra_ions[ION_NA];
    float cl_before = s.intra_ions[ION_CL];
    float gly_before = s.neurotransmitter[NT_GLYCINE];
    for (int t = 0; t < 50; t++) glyt1_current(&s.modifiers[0], &s, 0.1f);
    float dNa = s.intra_ions[ION_NA] - na_before;
    float dCl = s.intra_ions[ION_CL] - cl_before;
    REQUIRE(s.neurotransmitter[NT_GLYCINE] < gly_before);  // глицин захвачен
    REQUIRE(dNa > 0.0f);  // Na⁺ внутрь
    REQUIRE(dCl > 0.0f);  // Cl⁻ внутрь
    REQUIRE(dNa == Approx(2.0f * dCl).margin(0.1f * dNa + 1e-9f));  // 2 Na⁺ : 1 Cl⁻
}

// ── GAT3: захватывает GABA + 2 Na⁺ + 1 Cl⁻ внутрь ─────────────────────
TEST_CASE("GAT3: стехиометрия 2 Na⁺ + 1 Cl⁻ внутрь", "[gat3]") {
    Segment s = make_seg(100.0f, NT_GABA);
    s.modifiers[0].type = MOD_GAT3;
    float na_before = s.intra_ions[ION_NA];
    float cl_before = s.intra_ions[ION_CL];
    float gaba_before = s.neurotransmitter[NT_GABA];
    for (int t = 0; t < 50; t++) gat3_current(&s.modifiers[0], &s, 0.1f);
    float dNa = s.intra_ions[ION_NA] - na_before;
    float dCl = s.intra_ions[ION_CL] - cl_before;
    REQUIRE(s.neurotransmitter[NT_GABA] < gaba_before);  // GABA захвачен
    REQUIRE(dNa > 0.0f);
    REQUIRE(dCl > 0.0f);
    REQUIRE(dNa == Approx(2.0f * dCl).margin(0.1f * dNa + 1e-9f));
}
