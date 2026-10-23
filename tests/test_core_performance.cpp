// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_core_performance.cpp — нагрузочные тесты Core: run_tick, BION I/O, плагины.
// G6 thresholds: каждый REQUIRE проверяет регресс с запасом 2-50× от baseline.
// Baseline машина: Intel UHD Graphics 630, Ubuntu 24.04, GCC 13.3.
// При превышении порога CI должен фейлиться.
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <vector>
#include "types.h"
#include "bion_loader.h"
#include "plugin_manager.h"

using Clock = std::chrono::steady_clock;
using Ms = std::chrono::milliseconds;
using Us = std::chrono::microseconds;

extern "C" {
    void core_run_tick(Segment*, uint32_t, IntraConnection*, uint32_t,
                       Synapse*, uint32_t, uint8_t, uint64_t*, float,
                       CurrentStimulus*, uint32_t*, VoltageClamp*, uint32_t*,
                       RampStimulus*, uint32_t*, IonInjection*, uint32_t*,
                       LigandInjection*, uint32_t*, MechanoStimulus* mechano, uint32_t* mechano_count);
}

static const char* TEST_BION = "../BioNeuronStudio/build/network.bion";

// ── BION I/O ────────────────────────────────────────────────────────────

TEST_CASE("Perf: bion_load_host full network < 1 sec", "[perf][bion]") {
    if (access(TEST_BION, R_OK) != 0) { WARN("no test network"); return; }

    auto t0 = Clock::now();
    BionData bd; memset(&bd, 0, sizeof(bd));
    int ret = bion_load_host(TEST_BION, &bd);
    auto t1 = Clock::now();
    auto ms = std::chrono::duration_cast<Ms>(t1 - t0).count();

    REQUIRE(ret == 0);
    INFO("Loaded " << bd.num_segments << " segs in " << ms << " ms");
    REQUIRE(ms < 1000);

    if (bd.segments) delete[] bd.segments;
    if (bd.synapses) delete[] bd.synapses;
    if (bd.connections) delete[] bd.connections;
    if (bd.neurons) delete[] bd.neurons;
    if (bd.nuclei) delete[] bd.nuclei;
}

TEST_CASE("Perf: bion_save full network < 1 sec", "[perf][bion]") {
    if (access(TEST_BION, R_OK) != 0) { WARN("no test network"); return; }

    BionData bd; memset(&bd, 0, sizeof(bd));
    REQUIRE(bion_load_host(TEST_BION, &bd) == 0);

    auto t0 = Clock::now();
    int ret = bion_save("/tmp/perf_core_save.bion", &bd);
    auto t1 = Clock::now();
    auto ms = std::chrono::duration_cast<Ms>(t1 - t0).count();

    REQUIRE(ret == 0);
    REQUIRE(ms < 1000);
    INFO("Saved " << bd.num_segments << " segs in " << ms << " ms");

    if (bd.segments) delete[] bd.segments;
    if (bd.synapses) delete[] bd.synapses;
    if (bd.connections) delete[] bd.connections;
    if (bd.neurons) delete[] bd.neurons;
    if (bd.nuclei) delete[] bd.nuclei;
    unlink("/tmp/perf_core_save.bion");
}

// ── run_tick ────────────────────────────────────────────────────────────

TEST_CASE("Perf: core_run_tick throughput (full network)", "[perf][tick]") {
    if (access(TEST_BION, R_OK) != 0) { WARN("no test network"); return; }

    BionData bd; memset(&bd, 0, sizeof(bd));
    REQUIRE(bion_load_host(TEST_BION, &bd) == 0);

    INFO(bd.num_segments << " segs, " << bd.num_synapses << " syns");

    uint64_t tick = 0;
    uint32_t stim_cnt = 0, vc_cnt = 0, rm_cnt = 0, io_cnt = 0, lg_cnt = 0;

    // Прогрев: 10 тиков
    for (int i = 0; i < 10; i++) {
        core_run_tick(bd.segments, bd.num_segments,
                      bd.connections, bd.num_connections,
                      bd.synapses, bd.num_synapses,
                      0, &tick, 0.1f,
                      nullptr, &stim_cnt, nullptr, &vc_cnt,
                      nullptr, &rm_cnt, nullptr, &io_cnt,
                      nullptr, &lg_cnt, nullptr, nullptr);
    }

    // Замер: 100 тиков
    auto t0 = Clock::now();
    for (int i = 0; i < 100; i++) {
        core_run_tick(bd.segments, bd.num_segments,
                      bd.connections, bd.num_connections,
                      bd.synapses, bd.num_synapses,
                      0, &tick, 0.1f,
                      nullptr, &stim_cnt, nullptr, &vc_cnt,
                      nullptr, &rm_cnt, nullptr, &io_cnt,
                      nullptr, &lg_cnt, nullptr, nullptr);
    }
    auto t1 = Clock::now();
    auto us = std::chrono::duration_cast<Us>(t1 - t0).count();
    float us_per_tick = us / 100.0f;

    INFO("Throughput: " << us_per_tick << " µs/tick (" << (1000000.0f / us_per_tick) << " ticks/s)");
    REQUIRE(us_per_tick < 5000.0f); // менее 5ms на тик

    if (bd.segments) delete[] bd.segments;
    if (bd.synapses) delete[] bd.synapses;
    if (bd.connections) delete[] bd.connections;
    if (bd.neurons) delete[] bd.neurons;
    if (bd.nuclei) delete[] bd.nuclei;
}

TEST_CASE("Perf: core_run_tick minimal segment", "[perf][tick]") {
    Segment seg;
    memset(&seg, 0, sizeof(seg));
    seg.segment_type = 1; seg.radius = 10.0f; seg.length = 20.0f;
    seg.V_m = -65.0f; seg.id = 1;
    seg.modifier_count = 0;

    uint64_t tick = 0;
    uint32_t cnt = 0;

    auto t0 = Clock::now();
    for (int i = 0; i < 10000; i++) {
        core_run_tick(&seg, 1, nullptr, 0, nullptr, 0, 0, &tick, 0.1f,
                      nullptr, &cnt, nullptr, &cnt,
                      nullptr, &cnt, nullptr, &cnt, nullptr, &cnt, nullptr, nullptr);
    }
    auto t1 = Clock::now();
    auto us = std::chrono::duration_cast<Us>(t1 - t0).count();
    float ns_per_tick = (us * 1000.0f) / 10000.0f;

    INFO("Minimal segment: " << ns_per_tick << " ns/tick");
    REQUIRE(ns_per_tick < 100000.0f); // менее 100µs на пустой сегмент
}

// ── Плагины ─────────────────────────────────────────────────────────────

TEST_CASE("Perf: plugin_load_directory time", "[perf][plugins]") {
    plugin_manager_init();

    auto t0 = Clock::now();
    int loaded = plugin_load_directory("plugins/builtin");
    auto t1 = Clock::now();
    auto ms = std::chrono::duration_cast<Ms>(t1 - t0).count();

    REQUIRE(loaded > 0);
    INFO("Loaded " << loaded << " plugins in " << ms << " ms");
    REQUIRE(ms < 5000); // менее 5 секунд
}

TEST_CASE("Perf: plugin_get rapid access x10000", "[perf][plugins]") {
    plugin_manager_init();
    REQUIRE(plugin_load_builtin(".") > 0);
    int count = plugin_get_count();
    REQUIRE(count > 0);

    auto t0 = Clock::now();
    for (int i = 0; i < 10000; i++) {
        plugin_get(i % count);
    }
    auto t1 = Clock::now();
    auto us = std::chrono::duration_cast<Us>(t1 - t0).count();
    float ns_per = (us * 1000.0f) / 10000.0f;

    INFO("plugin_get: " << ns_per << " ns/call");
    REQUIRE(ns_per < 10000.0f); // менее 10µs на вызов
}
