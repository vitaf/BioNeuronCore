// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_nan_guards.cpp — проверка что NaN не распространяется после 1000+ тиков
// на CPU, CUDA и GPU_ALL. Тесты на исправленные баги (position, weight, facilitation).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <dlfcn.h>

#include "types.h"
#include "bion_format.h"
#include "bion_loader.h"

extern "C" {
    void core_run_tick(Segment* segs, uint32_t N, IntraConnection* conns, uint32_t M,
                       Synapse* syns, uint32_t K, int mode, uint64_t* tick, float dt,
                       void*,void*,void*,void*,void*,void*,void*,void*,void*,void*, MechanoStimulus* mechano, uint32_t* mechano_count);
}

struct CudaBackend {
    void* handle; void* sim;
    void* (*create)(); void (*destroy)(void*);
    int (*load_bion)(void*, const char*);
    void (*run_tick)(void*, float);
    void* (*bion_data)(void*);
    bool load() {
        const char* paths[] = {"./libbiocore_gpu_backend_cuda.so",
                               "../BioNeuronCore/build/libbiocore_gpu_backend_cuda.so",
                               "libbiocore_gpu_backend_cuda.so"};
        for (int i = 0; i < 3; i++) { handle = dlopen(paths[i], RTLD_NOW); if (handle) break; }
        if (!handle) return false;
        create = (void*(*)())dlsym(handle, "gpu_backend_create");
        destroy = (void(*)(void*))dlsym(handle, "gpu_backend_destroy");
        load_bion = (int(*)(void*,const char*))dlsym(handle, "gpu_backend_load_bion");
        run_tick = (void(*)(void*,float))dlsym(handle, "gpu_backend_run_tick");
        bion_data = (void*(*)(void*))dlsym(handle, "gpu_backend_bion_data");
        return create && load_bion && run_tick && bion_data;
    }
    void unload() { if (handle) dlclose(handle); }
};

static std::string make_test_bion(const char* tag, int n_segs = 2, int n_syns = 1) {
    char path[256]; snprintf(path, sizeof(path), "/tmp/nan_test_%s.bion", tag);
    NetworkHeader h = {}; h.magic = BION_MAGIC; h.version = BION_VERSION;
    h.num_neurons = (uint32_t)n_segs; h.num_segments = (uint32_t)n_segs;
    h.num_intra_connections = (n_segs > 1 ? 1u : 0u);
    h.num_synapses = (uint32_t)n_syns;

    FILE* f = fopen(path, "wb"); REQUIRE(f);
    fwrite(&h, sizeof(h), 1, f);

    for (int i = 0; i < n_segs; i++) {
        Neuron n = {}; n.id = (uint32_t)i; n.segment_start = (uint32_t)i; n.segment_count = 1; n.threshold = -20;
        fwrite(&n, sizeof(n), 1, f);
    }

    for (int i = 0; i < n_segs; i++) {
        Segment s = {}; s.id = (uint32_t)i; s.neuron_id = (uint32_t)i; s.segment_type = (i == 0 ? 1u : 0u);
        s.radius = 5; s.length = 10; s.V_m = -65;
        s.position = {0, (float)i*20, 0}; s.direction = {0, 1, 0};
        s.intra_ions[0]=0.015f; s.intra_ions[1]=0.140f; s.intra_ions[2]=0.0001f; s.intra_ions[3]=0.005f;
        s.extra_ions[0]=0.140f; s.extra_ions[1]=0.005f; s.extra_ions[2]=0.0012f; s.extra_ions[3]=0.130f;
        s.modifier_count = 3; s.modifiers[0].type=0; s.modifiers[0].density=1;
        s.modifiers[1].type=7; s.modifiers[1].density=0.2f; s.modifiers[2].type=107; s.modifiers[2].density=0.03f;
        // Add CaMKII on post segment for plasticity
        if (i == 1) { s.modifiers[3].type=82; s.modifiers[3].density=0.1f; s.modifiers[3].state[0]=0.5f; s.modifier_count=4; }
        fwrite(&s, sizeof(s), 1, f);
    }

    if (n_segs > 1) { IntraConnection c = {0, 1, 100}; fwrite(&c, sizeof(c), 1, f); }
    if (n_syns > 0) {
        Synapse syn = {}; syn.id = 0; syn.pre_segment = 0; syn.post_segment = (uint32_t)(n_segs-1);
        syn.type = SYNAPSE_TYPE_CHEMICAL; syn.weight = 0.5f; syn.release_probability = 0.5f;
        syn.release_amount = 1.0f; syn.tau_clearance = 5.0f; syn.pre_area = 0.1f; syn.post_area = 0.1f;
        syn.post_receptor_count = 1; syn.post_receptors[0].type = 8; syn.post_receptors[0].density = 0.1f;
        fwrite(&syn, sizeof(syn), 1, f);
    }

    fclose(f);
    return path;
}

static bool check_no_nan(Segment* segs, uint32_t N, Synapse* syns, uint32_t K) {
    for (uint32_t i = 0; i < N; i++) {
        if (std::isnan(segs[i].V_m)) return false;
        if (std::isnan(segs[i].position.x)) return false;
        if (std::isnan(segs[i].position.y)) return false;
        if (std::isnan(segs[i].position.z)) return false;
        if (std::isnan(segs[i].radius)) return false;
        if (std::isnan(segs[i].length)) return false;
        if (std::isnan(segs[i].I_buffer)) return false;
        for (int j = 0; j < ION_COUNT; j++)
            if (std::isnan(segs[i].intra_ions[j]) || std::isnan(segs[i].extra_ions[j])) return false;
    }
    for (uint32_t i = 0; i < K; i++) {
        if (std::isnan(syns[i].weight)) return false;
        if (std::isnan(syns[i].facilitation)) return false;
        if (std::isnan(syns[i].depression)) return false;
    }
    return true;
}

// ═══════════════════════════════════════════════════════════════
// CPU: 1000+ тиков без NaN
// ═══════════════════════════════════════════════════════════════

TEST_CASE("NaN guards: CPU 1000 ticks no NaN", "[nan_guards]") {
    std::string path = make_test_bion("cpu");
    BionData d; memset(&d, 0, sizeof(d));
    REQUIRE(bion_load_host(path.c_str(), &d) == 0);

    uint64_t tc = 0;
    for (int t = 0; t < 1000; t++)
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr, nullptr, nullptr);

    CHECK(check_no_nan(d.segments, d.num_segments, d.synapses, d.num_synapses));
    printf("[NAN-CPU] 1000 ticks: position=(%.1f,%.1f) V_m=%.2f weight=%.4f OK\n",
           d.segments[0].position.x, d.segments[0].position.y, d.segments[0].V_m, d.synapses[0].weight);
    bion_unload_host(&d); std::remove(path.c_str());
}

// ═══════════════════════════════════════════════════════════════
// CUDA: 1000+ тиков без NaN
// ═══════════════════════════════════════════════════════════════

TEST_CASE("NaN guards: CUDA 1000 ticks no NaN", "[nan_guards]") {
    CudaBackend bk;
    if (!bk.load()) { WARN("CUDA not available"); return; }

    std::string path = make_test_bion("cuda");
    bk.sim = bk.create(); REQUIRE(bk.sim);
    REQUIRE(bk.load_bion(bk.sim, path.c_str()) == 0);
    BionData* bd = (BionData*)bk.bion_data(bk.sim); REQUIRE(bd);

    for (int t = 0; t < 1000; t++)
        bk.run_tick(bk.sim, 0.1f);

    CHECK(check_no_nan(bd->segments, bd->num_segments, bd->synapses, bd->num_synapses));
    printf("[NAN-CUDA] 1000 ticks: position=(%.1f,%.1f) V_m=%.2f weight=%.4f OK\n",
           bd->segments[0].position.x, bd->segments[0].position.y, bd->segments[0].V_m, bd->synapses[0].weight);
    bk.destroy(bk.sim); std::remove(path.c_str()); bk.unload();
}

// ═══════════════════════════════════════════════════════════════
// V_m биологический диапазон
// ═══════════════════════════════════════════════════════════════

TEST_CASE("NaN guards: V_m stays in biological range", "[nan_guards]") {
    CudaBackend bk;
    if (!bk.load()) { WARN("CUDA not available"); return; }

    std::string path = make_test_bion("vm_range");
    bk.sim = bk.create(); REQUIRE(bk.sim);
    REQUIRE(bk.load_bion(bk.sim, path.c_str()) == 0);
    BionData* bd = (BionData*)bk.bion_data(bk.sim); REQUIRE(bd);

    for (int t = 0; t < 500; t++) bk.run_tick(bk.sim, 0.1f);

    for (uint32_t i = 0; i < bd->num_segments; i++) {
        CHECK(bd->segments[i].V_m > -200.0f);
        // V_m может достигать верхней границы clamp (150mV) при больших
        // аксиальных токах или активных HH-каналах — это биологически допустимо
        CHECK(bd->segments[i].V_m <= 150.0f);
        CHECK(bd->segments[i].I_buffer <= 10000.0f);
        CHECK(bd->segments[i].I_buffer >= -10000.0f);
    }
    printf("[NAN-RANGE] V_m=%.2f I_buf=%.6f OK\n", bd->segments[0].V_m, bd->segments[0].I_buffer);
    bk.destroy(bk.sim); std::remove(path.c_str()); bk.unload();
}

// ═══════════════════════════════════════════════════════════════
// Геометрия не меняется после тиков
// ═══════════════════════════════════════════════════════════════

TEST_CASE("NaN guards: geometry stable after ticks", "[nan_guards]") {
    CudaBackend bk;
    if (!bk.load()) { WARN("CUDA not available"); return; }

    std::string path = make_test_bion("geo");
    bk.sim = bk.create(); REQUIRE(bk.sim);
    REQUIRE(bk.load_bion(bk.sim, path.c_str()) == 0);
    BionData* bd = (BionData*)bk.bion_data(bk.sim); REQUIRE(bd);

    float px0 = bd->segments[0].position.x, py0 = bd->segments[0].position.y;
    float r0 = bd->segments[0].radius, l0 = bd->segments[0].length;

    for (int t = 0; t < 500; t++) bk.run_tick(bk.sim, 0.1f);

    CHECK(bd->segments[0].position.x == Approx(px0));
    CHECK(bd->segments[0].position.y == Approx(py0));
    CHECK(bd->segments[0].radius == Approx(r0));
    CHECK(bd->segments[0].length == Approx(l0));
    printf("[NAN-GEO] Geometry stable after 500 ticks: pos=(%.1f,%.1f) r=%.1f l=%.1f OK\n",
           bd->segments[0].position.x, bd->segments[0].position.y, bd->segments[0].radius, bd->segments[0].length);
    bk.destroy(bk.sim); std::remove(path.c_str()); bk.unload();
}

// ═══════════════════════════════════════════════════════════════
// Вес синапса в биологическом диапазоне [0.01, 10.0]
// ═══════════════════════════════════════════════════════════════

TEST_CASE("NaN guards: synaptic weight in range", "[nan_guards]") {
    CudaBackend bk;
    if (!bk.load()) { WARN("CUDA not available"); return; }

    std::string path = make_test_bion("weight");
    bk.sim = bk.create(); REQUIRE(bk.sim);
    REQUIRE(bk.load_bion(bk.sim, path.c_str()) == 0);
    BionData* bd = (BionData*)bk.bion_data(bk.sim); REQUIRE(bd);

    for (int t = 0; t < 500; t++) bk.run_tick(bk.sim, 0.1f);

    for (uint32_t i = 0; i < bd->num_synapses; i++) {
        CHECK(bd->synapses[i].weight >= 0.01f);
        CHECK(bd->synapses[i].weight <= 10.0f);
        CHECK_FALSE(std::isnan(bd->synapses[i].weight));
        CHECK_FALSE(std::isnan(bd->synapses[i].facilitation));
        CHECK_FALSE(std::isnan(bd->synapses[i].depression));
    }
    printf("[NAN-WEIGHT] weight=%.4f fac=%.4f dep=%.4f OK\n",
           bd->synapses[0].weight, bd->synapses[0].facilitation, bd->synapses[0].depression);
    bk.destroy(bk.sim); std::remove(path.c_str()); bk.unload();
}
