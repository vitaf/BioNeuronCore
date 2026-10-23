// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_nb_receptors.cpp — тесты NB_-рецепторов (Neuroblox-совместимых)
// + сома-модификаторов (ATP, Thermal, CREB)
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "types.h"
#include "modifier_interface.h"
#include <cmath>

// Forward declarations — extern из receptors_biological.cpp
extern double nb_nmda_moradi_full_current(Modifier*, Segment*, float);
extern double nb_ampa_sta_current(Modifier*, Segment*, float);
extern double nb_d1_modulator_current(Modifier*, Segment*, float);
extern double nb_d2_modulator_current(Modifier*, Segment*, float);
extern double nb_alpha7_er_nachr_current(Modifier*, Segment*, float);
extern double nb_beta2_nachr_current(Modifier*, Segment*, float);
extern double nb_ca_trpm4r_current(Modifier*, Segment*, float);
extern double nb_muscarinic_r_current(Modifier*, Segment*, float);
extern double nb_ampa_destexhe_current(Modifier*, Segment*, float);
extern double nb_nmda_destexhe_current(Modifier*, Segment*, float);
extern double nb_gabaa_destexhe_current(Modifier*, Segment*, float);
extern double nb_gabab_destexhe_current(Modifier*, Segment*, float);
extern double nb_msn_nmdar_current(Modifier*, Segment*, float);
extern double nb_msn_ampar_current(Modifier*, Segment*, float);
extern double nb_soma_atp_current(Modifier*, Segment*, float);
extern double nb_soma_thermal_current(Modifier*, Segment*, float);
extern double nb_creb_transcription_current(Modifier*, Segment*, float);

// Хелпер: инициализация сегмента со стандартными ионными концентрациями
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
    s->ATP_level = 1.0f;
}

static Modifier make_mod(uint8_t type, float density = 1.0f) {
    Modifier m = {};
    m.type = type;
    m.density = density;
    m.active_vars = 0;
    return m;
}

// ==================== NMDA (63, 72) ====================

TEST_CASE("NB_NMDA: activates on glutamate, no current without NT", "[nb_receptors]") {
    init_biological_library();

    Segment seg = {}; init_segment(&seg);
    seg.neurotransmitter[NT_GLUTAMATE] = 1.0f;
    seg.modifiers[0] = make_mod(MOD_NB_NMDA_MORADI_FULL);
    seg.modifier_count = 1;

    for (int t = 0; t < 200; t++) nb_nmda_moradi_full_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] > 0.0f);  // r активирован

    Segment no_nt = {}; init_segment(&no_nt);
    no_nt.modifiers[0] = make_mod(MOD_NB_NMDA_MORADI_FULL);
    no_nt.modifier_count = 1;
    for (int t = 0; t < 200; t++) nb_nmda_moradi_full_current(&no_nt.modifiers[0], &no_nt, 1.0f);
    REQUIRE(no_nt.modifiers[0].state[0] == Approx(0.0f).margin(1e-6f));
}

TEST_CASE("NB_NMDA: Mg2+ block reduces current", "[nb_receptors]") {
    init_biological_library();

    Segment lo = {}; init_segment(&lo);
    lo.neurotransmitter[NT_GLUTAMATE] = 1.0f;
    lo.extra_ions[ION_MG] = 1.0f;
    lo.modifiers[0] = make_mod(MOD_NB_NMDA_MORADI_FULL);
    lo.modifier_count = 1;

    Segment hi = lo;
    hi.extra_ions[ION_MG] = 5.0f;  // высокий Mg²⁺ → сильнее Woodhull-блок

    for (int t = 0; t < 200; t++) {
        nb_nmda_moradi_full_current(&lo.modifiers[0], &lo, 1.0f);
        nb_nmda_moradi_full_current(&hi.modifiers[0], &hi, 1.0f);
    }
    float I_lo = nb_nmda_moradi_full_current(&lo.modifiers[0], &lo, 1.0f);
    float I_hi = nb_nmda_moradi_full_current(&hi.modifiers[0], &hi, 1.0f);
    REQUIRE(fabsf(I_lo) > fabsf(I_hi));
}

TEST_CASE("NB_NMDA_Destexhe: activates on glutamate", "[nb_receptors]") {
    init_biological_library();

    Segment seg = {}; init_segment(&seg);
    seg.neurotransmitter[NT_GLUTAMATE] = 1.0f;
    seg.modifiers[0] = make_mod(MOD_NB_NMDA_DESTEXHE);
    seg.modifier_count = 1;

    for (int t = 0; t < 200; t++) nb_nmda_destexhe_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] > 0.0f);
}

// ==================== AMPA (64, 71) ====================

TEST_CASE("NB_AMPA: activates on glutamate", "[nb_receptors]") {
    init_biological_library();

    Segment seg = {}; init_segment(&seg);
    seg.neurotransmitter[NT_GLUTAMATE] = 1.0f;
    seg.modifiers[0] = make_mod(MOD_NB_AMPA_STA);
    seg.modifier_count = 1;

    for (int t = 0; t < 50; t++) nb_ampa_sta_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] > 0.0f);

    Segment no_nt = {}; init_segment(&no_nt);
    no_nt.modifiers[0] = make_mod(MOD_NB_AMPA_DESTEXHE);
    no_nt.modifier_count = 1;
    for (int t = 0; t < 50; t++) nb_ampa_destexhe_current(&no_nt.modifiers[0], &no_nt, 1.0f);
    REQUIRE(no_nt.modifiers[0].state[0] == Approx(0.0f).margin(1e-6f));
}

// ==================== nAChR (67, 68) ====================

TEST_CASE("NB_Alpha7_nAChR: activates on ACh", "[nb_receptors]") {
    init_biological_library();

    Segment seg = {}; init_segment(&seg);
    seg.neurotransmitter[NT_ACETYLCHOLINE] = 10.0f;
    seg.modifiers[0] = make_mod(MOD_NB_ALPHA7_ER_NACHR);
    seg.modifier_count = 1;

    for (int t = 0; t < 50; t++) nb_alpha7_er_nachr_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] > 0.0f);
}

TEST_CASE("NB_Beta2_nAChR: activates on ACh", "[nb_receptors]") {
    init_biological_library();

    Segment seg = {}; init_segment(&seg);
    seg.neurotransmitter[NT_ACETYLCHOLINE] = 10.0f;
    seg.modifiers[0] = make_mod(MOD_NB_BETA2_NACHR);
    seg.modifier_count = 1;

    for (int t = 0; t < 20; t++) nb_beta2_nachr_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] > 0.0f);
}

// ==================== TRPM4 (69) ====================

TEST_CASE("NB_TRPM4: gates on intracellular Ca2+", "[nb_receptors]") {
    init_biological_library();

    Segment hi = {}; init_segment(&hi);
    hi.intra_ions[ION_CA] = 0.002f;  // 2 µM — выше EC50 0.5 µM
    hi.modifiers[0] = make_mod(MOD_NB_CA_TRPM4R);
    hi.modifier_count = 1;

    Segment lo = {}; init_segment(&lo);
    lo.intra_ions[ION_CA] = 0.00005f;  // 0.05 µM — ниже EC50
    lo.modifiers[0] = make_mod(MOD_NB_CA_TRPM4R);
    lo.modifier_count = 1;

    for (int t = 0; t < 200; t++) {
        nb_ca_trpm4r_current(&hi.modifiers[0], &hi, 1.0f);
        nb_ca_trpm4r_current(&lo.modifiers[0], &lo, 1.0f);
    }
    REQUIRE(hi.modifiers[0].state[0] > lo.modifiers[0].state[0]);  // выше Ca²⁺ → выше gate
}

// ==================== Muscarinic (70) ====================

TEST_CASE("NB_MuscarinicR: Gq activation on ACh, no direct current", "[nb_receptors]") {
    init_biological_library();

    Segment seg = {}; init_segment(&seg);
    seg.neurotransmitter[NT_ACETYLCHOLINE] = 1.0f;
    seg.modifiers[0] = make_mod(MOD_NB_MUSCARINIC_R);
    seg.modifier_count = 1;

    for (int t = 0; t < 200; t++) nb_muscarinic_r_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] > 0.0f);  // Gαq активирован
    REQUIRE(nb_muscarinic_r_current(&seg.modifiers[0], &seg, 1.0f) == 0.0f);  // сигнальный
}

// ==================== GABAA (73) / GABAB (74) ====================

TEST_CASE("NB_GABAA: activates on GABA", "[nb_receptors]") {
    init_biological_library();

    Segment seg = {}; init_segment(&seg);
    seg.neurotransmitter[NT_GABA] = 1.0f;
    seg.modifiers[0] = make_mod(MOD_NB_GABAA_DESTEXHE);
    seg.modifier_count = 1;

    for (int t = 0; t < 50; t++) nb_gabaa_destexhe_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] > 0.0f);
}

TEST_CASE("NB_GABAB: GIRK activation on GABA", "[nb_receptors]") {
    init_biological_library();

    Segment seg = {}; init_segment(&seg);
    seg.neurotransmitter[NT_GABA] = 1.0f;
    seg.modifiers[0] = make_mod(MOD_NB_GABAB_DESTEXHE);
    seg.modifier_count = 1;

    for (int t = 0; t < 200; t++) nb_gabab_destexhe_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] > 0.0f);  // GIRK r активирован
}

// ==================== D1/D2 Modulators (65, 66) ====================

TEST_CASE("NB_D1_Modulator: computes M_NMDA1 from dopamine", "[nb_receptors]") {
    init_biological_library();

    Segment seg = {}; init_segment(&seg);
    seg.neurotransmitter[NT_DOPAMINE] = 1.0f;
    seg.modifiers[0] = make_mod(MOD_NB_D1_MODULATOR);
    seg.modifier_count = 1;

    for (int t = 0; t < 500; t++) nb_d1_modulator_current(&seg.modifiers[0], &seg, 1.0f);

    float phi1 = seg.modifiers[0].state[0];
    REQUIRE(phi1 > 0.5f);
    REQUIRE(phi1 <= 1.0f);

    float M = seg.modifiers[0].state[1];
    REQUIRE(M > 1.0f);
    REQUIRE(M < 1.5f);
}

TEST_CASE("NB_D2_Modulator: M_AMPA2 attenuates with dopamine", "[nb_receptors]") {
    init_biological_library();

    Segment seg = {}; init_segment(&seg);
    seg.neurotransmitter[NT_DOPAMINE] = 1.0f;
    seg.modifiers[0] = make_mod(MOD_NB_D2_MODULATOR);
    seg.modifier_count = 1;

    for (int t = 0; t < 500; t++) nb_d2_modulator_current(&seg.modifiers[0], &seg, 1.0f);

    float M = seg.modifiers[0].state[1];
    REQUIRE(M < 1.0f);
    REQUIRE(M > 0.5f);
}

TEST_CASE("NB_D1: no current generated (modulator only)", "[nb_receptors]") {
    init_biological_library();

    Segment seg = {}; init_segment(&seg);
    seg.neurotransmitter[NT_DOPAMINE] = 1.0f;
    seg.modifiers[0] = make_mod(MOD_NB_D1_MODULATOR);
    seg.modifier_count = 1;

    float I = nb_d1_modulator_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(I == 0.0f);
}

// ==================== MsnNMDAR (75) / MsnAMPAR (76) ====================

TEST_CASE("NB_MsnNMDAR: D1 enhances NMDA current", "[nb_receptors]") {
    init_biological_library();

    Segment seg = {}; init_segment(&seg);
    seg.neurotransmitter[NT_GLUTAMATE] = 1.0f;
    seg.neurotransmitter[NT_DOPAMINE] = 1.0f;
    seg.modifiers[0] = make_mod(MOD_NB_D1_MODULATOR);
    seg.modifiers[1] = make_mod(MOD_NB_MSN_NMDAR);
    seg.modifier_count = 2;

    Segment ctrl = {}; init_segment(&ctrl);
    ctrl.neurotransmitter[NT_GLUTAMATE] = 1.0f;
    ctrl.modifiers[0] = make_mod(MOD_NB_MSN_NMDAR);
    ctrl.modifier_count = 1;

    for (int t = 0; t < 200; t++) nb_d1_modulator_current(&seg.modifiers[0], &seg, 1.0f);

    for (int t = 0; t < 100; t++) {
        nb_msn_nmdar_current(&seg.modifiers[1], &seg, 1.0f);
        nb_msn_nmdar_current(&ctrl.modifiers[0], &ctrl, 1.0f);
    }

    float I_d1 = fabsf(nb_msn_nmdar_current(&seg.modifiers[1], &seg, 1.0f));
    float I_ctrl = fabsf(nb_msn_nmdar_current(&ctrl.modifiers[0], &ctrl, 1.0f));
    REQUIRE(I_d1 > I_ctrl);  // D1 усиливает NMDA
}

TEST_CASE("NB_MsnAMPAR: D2 suppresses AMPA current", "[nb_receptors]") {
    init_biological_library();

    Segment seg = {}; init_segment(&seg);
    seg.neurotransmitter[NT_GLUTAMATE] = 1.0f;
    seg.neurotransmitter[NT_DOPAMINE] = 1.0f;
    seg.modifiers[0] = make_mod(MOD_NB_D2_MODULATOR);
    seg.modifiers[1] = make_mod(MOD_NB_MSN_AMPAR);
    seg.modifier_count = 2;

    Segment ctrl = {}; init_segment(&ctrl);
    ctrl.neurotransmitter[NT_GLUTAMATE] = 1.0f;
    ctrl.modifiers[0] = make_mod(MOD_NB_MSN_AMPAR);
    ctrl.modifier_count = 1;

    for (int t = 0; t < 200; t++) nb_d2_modulator_current(&seg.modifiers[0], &seg, 1.0f);

    for (int t = 0; t < 100; t++) {
        nb_msn_ampar_current(&seg.modifiers[1], &seg, 1.0f);
        nb_msn_ampar_current(&ctrl.modifiers[0], &ctrl, 1.0f);
    }

    float I_d2 = fabsf(nb_msn_ampar_current(&seg.modifiers[1], &seg, 1.0f));
    float I_ctrl = fabsf(nb_msn_ampar_current(&ctrl.modifiers[0], &ctrl, 1.0f));
    REQUIRE(I_d2 < I_ctrl);  // D2 подавляет AMPA
}

// ==================== NB_SomaATP (77) ====================

TEST_CASE("NB_SomaATP: synthesizes ATP", "[nb_soma]") {
    init_biological_library();

    Segment seg = {}; init_segment(&seg);
    seg.ATP_level = 0.5f;
    seg.modifiers[0] = make_mod(MOD_NB_SOMA_ATP);
    seg.modifiers[0].state[1] = 0.5f;
    seg.modifier_count = 1;

    float atp_before = seg.ATP_level;
    for (int t = 0; t < 500; t++) nb_soma_atp_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.ATP_level > atp_before);
}

TEST_CASE("NB_SomaATP: feedback inhibition at high ATP", "[nb_soma]") {
    init_biological_library();

    Segment seg = {}; init_segment(&seg);
    seg.ATP_level = 4.9f;
    seg.modifiers[0] = make_mod(MOD_NB_SOMA_ATP);
    seg.modifiers[0].state[1] = 0.5f;
    seg.modifier_count = 1;

    for (int t = 0; t < 500; t++) nb_soma_atp_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.ATP_level <= 5.0f);
}

TEST_CASE("NB_SomaATP: generates heat", "[nb_soma]") {
    init_biological_library();

    Segment seg = {}; init_segment(&seg);
    seg.temperature = 37.0f;
    seg.ATP_level = 0.5f;
    seg.modifiers[0] = make_mod(MOD_NB_SOMA_ATP);
    seg.modifiers[0].state[1] = 0.5f;
    seg.modifier_count = 1;

    float T_before = seg.temperature;
    for (int t = 0; t < 500; t++) nb_soma_atp_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.temperature >= T_before);
}

// ==================== NB_SomaThermal (78) ====================

TEST_CASE("NB_SomaThermal: homeostasis at 37C", "[nb_soma]") {
    init_biological_library();

    Segment hot = {}; init_segment(&hot);
    hot.temperature = 41.0f;
    hot.modifiers[0] = make_mod(MOD_NB_SOMA_THERMAL);
    hot.modifier_count = 1;

    Segment cold = {}; init_segment(&cold);
    cold.temperature = 33.0f;
    cold.modifiers[0] = make_mod(MOD_NB_SOMA_THERMAL);
    cold.modifier_count = 1;

    for (int t = 0; t < 1000; t++) {
        nb_soma_thermal_current(&hot.modifiers[0], &hot, 1.0f);
        nb_soma_thermal_current(&cold.modifiers[0], &cold, 1.0f);
    }

    REQUIRE(hot.temperature < 41.0f);
    REQUIRE(cold.temperature > 33.0f);
}

TEST_CASE("NB_SomaThermal: heat stress accumulates", "[nb_soma]") {
    init_biological_library();

    Segment seg = {}; init_segment(&seg);
    seg.temperature = 40.0f;
    seg.modifiers[0] = make_mod(MOD_NB_SOMA_THERMAL);
    seg.modifier_count = 1;

    for (int t = 0; t < 2000; t++) nb_soma_thermal_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] > 0.0f);
}

// ==================== NB_CREB_Transcription (79) ====================

TEST_CASE("NB_CREB_Transcription: Ca2+ activates pCREB", "[nb_soma]") {
    init_biological_library();

    Segment seg = {}; init_segment(&seg);
    seg.intra_ions[ION_CA] = 0.5f;
    seg.modifiers[0] = make_mod(MOD_NB_CREB_TRANSCRIPTION);
    seg.modifier_count = 1;

    float pCREB_start = seg.modifiers[0].state[0];
    for (int t = 0; t < 10000; t++) nb_creb_transcription_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] > pCREB_start);
}

TEST_CASE("NB_CREB_Transcription: writes transcription_signal", "[nb_soma]") {
    init_biological_library();

    Segment seg = {}; init_segment(&seg);
    seg.intra_ions[ION_CA] = 0.5f;
    seg.modifiers[0] = make_mod(MOD_NB_CREB_TRANSCRIPTION);
    seg.modifier_count = 1;

    for (int t = 0; t < 10000; t++) nb_creb_transcription_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.transcription_signal > 0.0f);
    REQUIRE(seg.transcription_signal <= 1.0f);
}

TEST_CASE("NB_CREB_Transcription: late genes lag behind pCREB", "[nb_soma]") {
    init_biological_library();

    Segment seg = {}; init_segment(&seg);
    seg.intra_ions[ION_CA] = 0.5f;
    seg.modifiers[0] = make_mod(MOD_NB_CREB_TRANSCRIPTION);
    seg.modifier_count = 1;

    for (int t = 0; t < 10000; t++) nb_creb_transcription_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] > seg.modifiers[0].state[1]);
}

TEST_CASE("NB_CREB_Transcription: low Ca2+ = no activation", "[nb_soma]") {
    init_biological_library();

    Segment seg = {}; init_segment(&seg);
    seg.intra_ions[ION_CA] = 0.0001f;
    seg.modifiers[0] = make_mod(MOD_NB_CREB_TRANSCRIPTION);
    seg.modifier_count = 1;

    for (int t = 0; t < 5000; t++) nb_creb_transcription_current(&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(seg.modifiers[0].state[0] < 0.01f);
}
