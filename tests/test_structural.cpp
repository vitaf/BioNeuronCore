// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_structural.cpp — тесты структурных/generic модификаторов (3, 5)
// Metabotropic (3) и Synaptic release (5): Ом→GHK K⁺-ток.
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "types.h"
#include "modifier_interface.h"
#include <cmath>

extern double metabotropic_current(Modifier*, Segment*, float);
extern double synaptic_release_current(Modifier*, Segment*, float);

static void init_segment(Segment* s) {
    memset(s, 0, sizeof(Segment));
    s->V_m = -65.0f;
    s->length = 100.0f;
    s->radius = 10.0f;
    s->temperature = 37.0f;
    s->intra_ions[ION_NA] = 10.0f;
    s->extra_ions[ION_NA] = 140.0f;
    s->intra_ions[ION_K]  = 140.0f;
    s->extra_ions[ION_K]  = 4.0f;
    s->intra_ions[ION_CA] = 0.0001f;
    s->extra_ions[ION_CA] = 2.0f;
    s->intra_ions[ION_CL] = 10.0f;
    s->extra_ions[ION_CL] = 120.0f;
    s->extra_ions[ION_MG] = 1.0f;
    s->ATP_level = 3.0f;
}

static Modifier make_mod(uint8_t type, float density = 1.0f) {
    Modifier m = {};
    m.type = type;
    m.density = density;
    m.active_vars = 0;
    return m;
}

TEST_CASE("Metabotropic (3): NT drives GIRK K+ current (GHK)", "[structural]") {
    Segment seg = {}; init_segment(&seg);
    seg.neurotransmitter[NT_GLUTAMATE] = 1.0f;  // суммарный NT → occupancy
    seg.modifiers[0] = make_mod(MOD_METABOTROPIC);
    seg.modifier_count = 1;

    for (int t = 0; t < 1000; t++) metabotropic_current(&seg.modifiers[0], &seg, 1.0f);
    // G-белковая активация выросла, ток ненулевой (GHK K⁺)
    REQUIRE(seg.modifiers[0].state[0] > 0.3f);
    float I = metabotropic_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(fabsf(I) > 1e-4f);
}

TEST_CASE("Synaptic release (5): Ca2+ drives SK-like K+ current (GHK)", "[structural]") {
    Segment seg = {}; init_segment(&seg);
    seg.intra_ions[ION_CA] = 0.002f;  // 2 µM — выше K_d=0.5µM
    seg.modifiers[0] = make_mod(MOD_SYNAPTIC_RELEASE);
    seg.modifier_count = 1;

    for (int t = 0; t < 1000; t++) synaptic_release_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] > 0.5f);  // Ca²⁺-сенсор активирован
    float I = synaptic_release_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(fabsf(I) > 1e-4f);
}
