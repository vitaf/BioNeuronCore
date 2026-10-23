// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_molecular_cascades.cpp — тесты сигнальных ферментов (типы 82, 83, 86-92, 113)
// Каждый: параметрический (Kd/steady-state) + динамический (τ/отклик).
// Кросс-каскадные: DARPP-32←PKA, eCB←DAG, protein_synth←mTORC1, cofilin/mmp9←ERK.
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "types.h"
#include "modifier_interface.h"
#include <cmath>

extern double camkii_current(Modifier*, Segment*, float);
extern double calcineurin_current(Modifier*, Segment*, float);
extern double ecb_synthesis_current(Modifier*, Segment*, float);
extern double mapk_erk_current(Modifier*, Segment*, float);
extern double ikk_nfkb_current(Modifier*, Segment*, float);
extern double mtorc1_current(Modifier*, Segment*, float);
extern double protein_synth_current(Modifier*, Segment*, float);
extern double cofilin_current(Modifier*, Segment*, float);
extern double mmp9_current(Modifier*, Segment*, float);
extern double pp1_inhibitor_darpp32_current(Modifier*, Segment*, float);

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
    s->intra_ions[ION_CA] = 0.0001f;  // 0.1 µM (resting)
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

// ==================== CaMKII (82) ====================

TEST_CASE("CaMKII: high Ca2+ activates, resting does not", "[cascade]") {
    Segment hi = {}; init_segment(&hi);
    hi.intra_ions[ION_CA] = 0.002f;  // 2 µM (выше Kd=1µM)
    hi.modifiers[0] = make_mod(MOD_CAMKII);
    hi.modifier_count = 1;

    Segment lo = {}; init_segment(&lo);
    lo.intra_ions[ION_CA] = 0.0001f;  // 0.1 µM (resting, ниже Kd)
    lo.modifiers[0] = make_mod(MOD_CAMKII);
    lo.modifier_count = 1;

    for (int t = 0; t < 2000; t++) {
        camkii_current(&hi.modifiers[0], &hi, 1.0f);
        camkii_current(&lo.modifiers[0], &lo, 1.0f);
    }
    REQUIRE(hi.modifiers[0].state[0] > 0.5f);   // активирован
    REQUIRE(lo.modifiers[0].state[0] < 0.1f);   // resting — неактивен
}

TEST_CASE("CaMKII: density scales kinetics (0 = frozen)", "[cascade]") {
    Segment on = {}; init_segment(&on);
    on.intra_ions[ION_CA] = 0.002f;
    on.modifiers[0] = make_mod(MOD_CAMKII, 1.0f);
    on.modifier_count = 1;

    Segment off = on;
    off.modifiers[0] = make_mod(MOD_CAMKII, 0.0f);  // нет фермента

    for (int t = 0; t < 2000; t++) {
        camkii_current(&on.modifiers[0], &on, 1.0f);
        camkii_current(&off.modifiers[0], &off, 1.0f);
    }
    REQUIRE(on.modifiers[0].state[0] > 0.5f);
    REQUIRE(off.modifiers[0].state[0] == Approx(0.0f).margin(1e-6f));  // заморожен
}

// ==================== Calcineurin (83) ====================

TEST_CASE("Calcineurin: low-threshold — active at resting Ca2+", "[cascade]") {
    Segment seg = {}; init_segment(&seg);
    seg.intra_ions[ION_CA] = 0.0001f;  // 0.1 µM (выше Kd=0.05µM)
    seg.modifiers[0] = make_mod(MOD_CALCINEURIN);
    seg.modifier_count = 1;

    for (int t = 0; t < 1000; t++) calcineurin_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] > 0.5f);  // активен при resting
}

TEST_CASE("CaMKII vs Calcineurin: threshold ordering", "[cascade]") {
    Segment seg = {}; init_segment(&seg);
    seg.intra_ions[ION_CA] = 0.0001f;  // resting 0.1 µM
    seg.modifiers[0] = make_mod(MOD_CAMKII);
    seg.modifiers[1] = make_mod(MOD_CALCINEURIN);
    seg.modifier_count = 2;

    for (int t = 0; t < 2000; t++) {
        camkii_current(&seg.modifiers[0], &seg, 1.0f);
        calcineurin_current(&seg.modifiers[1], &seg, 1.0f);
    }
    // При resting Ca²⁺: Calcineurin активен, CaMKII — нет (Kd ниже).
    REQUIRE(seg.modifiers[1].state[0] > seg.modifiers[0].state[0]);
}

// ==================== eCB synthesis (86) ====================

TEST_CASE("eCB: Ca2+ drives 2-AG synthesis", "[cascade]") {
    Segment hi = {}; init_segment(&hi);
    hi.intra_ions[ION_CA] = 0.002f;
    hi.modifiers[0] = make_mod(MOD_ECB_SYNTHESIS);
    hi.modifier_count = 1;

    Segment lo = {}; init_segment(&lo);
    lo.modifiers[0] = make_mod(MOD_ECB_SYNTHESIS);
    lo.modifier_count = 1;

    for (int t = 0; t < 5000; t++) {
        ecb_synthesis_current(&hi.modifiers[0], &hi, 1.0f);
        ecb_synthesis_current(&lo.modifiers[0], &lo, 1.0f);
    }
    REQUIRE(hi.modifiers[0].state[0] > lo.modifiers[0].state[0]);
}

TEST_CASE("eCB: DAG enhances synthesis", "[cascade]") {
    Segment with_dag = {}; init_segment(&with_dag);
    with_dag.intra_ions[ION_CA] = 0.002f;
    with_dag.modifiers[0] = make_mod(MOD_PLC_IP3_DAG);
    with_dag.modifiers[0].state[1] = 1.0f;  // DAG высокий
    with_dag.modifiers[1] = make_mod(MOD_ECB_SYNTHESIS);
    with_dag.modifier_count = 2;

    Segment no_dag = {}; init_segment(&no_dag);
    no_dag.intra_ions[ION_CA] = 0.002f;
    no_dag.modifiers[0] = make_mod(MOD_ECB_SYNTHESIS);
    no_dag.modifier_count = 1;

    for (int t = 0; t < 5000; t++) {
        ecb_synthesis_current(&with_dag.modifiers[1], &with_dag, 1.0f);
        ecb_synthesis_current(&no_dag.modifiers[0], &no_dag, 1.0f);
    }
    REQUIRE(with_dag.modifiers[1].state[0] > no_dag.modifiers[0].state[0]);
}

// ==================== MAPK/ERK (87) ====================

TEST_CASE("MAPK/ERK: Ca2+ activates ERK cascade", "[cascade]") {
    Segment seg = {}; init_segment(&seg);
    seg.intra_ions[ION_CA] = 0.002f;
    seg.modifiers[0] = make_mod(MOD_MAPK_ERK);
    seg.modifier_count = 1;

    for (int t = 0; t < 50000; t++) mapk_erk_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] > 0.1f);  // ERK активирован
    REQUIRE(seg.modifiers[0].state[2] > 0.1f);  // Ras активирован
}

// ==================== IKK/NF-κB (88) ====================

TEST_CASE("IKK/NF-kB: Ca2+ activates NF-kB", "[cascade]") {
    Segment seg = {}; init_segment(&seg);
    seg.intra_ions[ION_CA] = 0.002f;
    seg.modifiers[0] = make_mod(MOD_IKK_NFKB);
    seg.modifier_count = 1;

    float sig_before = seg.transcription_signal;
    for (int t = 0; t < 50000; t++) ikk_nfkb_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] > 0.1f);       // NF-κB активирован
    REQUIRE(seg.transcription_signal > sig_before);  // сигнал записан
}

// ==================== mTORC1 (89) ====================

TEST_CASE("mTORC1: AA + ATP drives activation", "[cascade]") {
    Segment on = {}; init_segment(&on);
    on.modifiers[0] = make_mod(MOD_MTORC1);
    on.modifiers[0].state[3] = 1.0f;  // AA доступны
    on.ATP_level = 3.0f;
    on.modifier_count = 1;

    Segment low_atp = on;
    low_atp.modifiers[0] = make_mod(MOD_MTORC1);
    low_atp.modifiers[0].state[3] = 1.0f;
    low_atp.ATP_level = 0.0f;  // нет ATP → AMPK ингибирует

    for (int t = 0; t < 50000; t++) {
        mtorc1_current(&on.modifiers[0], &on, 1.0f);
        mtorc1_current(&low_atp.modifiers[0], &low_atp, 1.0f);
    }
    REQUIRE(on.modifiers[0].state[0] > low_atp.modifiers[0].state[0]);  // ATP-зависимость
}

// ==================== Protein synthesis (90) ====================

TEST_CASE("Protein synthesis: mTORC1 drives translation", "[cascade]") {
    Segment seg = {}; init_segment(&seg);
    seg.modifiers[0] = make_mod(MOD_MTORC1);
    seg.modifiers[0].state[0] = 1.0f;  // mTORC1 активен
    seg.modifiers[1] = make_mod(MOD_PROTEIN_SYNTH);
    seg.modifier_count = 2;

    for (int t = 0; t < 100000; t++) protein_synth_current(&seg.modifiers[1], &seg, 1.0f);
    REQUIRE(seg.modifiers[1].state[0] > 0.1f);  // трансляция идёт
}

// ==================== Cofilin (91) ====================

TEST_CASE("Cofilin: ERK drives LIMK → phospho-cofilin", "[cascade]") {
    Segment seg = {}; init_segment(&seg);
    seg.modifiers[0] = make_mod(MOD_MAPK_ERK);
    seg.modifiers[0].state[0] = 1.0f;  // ERK активен
    seg.modifiers[1] = make_mod(MOD_COFILIN);
    seg.modifier_count = 2;

    for (int t = 0; t < 50000; t++) cofilin_current(&seg.modifiers[1], &seg, 1.0f);
    REQUIRE(seg.modifiers[1].state[1] > 0.1f);  // LIMK активирован
    REQUIRE(seg.modifiers[1].state[2] > 0.1f);  // pCofilin вырос
}

// ==================== MMP9 (92) ====================

TEST_CASE("MMP9: ERK + NF-kB drive transcription", "[cascade]") {
    Segment seg = {}; init_segment(&seg);
    seg.modifiers[0] = make_mod(MOD_MAPK_ERK);
    seg.modifiers[0].state[3] = 1.0f;  // nuclear ERK
    seg.modifiers[1] = make_mod(MOD_IKK_NFKB);
    seg.modifiers[1].state[3] = 1.0f;  // nuclear NF-kB
    seg.modifiers[2] = make_mod(MOD_MMP9);
    seg.modifier_count = 3;

    for (int t = 0; t < 50000; t++) mmp9_current(&seg.modifiers[2], &seg, 1.0f);
    REQUIRE(seg.modifiers[2].state[0] > 0.1f);  // MMP9 активирован
}

// ==================== DARPP-32/PP1 (113) ====================

TEST_CASE("DARPP-32: PKA phosphorylates → inhibits PP1", "[cascade]") {
    Segment seg = {}; init_segment(&seg);
    seg.modifiers[0] = make_mod(MOD_CAMP_PKA);
    seg.modifiers[0].state[1] = 1.0f;  // PKA активна (честный пул)
    seg.modifiers[1] = make_mod(MOD_PP1_INHIBITOR_DARPP32);
    seg.modifier_count = 2;

    for (int t = 0; t < 10000; t++) pp1_inhibitor_darpp32_current(&seg.modifiers[1], &seg, 1.0f);
    REQUIRE(seg.modifiers[1].state[0] > 0.5f);  // p-DARPP32 вырос
    REQUIRE(seg.modifiers[1].state[3] < 0.2f);  // PP1 подавлен (basal 0.5 → <0.2)
}

TEST_CASE("DARPP-32: no PKA → PP1 stays basal", "[cascade]") {
    Segment seg = {}; init_segment(&seg);
    seg.modifiers[0] = make_mod(MOD_PP1_INHIBITOR_DARPP32);
    seg.modifier_count = 1;

    for (int t = 0; t < 10000; t++) pp1_inhibitor_darpp32_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] == Approx(0.0f).margin(1e-6f));  // нет PKA → нет p-DARPP32
    REQUIRE(seg.modifiers[0].state[3] == Approx(0.5f).margin(0.01f));  // PP1 = basal
}
