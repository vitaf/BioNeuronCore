// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include "types.h"
#include "modifier_interface.h"
#include <cmath>
#include <cstring>

// Прототипы из simulation_host.cpp (19 параметров — стимулы в Core с v0.8.0)
extern "C" {
    void core_run_tick(Segment* segments, uint32_t num_segments,
                       IntraConnection* connections, uint32_t num_connections,
                       Synapse* synapses, uint32_t num_synapses,
                       uint8_t mode, uint64_t* tick_count, float dt,
                       CurrentStimulus* stimuli, uint32_t* stimulus_count,
                       VoltageClamp* vclamps, uint32_t* vclamp_count,
                       RampStimulus* ramps, uint32_t* ramp_count,
                       IonInjection* ion_inj, uint32_t* ion_inj_count,
                       LigandInjection* lig_inj, uint32_t* lig_inj_count, MechanoStimulus* mechano, uint32_t* mechano_count);
    const char* core_version_string();
    uint32_t core_version_major();
    uint32_t core_version_minor();
    uint32_t core_version_patch();
}

#define STIM_NULL nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr

// ═══════════════════════════════════════════════════════════════════
// Версионирование
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("Core: Version string", "[simulation_host]") {
    const char* v = core_version_string();
    REQUIRE(v != nullptr);
    REQUIRE(strlen(v) > 0);
}

TEST_CASE("Core: Version numbers", "[simulation_host]") {
    REQUIRE(core_version_major() == 0);
    REQUIRE(core_version_minor() >= 5);
    REQUIRE(core_version_patch() >= 0);
}

// ═══════════════════════════════════════════════════════════════════
// Базовое обновление V_m (шаг 1 + шаг 4)
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("CPU: V_m stays within bounds", "[simulation_host]") {
    init_biological_library();

    Segment seg = {};
    seg.id = 1;
    seg.V_m = -65.0f;
    seg.intra_ions[ION_NA] = 10.0f;   seg.extra_ions[ION_NA] = 140.0f;
    seg.intra_ions[ION_K]  = 140.0f;  seg.extra_ions[ION_K]  = 4.0f;
    seg.intra_ions[ION_CL] = 10.0f;   seg.extra_ions[ION_CL] = 120.0f;
    seg.length = 100.0f;
    seg.radius = 10.0f;
    seg.ATP_level = 1.0f;
    seg.modifiers[0] = {MOD_AXON_NAV12, {0,0,0}, 15.6f, {0.05f, 0.6f}, 2, 0, {0,0}};
    seg.modifiers[1] = {MOD_PUMP_NAK, {0,0,0}, 1.0f, {}, 0, 0, {0,0}};
    seg.modifier_count = 2;

    uint64_t tick = 0;
    for (int i = 0; i < 100; i++) {
        core_run_tick(&seg, 1, nullptr, 0, nullptr, 0, MODE_BIOLOGICAL, &tick, 0.01f, STIM_NULL, nullptr, nullptr);
    }

    REQUIRE(seg.V_m >= -200.0f);
    REQUIRE(seg.V_m <= 100.5f);
}

// ═══════════════════════════════════════════════════════════════════
// I_buffer сбрасывается после тика
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("CPU: I_buffer resets after tick", "[simulation_host]") {
    init_biological_library();

    Segment seg = {};
    seg.id = 1;
    seg.V_m = -65.0f;
    seg.intra_ions[ION_NA] = 10.0f;   seg.extra_ions[ION_NA] = 140.0f;
    seg.intra_ions[ION_K]  = 140.0f;  seg.extra_ions[ION_K]  = 4.0f;
    seg.length = 100.0f;
    seg.radius = 10.0f;
    seg.ATP_level = 1.0f;
    seg.modifiers[0] = {MOD_AXON_NAV12, {0,0,0}, 15.6f, {0.05f, 0.6f}, 2, 0, {0,0}};
    seg.modifier_count = 1;

    uint64_t tick = 0;
    core_run_tick(&seg, 1, nullptr, 0, nullptr, 0, MODE_BIOLOGICAL, &tick, 0.01f, STIM_NULL, nullptr, nullptr);

    REQUIRE(seg.I_buffer == Approx(0.0f).margin(0.001f));
}

// ═══════════════════════════════════════════════════════════════════
// Так увеличивается
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("CPU: Tick increments", "[simulation_host]") {
    init_biological_library();

    Segment seg = {};
    seg.id = 1;
    seg.V_m = -65.0f;
    seg.length = 100.0f;
    seg.radius = 10.0f;

    uint64_t tick = 0;
    core_run_tick(&seg, 1, nullptr, 0, nullptr, 0, MODE_BIOLOGICAL, &tick, 0.01f, STIM_NULL, nullptr, nullptr);
    REQUIRE(tick == 1);

    core_run_tick(&seg, 1, nullptr, 0, nullptr, 0, MODE_BIOLOGICAL, &tick, 0.01f, STIM_NULL, nullptr, nullptr);
    REQUIRE(tick == 2);
}

// ═══════════════════════════════════════════════════════════════════
// Осевые токи между сегментами (шаг 2)
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("CPU: Axial current flows between segments", "[simulation_host]") {
    init_biological_library();

    const int N = 2;
    Segment segs[N];
    IntraConnection conns[1];

    for (int i = 0; i < N; i++) {
        segs[i] = {};
        segs[i].id = (uint32_t)(i + 1);
        segs[i].V_m = (i == 0) ? -50.0f : -70.0f;
        segs[i].length = 100.0f;
        segs[i].radius = 10.0f;
        segs[i].ATP_level = 1.0f;
        // ионы нужны для Нернст-Планка (осевой заряд-флюкс через градиенты)
        segs[i].intra_ions[ION_NA] = 10.0f;  segs[i].extra_ions[ION_NA] = 140.0f;
        segs[i].intra_ions[ION_K]  = 140.0f; segs[i].extra_ions[ION_K]  = 4.0f;
        segs[i].intra_ions[ION_CL] = 10.0f;  segs[i].extra_ions[ION_CL] = 120.0f;
    }

    conns[0] = {0, 1, 1};

    uint64_t tick = 0;
    core_run_tick(segs, N, conns, 1, nullptr, 0, MODE_BIOLOGICAL, &tick, 0.01f, STIM_NULL, nullptr, nullptr);

    float diff_after = segs[0].V_m - segs[1].V_m;
    REQUIRE(diff_after < 20.0f);
}

// ═══════════════════════════════════════════════════════════════════
// Oсевые токи с нулевым числом соединений не падают
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("CPU: Zero connections", "[simulation_host]") {
    init_biological_library();

    Segment seg = {};
    seg.id = 1;
    seg.V_m = -65.0f;
    seg.length = 100.0f;
    seg.radius = 10.0f;
    seg.ATP_level = 1.0f;

    uint64_t tick = 0;
    REQUIRE_NOTHROW(core_run_tick(nullptr, 0, nullptr, 0, nullptr, 0, MODE_BIOLOGICAL, &tick, 0.01f, STIM_NULL, nullptr, nullptr));
    REQUIRE_NOTHROW(core_run_tick(&seg, 1, nullptr, 0, nullptr, 0, MODE_BIOLOGICAL, &tick, 0.01f, STIM_NULL, nullptr, nullptr));
}

// ═══════════════════════════════════════════════════════════════════
// Синаптическая передача (шаг 3)
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("CPU: Synaptic transmission", "[simulation_host]") {
    init_biological_library();

    Segment segs[2] = {};
    segs[0].id = 1;
    segs[0].V_m = 0.0f;
    segs[0].length = 100.0f;
    segs[0].radius = 10.0f;
    segs[0].ATP_level = 1.0f;

    segs[1].id = 2;
    segs[1].V_m = -65.0f;
    segs[1].intra_ions[ION_NA] = 10.0f;   segs[1].extra_ions[ION_NA] = 140.0f;
    segs[1].intra_ions[ION_K]  = 140.0f;  segs[1].extra_ions[ION_K]  = 4.0f;
    segs[1].intra_ions[ION_CL] = 10.0f;   segs[1].extra_ions[ION_CL] = 120.0f;
    segs[1].length = 100.0f;
    segs[1].radius = 10.0f;
    segs[1].ATP_level = 1.0f;
    segs[1].modifiers[0] = {MOD_NT_RECEPTOR_AMPA, {0,0,0}, 1.0f, {}, 0, 0, {0,0}};
    segs[1].modifier_count = 1;

    Synapse syn = {};
    syn.id = 1;
    syn.pre_segment = 0;
    syn.post_segment = 1;
    syn.type = SYNAPSE_TYPE_CHEMICAL;
    syn.weight = 1.0f;
    syn.release_probability = 1.0f;
    syn.release_amount = 1.0f;
    syn.tau_clearance = 10.0f;
    syn.bouton_Ca = 0.1f;  // 100 µM — выше SYT_KD (10 µM) → Ca²⁺-зависимый выброс

    uint64_t tick = 0;
    core_run_tick(segs, 2, nullptr, 0, &syn, 1, MODE_BIOLOGICAL, &tick, 0.01f, STIM_NULL, nullptr, nullptr);

    REQUIRE(syn.cleft_neurotransmitter[NT_GLUTAMATE] > 0.0f);
}

// ═══════════════════════════════════════════════════════════════════
// Синапс: рефрактерный период
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("CPU: Synapse refractory period", "[simulation_host]") {
    init_biological_library();

    Segment segs[2] = {};
    segs[0].id = 1;
    segs[0].V_m = 0.0f;
    segs[0].length = 100.0f;
    segs[0].radius = 10.0f;
    segs[0].ATP_level = 1.0f;

    segs[1].id = 2;
    segs[1].V_m = -65.0f;
    segs[1].length = 100.0f;
    segs[1].radius = 10.0f;
    segs[1].ATP_level = 1.0f;

    Synapse syn = {};
    syn.id = 1;
    syn.pre_segment = 0;
    syn.post_segment = 1;
    syn.type = SYNAPSE_TYPE_CHEMICAL;
    syn.release_probability = 1.0f;
    syn.release_amount = 1.0f;
    syn.tau_clearance = 10.0f;
    syn.bouton_Ca = 0.1f;  // 100 µM — выше SYT_KD (10 µM) → Ca²⁺-зависимый выброс

    uint64_t tick = 0;

    core_run_tick(segs, 2, nullptr, 0, &syn, 1, MODE_BIOLOGICAL, &tick, 0.01f, STIM_NULL, nullptr, nullptr);
    float nt_after_first = syn.cleft_neurotransmitter[NT_GLUTAMATE];
    REQUIRE(syn.refractory > 0);

    segs[0].V_m = 0.0f;
    core_run_tick(segs, 2, nullptr, 0, &syn, 1, MODE_BIOLOGICAL, &tick, 0.01f, STIM_NULL, nullptr, nullptr);
    float nt_after_second = syn.cleft_neurotransmitter[NT_GLUTAMATE];
    REQUIRE(nt_after_second < nt_after_first);
}

// ═══════════════════════════════════════════════════════════════════
// Миелин блокирует ионотропные токи (шаг 1)
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("CPU: Myelin blocks ionotropic currents", "[simulation_host]") {
    init_biological_library();

    Segment seg_no_myelin = {};
    seg_no_myelin.id = 1;
    // Деполяризованное состояние: Na-каналы открыты, значительный ток
    seg_no_myelin.V_m = -20.0f;
    seg_no_myelin.intra_ions[ION_NA] = 10.0f;   seg_no_myelin.extra_ions[ION_NA] = 140.0f;
    seg_no_myelin.intra_ions[ION_K]  = 140.0f;  seg_no_myelin.extra_ions[ION_K]  = 4.0f;
    seg_no_myelin.length = 100.0f;
    seg_no_myelin.radius = 10.0f;
    seg_no_myelin.ATP_level = 1.0f;
    seg_no_myelin.modifiers[0] = {MOD_KV11, {0,0,0}, 5.0f, {0.8f, 1.0f}, 2, 0, {0,0}};
    seg_no_myelin.modifier_count = 1;

    // Миелиновый сегмент: тот же ID (тот же тепловой шум), + MOD_MYELIN
    Segment seg_myelin = seg_no_myelin;
    seg_myelin.id = 1;
    seg_myelin.modifiers[1] = {MOD_MYELIN, {0,0,0}, 1.0f, {}, 0, 0, {0,0}};
    seg_myelin.modifier_count = 2;

    // Много тиков: ток накапливается, шум усредняется
    uint64_t tick = 0;
    float start_no = seg_no_myelin.V_m;
    float start_myelin = seg_myelin.V_m;
    for (int t = 0; t < 100; t++) {
        core_run_tick(&seg_no_myelin, 1, nullptr, 0, nullptr, 0, MODE_BIOLOGICAL, &tick, 0.01f, STIM_NULL, nullptr, nullptr);
    }
    tick = 0;
    for (int t = 0; t < 100; t++) {
        core_run_tick(&seg_myelin, 1, nullptr, 0, nullptr, 0, MODE_BIOLOGICAL, &tick, 0.01f, STIM_NULL, nullptr, nullptr);
    }
    float dV_no_myelin = seg_no_myelin.V_m - start_no;
    float dV_myelin = seg_myelin.V_m - start_myelin;

    // Миелин блокирует ионотропные токи — 100 тиков накопления
    REQUIRE(std::fabs(dV_myelin) < std::fabs(dV_no_myelin));
}

// ═══════════════════════════════════════════════════════════════════
// ARBITRARY режим
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("CPU: Arbitrary mode", "[simulation_host]") {
    init_biological_library();
    init_arbitrary_library();

    Segment seg = {};
    seg.id = 1;
    seg.V_m = -65.0f;
    seg.length = 100.0f;
    seg.radius = 10.0f;
    seg.ATP_level = 1.0f;
    seg.modifiers[0] = {MOD_AXON_NAV12, {0,0,0}, 15.6f, {0.05f, 0.6f}, 2, 0, {0,0}};
    seg.modifier_count = 1;

    uint64_t tick = 0;
    REQUIRE_NOTHROW(core_run_tick(&seg, 1, nullptr, 0, nullptr, 0, MODE_ARBITRARY, &tick, 0.01f, STIM_NULL, nullptr, nullptr));
}

// ═══════════════════════════════════════════════════════════════════
// Диффузия ионов между сегментами (шаг 2.5)
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("CPU: Ion diffusion between connected segments", "[simulation_host]") {
    init_biological_library();

    Segment segs[2] = {};
    segs[0].id = 1;
    segs[0].V_m = -65.0f;
    segs[0].intra_ions[ION_NA] = 20.0f;
    segs[0].intra_ions[ION_K]  = 140.0f;
    segs[0].length = 100.0f;
    segs[0].radius = 10.0f;
    segs[0].ATP_level = 1.0f;

    segs[1].id = 2;
    segs[1].V_m = -65.0f;
    segs[1].intra_ions[ION_NA] = 10.0f;
    segs[1].intra_ions[ION_K]  = 140.0f;
    segs[1].length = 100.0f;
    segs[1].radius = 10.0f;
    segs[1].ATP_level = 1.0f;

    IntraConnection conn = {0, 1, 1};

    uint64_t tick = 0;
    // диффузия за 1 тик крошечная (ниже float-точности у 20.0) → прогоняем 1000 тиков (10 мс)
    for (int t = 0; t < 1000; t++)
        core_run_tick(segs, 2, &conn, 1, nullptr, 0, MODE_BIOLOGICAL, &tick, 0.01f, STIM_NULL, nullptr, nullptr);

    REQUIRE(segs[0].intra_ions[ION_NA] < 20.0f);
    REQUIRE(segs[1].intra_ions[ION_NA] > 10.0f);
}

// ═══════════════════════════════════════════════════════════════════
// ATP диффузия
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("CPU: ATP diffusion", "[simulation_host]") {
    init_biological_library();

    Segment segs[2] = {};
    segs[0].id = 1;
    segs[0].V_m = -65.0f;
    segs[0].ATP_level = 2.0f;
    segs[0].length = 100.0f;
    segs[0].radius = 10.0f;

    segs[1].id = 2;
    segs[1].V_m = -65.0f;
    segs[1].ATP_level = 1.0f;
    segs[1].length = 100.0f;
    segs[1].radius = 10.0f;

    IntraConnection conn = {0, 1, 1};

    uint64_t tick = 0;
    core_run_tick(segs, 2, &conn, 1, nullptr, 0, MODE_BIOLOGICAL, &tick, 0.01f, STIM_NULL, nullptr, nullptr);

    REQUIRE(segs[0].ATP_level < 2.0f);
    REQUIRE(segs[1].ATP_level > 1.0f);
}

// ═══════════════════════════════════════════════════════════════════
// Теплопроводность
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("CPU: Heat diffusion", "[simulation_host]") {
    init_biological_library();

    Segment segs[2] = {};
    segs[0].id = 1;
    segs[0].V_m = -65.0f;
    segs[0].temperature = 315.0f;
    segs[0].length = 100.0f;
    segs[0].radius = 10.0f;

    segs[1].id = 2;
    segs[1].V_m = -65.0f;
    segs[1].temperature = 310.0f;
    segs[1].length = 100.0f;
    segs[1].radius = 10.0f;

    IntraConnection conn = {0, 1, 1};

    uint64_t tick = 0;
    for (int i = 0; i < 100; i++) {
        core_run_tick(segs, 2, &conn, 1, nullptr, 0, MODE_BIOLOGICAL, &tick, 0.01f, STIM_NULL, nullptr, nullptr);
    }

    REQUIRE(segs[0].temperature <= 315.0f);
    REQUIRE(segs[1].temperature >= 310.0f);
}

// ═══════════════════════════════════════════════════════════════════
// V_m клиампинг на границах
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("CPU: V_m clamped to bounds", "[simulation_host]") {
    init_biological_library();

    Segment seg = {};
    seg.id = 1;
    seg.V_m = 150.0f;
    seg.length = 100.0f;
    seg.radius = 10.0f;
    seg.ATP_level = 1.0f;
    // Ионы нужны насосу для работы (иначе na_factor = k_factor = 0)
    seg.intra_ions[ION_NA] = 10.0f;  seg.extra_ions[ION_NA] = 140.0f;
    seg.intra_ions[ION_K]  = 140.0f; seg.extra_ions[ION_K]  = 4.0f;
    seg.intra_ions[ION_CL] = 10.0f;  seg.extra_ions[ION_CL] = 120.0f;
    seg.modifiers[0] = {MOD_PUMP_NAK, {0,0,0}, 1.0f, {}, 0, 0, {0,0}};
    seg.modifier_count = 1;

    uint64_t tick = 0;
    core_run_tick(&seg, 1, nullptr, 0, nullptr, 0, MODE_BIOLOGICAL, &tick, 0.01f, STIM_NULL, nullptr, nullptr);

    // После coordinated switch насос даёт ~2.75 pA → ΔV≈0.0004 mV/тик при dt=0.01ms.
    // V_m остаётся у верхней границы clamp, но не превышает её и не уходит в NaN.
    REQUIRE(seg.V_m <= 150.0f);
    REQUIRE(seg.V_m >= -200.0f);
    REQUIRE_FALSE(std::isnan(seg.V_m));
}

// ═══════════════════════════════════════════════════════════════════
// Инвалидные индексы IntraConnection игнорируются
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("CPU: Invalid connection indices ignored", "[simulation_host]") {
    init_biological_library();

    Segment seg = {};
    seg.id = 1;
    seg.V_m = -65.0f;
    seg.length = 100.0f;
    seg.radius = 10.0f;
    seg.ATP_level = 1.0f;

    IntraConnection conn = {0, 99, 1};

    uint64_t tick = 0;
    REQUIRE_NOTHROW(core_run_tick(&seg, 1, &conn, 1, nullptr, 0, MODE_BIOLOGICAL, &tick, 0.01f, STIM_NULL, nullptr, nullptr));
}

// ═══════════════════════════════════════════════════════════════════
// Инвалидные индексы Synapse игнорируются
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("CPU: Invalid synapse indices ignored", "[simulation_host]") {
    init_biological_library();

    Segment seg = {};
    seg.id = 1;
    seg.V_m = 0.0f;
    seg.length = 100.0f;
    seg.radius = 10.0f;
    seg.ATP_level = 1.0f;

    Synapse syn = {};
    syn.pre_segment = 0;
    syn.post_segment = 99;
    syn.release_probability = 1.0f;
    syn.release_amount = 1.0f;

    uint64_t tick = 0;
    REQUIRE_NOTHROW(core_run_tick(&seg, 1, nullptr, 0, &syn, 1, MODE_BIOLOGICAL, &tick, 0.01f, STIM_NULL, nullptr, nullptr));
}

// ═══════════════════════════════════════════════════════════════════
// Диффузия extra_ions между сегментами
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("CPU: Extra ions diffusion between connected segments", "[simulation_host]") {
    init_biological_library();

    Segment segs[2] = {};
    segs[0].id = 1; segs[0].V_m = -65.0f;
    segs[0].length = 100.0f; segs[0].radius = 10.0f;
    segs[0].extra_ions[ION_K] = 20.0f;
    segs[0].ATP_level = 1.0f;

    segs[1].id = 2; segs[1].V_m = -65.0f;
    segs[1].length = 100.0f; segs[1].radius = 10.0f;
    segs[1].extra_ions[ION_K] = 4.0f;
    segs[1].ATP_level = 1.0f;

    IntraConnection conn = {0, 1, 1};

    uint64_t tick = 0;
    core_run_tick(segs, 2, &conn, 1, nullptr, 0, MODE_BIOLOGICAL, &tick, 0.01f, STIM_NULL, nullptr, nullptr);

    REQUIRE(segs[0].extra_ions[ION_K] < 20.0f);
    // Bath-релаксация (внеклетка = резервуар CSF) тянет обе концентрации к BATH_ION[K]=3.0.
    // Диффузия от 20→4 даёт слабый входящий поток в segs[1], поэтому K остаётся ВЫШЕ bath (3.0),
    // но ниже исходного 4.0 (bath доминирует при BATH_TAU_MS=1).
    REQUIRE(segs[1].extra_ions[ION_K] > 3.0f);
    REQUIRE(segs[1].extra_ions[ION_K] < 4.0f);
}

// ═══════════════════════════════════════════════════════════════════
// Диффузия intra_nt между сегментами
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("CPU: Intra-nt diffusion between connected segments", "[simulation_host]") {
    init_biological_library();

    Segment segs[2] = {};
    segs[0].id = 1; segs[0].V_m = -65.0f;
    segs[0].length = 100.0f; segs[0].radius = 10.0f;
    segs[0].intra_nt[NT_DOPAMINE] = 2.0f;
    segs[0].ATP_level = 1.0f;

    segs[1].id = 2; segs[1].V_m = -65.0f;
    segs[1].length = 100.0f; segs[1].radius = 10.0f;
    segs[1].intra_nt[NT_DOPAMINE] = 0.0f;
    segs[1].ATP_level = 1.0f;

    IntraConnection conn = {0, 1, 1};

    uint64_t tick = 0;
    core_run_tick(segs, 2, &conn, 1, nullptr, 0, MODE_BIOLOGICAL, &tick, 0.01f, STIM_NULL, nullptr, nullptr);

    REQUIRE(segs[0].intra_nt[NT_DOPAMINE] < 2.0f);
    REQUIRE(segs[1].intra_nt[NT_DOPAMINE] > 0.0f);
}

// ═══════════════════════════════════════════════════════════════════
// Диффузия neurotransmitter (volume transmission) между сегментами
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("CPU: Neurotransmitter diffusion (volume transmission)", "[simulation_host]") {
    init_biological_library();

    Segment segs[2] = {};
    segs[0].id = 1; segs[0].V_m = -65.0f;
    segs[0].length = 100.0f; segs[0].radius = 10.0f;
    segs[0].neurotransmitter[NT_GLUTAMATE] = 3.0f;
    segs[0].ATP_level = 1.0f;

    segs[1].id = 2; segs[1].V_m = -65.0f;
    segs[1].length = 100.0f; segs[1].radius = 10.0f;
    segs[1].neurotransmitter[NT_GLUTAMATE] = 1.0f;
    segs[1].ATP_level = 1.0f;

    IntraConnection conn = {0, 1, 1};

    uint64_t tick = 0;
    core_run_tick(segs, 2, &conn, 1, nullptr, 0, MODE_BIOLOGICAL, &tick, 0.01f, STIM_NULL, nullptr, nullptr);

    REQUIRE(segs[0].neurotransmitter[NT_GLUTAMATE] < 3.0f);
    REQUIRE(segs[1].neurotransmitter[NT_GLUTAMATE] > 1.0f);
}

// ═══════════════════════════════════════════════════════════════════
// Диффузия крупных молекул медленнее мелких (Стокс-Эйнштейн)
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("CPU: Large molecules diffuse slower than small ones", "[simulation_host]") {
    init_biological_library();

    // ATP (M=507 Да) vs Glycine (M=75 Да)
    Segment segs[2] = {};
    segs[0].id = 1; segs[0].V_m = -65.0f;
    segs[0].length = 100.0f; segs[0].radius = 10.0f;
    segs[0].intra_nt[NT_ATP] = 2.0f;
    segs[0].intra_nt[NT_GLYCINE] = 2.0f;
    segs[0].ATP_level = 1.0f;

    segs[1].id = 2; segs[1].V_m = -65.0f;
    segs[1].length = 100.0f; segs[1].radius = 10.0f;
    segs[1].intra_nt[NT_ATP] = 0.0f;
    segs[1].intra_nt[NT_GLYCINE] = 0.0f;
    segs[1].ATP_level = 1.0f;

    IntraConnection conn = {0, 1, 1};

    uint64_t tick = 0;
    core_run_tick(segs, 2, &conn, 1, nullptr, 0, MODE_BIOLOGICAL, &tick, 0.01f, STIM_NULL, nullptr, nullptr);

    // Глицин (лёгкий) диффундирует быстрее ATP (тяжёлый)
    float glycine_loss = 2.0f - segs[0].intra_nt[NT_GLYCINE];
    float atp_loss    = 2.0f - segs[0].intra_nt[NT_ATP];
    REQUIRE(glycine_loss > atp_loss);
}

// ═══════════════════════════════════════════════════════════════════
// Клиренс нейромедиаторов (tau_clearance)
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("CPU: Neurotransmitter clearance", "[simulation_host]") {
    init_biological_library();

    Segment segs[2] = {};
    segs[0].id = 1;
    segs[0].V_m = 0.0f;
    segs[0].length = 100.0f;
    segs[0].radius = 10.0f;
    segs[0].ATP_level = 1.0f;

    segs[1].id = 2;
    segs[1].V_m = -65.0f;
    segs[1].length = 100.0f;
    segs[1].radius = 10.0f;
    segs[1].ATP_level = 1.0f;

    Synapse syn = {};
    syn.id = 1;
    syn.pre_segment = 0;
    syn.post_segment = 1;
    syn.type = SYNAPSE_TYPE_CHEMICAL;
    syn.release_probability = 1.0f;
    syn.release_amount = 1.0f;
    syn.tau_clearance = 5.0f;
    syn.bouton_Ca = 0.1f;  // 100 µM — выше SYT_KD (10 µM) → Ca²⁺-зависимый выброс

    uint64_t tick = 0;
    core_run_tick(segs, 2, nullptr, 0, &syn, 1, MODE_BIOLOGICAL, &tick, 0.01f, STIM_NULL, nullptr, nullptr);
    float nt_peak = syn.cleft_neurotransmitter[NT_GLUTAMATE];
    REQUIRE(nt_peak > 0.0f);

    for (int i = 0; i < 100; i++) {
        segs[0].V_m = -65.0f;
        syn.bouton_Ca = 0.0001f;  // базовая (100 нМ) — нет Ca²⁺-зависимого выброса → NT клирится
        core_run_tick(segs, 2, nullptr, 0, &syn, 1, MODE_BIOLOGICAL, &tick, 0.01f, STIM_NULL, nullptr, nullptr);
    }

    REQUIRE(syn.cleft_neurotransmitter[NT_GLUTAMATE] < nt_peak * 0.9f);
}
