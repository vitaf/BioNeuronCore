// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// Тесты межсегментной диффузии (шаг 2.5 core_run_tick):
// ионы, ATP, температура, транскрипционный сигнал.
#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include "types.h"
#include <cstring>
#include <cmath>

extern "C" void core_run_tick(Segment* segments, uint32_t num_segments,
                   IntraConnection* connections, uint32_t num_connections,
                   Synapse* synapses, uint32_t num_synapses,
                   uint8_t mode, uint64_t* tick_count, float dt,
                   CurrentStimulus* stimuli, uint32_t* stimulus_count,
                   VoltageClamp* vclamps, uint32_t* vclamp_count,
                   RampStimulus* ramps, uint32_t* ramp_count,
                   IonInjection* ion_inj, uint32_t* ion_inj_count,
                   LigandInjection* lig_inj, uint32_t* lig_inj_count, MechanoStimulus* mechano, uint32_t* mechano_count);

static void make_seg_pair(Segment& a, Segment& b) {
    memset(&a, 0, sizeof(a)); memset(&b, 0, sizeof(b));
    a.id = 1; a.length = 100.0f; a.radius = 10.0f;
    a.V_m = -65.0f;
    a.intra_ions[ION_NA] = 50.0f; a.intra_ions[ION_K] = 140.0f;
    a.intra_ions[ION_CA] = 0.1f;  a.intra_ions[ION_CL] = 20.0f;
    a.intra_ions[ION_MG] = 1.0f;
    a.ATP_level = 5.0f; a.temperature = 37.0f; a.transcription_signal = 2.0f;
    a.modifier_count = 0; a.I_buffer = 0.0f;

    b.id = 2; b.length = 100.0f; b.radius = 10.0f;
    b.V_m = -65.0f;
    b.intra_ions[ION_NA] = 10.0f; b.intra_ions[ION_K] = 140.0f;
    b.intra_ions[ION_CA] = 0.0001f; b.intra_ions[ION_CL] = 5.0f;
    b.intra_ions[ION_MG] = 0.3f;
    b.ATP_level = 1.0f; b.temperature = 37.0f; b.transcription_signal = 0.1f;
    b.modifier_count = 0; b.I_buffer = 0.0f;
}

static void make_small_seg_pair(Segment& a, Segment& b) {
    memset(&a, 0, sizeof(a)); memset(&b, 0, sizeof(b));
    a.id = 1; a.length = 10.0f; a.radius = 1.0f;
    a.V_m = -65.0f;
    a.intra_ions[ION_NA] = 50.0f; a.intra_ions[ION_K] = 140.0f;
    a.intra_ions[ION_CA] = 0.1f;  a.intra_ions[ION_CL] = 20.0f;
    a.intra_ions[ION_MG] = 1.0f;
    a.ATP_level = 5.0f; a.temperature = 50.0f; a.transcription_signal = 2.0f;
    a.modifier_count = 0; a.I_buffer = 0.0f;

    b.id = 2; b.length = 10.0f; b.radius = 1.0f;
    b.V_m = -65.0f;
    b.intra_ions[ION_NA] = 10.0f; b.intra_ions[ION_K] = 140.0f;
    b.intra_ions[ION_CA] = 0.0001f; b.intra_ions[ION_CL] = 5.0f;
    b.intra_ions[ION_MG] = 0.3f;
    b.ATP_level = 1.0f; b.temperature = 30.0f; b.transcription_signal = 0.1f;
    b.modifier_count = 0; b.I_buffer = 0.0f;
}

TEST_CASE("Diffusion: Na+ flows high→low", "[diffusion]") {
    Segment segs[2]; make_seg_pair(segs[0], segs[1]);
    IntraConnection conn = {0, 1, 1};
    uint64_t tick = 0;

    float na0_before = segs[0].intra_ions[ION_NA];
    float na1_before = segs[1].intra_ions[ION_NA];

    for (int i = 0; i < 100; i++)
        core_run_tick(segs, 2, &conn, 1, nullptr, 0, 0, &tick, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

    CHECK(segs[0].intra_ions[ION_NA] < na0_before);
    CHECK(segs[1].intra_ions[ION_NA] > na1_before);
}

TEST_CASE("Diffusion: Ca2+ flows high→low", "[diffusion]") {
    Segment segs[2]; make_seg_pair(segs[0], segs[1]);
    IntraConnection conn = {0, 1, 1};
    uint64_t tick = 0;

    float ca0 = segs[0].intra_ions[ION_CA], ca1 = segs[1].intra_ions[ION_CA];
    for (int i = 0; i < 100; i++)
        core_run_tick(segs, 2, &conn, 1, nullptr, 0, 0, &tick, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    CHECK(segs[0].intra_ions[ION_CA] < ca0);
    CHECK(segs[1].intra_ions[ION_CA] > ca1);
}

TEST_CASE("Diffusion: Mg2+ flows high→low", "[diffusion]") {
    Segment segs[2]; make_seg_pair(segs[0], segs[1]);
    IntraConnection conn = {0, 1, 1};
    uint64_t tick = 0;

    float mg0 = segs[0].intra_ions[ION_MG], mg1 = segs[1].intra_ions[ION_MG];
    for (int i = 0; i < 100; i++)
        core_run_tick(segs, 2, &conn, 1, nullptr, 0, 0, &tick, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    CHECK(segs[0].intra_ions[ION_MG] < mg0);
    CHECK(segs[1].intra_ions[ION_MG] > mg1);
}

TEST_CASE("Diffusion: ATP flows soma→dendrite", "[diffusion]") {
    Segment segs[2]; make_seg_pair(segs[0], segs[1]);
    IntraConnection conn = {0, 1, 1};
    uint64_t tick = 0;

    float atp0 = segs[0].ATP_level, atp1 = segs[1].ATP_level;
    for (int i = 0; i < 100; i++)
        core_run_tick(segs, 2, &conn, 1, nullptr, 0, 0, &tick, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    CHECK(segs[0].ATP_level < atp0);
    CHECK(segs[1].ATP_level > atp1);
}

TEST_CASE("Diffusion: temperature equalises with small segments", "[diffusion]") {
    Segment segs[2]; make_small_seg_pair(segs[0], segs[1]);
    IntraConnection conn = {0, 1, 1};
    uint64_t tick = 0;

    float t0 = segs[0].temperature, t1 = segs[1].temperature;
    // Малые сегменты (r=1, L=10) + большой градиент (20°C) + много тиков
    for (int i = 0; i < 50000; i++)
        core_run_tick(segs, 2, &conn, 1, nullptr, 0, 0, &tick, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    CHECK(segs[0].temperature < t0);
    CHECK(segs[1].temperature > t1);
}

TEST_CASE("Diffusion: transcription_signal flows", "[diffusion]") {
    Segment segs[2]; make_seg_pair(segs[0], segs[1]);
    IntraConnection conn = {0, 1, 1};
    uint64_t tick = 0;

    float tx0 = segs[0].transcription_signal, tx1 = segs[1].transcription_signal;
    for (int i = 0; i < 100; i++)
        core_run_tick(segs, 2, &conn, 1, nullptr, 0, 0, &tick, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    CHECK(segs[0].transcription_signal < tx0);
    CHECK(segs[1].transcription_signal > tx1);
}

TEST_CASE("Diffusion: no connection → no flow", "[diffusion]") {
    Segment segs[2]; make_seg_pair(segs[0], segs[1]);
    uint64_t tick = 0;

    float na0 = segs[0].intra_ions[ION_NA], na1 = segs[1].intra_ions[ION_NA];
    for (int i = 0; i < 100; i++)
        core_run_tick(segs, 2, nullptr, 0, nullptr, 0, 0, &tick, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    CHECK(segs[0].intra_ions[ION_NA] == Approx(na0));
    CHECK(segs[1].intra_ions[ION_NA] == Approx(na1));
}

TEST_CASE("Diffusion: zero gradient → no change", "[diffusion]") {
    Segment segs[2];
    memset(&segs[0], 0, sizeof(segs[0])); memset(&segs[1], 0, sizeof(segs[1]));
    segs[0].id = 1; segs[0].length = 100.0f; segs[0].radius = 10.0f;
    segs[0].V_m = -65.0f; segs[0].intra_ions[ION_K] = 140.0f;
    segs[0].temperature = 37.0f;
    segs[1].id = 2; segs[1].length = 100.0f; segs[1].radius = 10.0f;
    segs[1].V_m = -65.0f; segs[1].intra_ions[ION_K] = 140.0f;
    segs[1].temperature = 37.0f;

    IntraConnection conn = {0, 1, 1};
    uint64_t tick = 0;

    float k0 = segs[0].intra_ions[ION_K];
    for (int i = 0; i < 100; i++)
        core_run_tick(segs, 2, &conn, 1, nullptr, 0, 0, &tick, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    CHECK(segs[0].intra_ions[ION_K] == Approx(k0));
}
