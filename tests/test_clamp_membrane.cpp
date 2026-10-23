// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_clamp_membrane.cpp — проверка clamp-защиты: V_m, I_buffer, ионы, modifier_count.
// Предотвращает numerical runaway, NaN-распространение и порчу памяти.

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

static std::string make_extreme_bion(const char* tag, float I_buf_init = 0.0f) {
    char path[256]; snprintf(path, sizeof(path), "/tmp/clamp_%s.bion", tag);
    NetworkHeader h = {}; h.magic = BION_MAGIC; h.version = BION_VERSION;
    h.num_neurons = 1; h.num_segments = 1;
    h.num_intra_connections = 0; h.num_synapses = 0;

    FILE* f = fopen(path, "wb"); REQUIRE(f);
    fwrite(&h, sizeof(h), 1, f);

    Neuron n = {}; n.id = 0; n.segment_count = 1; n.threshold = -20;
    fwrite(&n, sizeof(n), 1, f);

    Segment s = {}; s.id = 0; s.neuron_id = 0; s.segment_type = 1;
    s.radius = 5; s.length = 10; s.V_m = -65;
    s.intra_ions[0]=0.015f; s.intra_ions[1]=0.140f; s.intra_ions[2]=0.0001f; s.intra_ions[3]=0.005f;
    s.extra_ions[0]=0.140f; s.extra_ions[1]=0.005f; s.extra_ions[2]=0.0012f; s.extra_ions[3]=0.130f;
    s.I_buffer = I_buf_init;
    s.modifier_count = 3;
    s.modifiers[0].type=0; s.modifiers[0].density=1;
    s.modifiers[1].type=7; s.modifiers[1].density=0.2f;
    s.modifiers[2].type=107; s.modifiers[2].density=0.03f;
    fwrite(&s, sizeof(s), 1, f);
    fclose(f);
    return path;
}

// ── test_clamp_membrane: I_buffer=10⁶nA → V_m≤+150mV, V_m≥-200mV ──

TEST_CASE("Clamp: extreme I_buffer does not produce V_m outside [-200,+150] mV", "[clamp]") {
    // Позитивный шок
    {
        std::string path = make_extreme_bion("pos", 1e6f);
        BionData d; memset(&d, 0, sizeof(d));
        REQUIRE(bion_load_host(path.c_str(), &d) == 0);
        uint64_t tc = 0;
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr, nullptr, nullptr);
        CHECK(d.segments[0].V_m <= 150.0f);
        CHECK(d.segments[0].V_m >= -200.0f);
        CHECK_FALSE(std::isnan(d.segments[0].V_m));
        printf("[CLAMP-POS] I_buf=1e6 nA → V_m=%.2f mV (clamped to +150 max)\n", d.segments[0].V_m);
        bion_unload_host(&d); std::remove(path.c_str());
    }
    // Негативный шок
    {
        std::string path = make_extreme_bion("neg", -1e6f);
        BionData d; memset(&d, 0, sizeof(d));
        REQUIRE(bion_load_host(path.c_str(), &d) == 0);
        uint64_t tc = 0;
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr, nullptr, nullptr);
        CHECK(d.segments[0].V_m <= 150.0f);
        CHECK(d.segments[0].V_m >= -200.0f);
        CHECK_FALSE(std::isnan(d.segments[0].V_m));
        printf("[CLAMP-NEG] I_buf=-1e6 nA → V_m=%.2f mV (clamped to -200 min)\n", d.segments[0].V_m);
        bion_unload_host(&d); std::remove(path.c_str());
    }
    // NaN I_buffer
    {
        std::string path = make_extreme_bion("nan", NAN);
        BionData d; memset(&d, 0, sizeof(d));
        REQUIRE(bion_load_host(path.c_str(), &d) == 0);
        uint64_t tc = 0;
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr, nullptr, nullptr);
        CHECK_FALSE(std::isnan(d.segments[0].V_m));
        printf("[CLAMP-NAN] NaN I_buf → V_m=%.2f mV (recovered)\n", d.segments[0].V_m);
        bion_unload_host(&d); std::remove(path.c_str());
    }
}

// ── test_clamp_per_tick: 100 сегментов × 1000 тиков → все V_m в [-200,+150] ──

TEST_CASE("Clamp: 100 segments x 1000 ticks — all V_m in [-200,+150]", "[clamp]") {
    char path[256]; snprintf(path, sizeof(path), "/tmp/clamp_many.bion");
    const int N = 100;
    NetworkHeader h = {}; h.magic = BION_MAGIC; h.version = BION_VERSION;
    h.num_neurons = N; h.num_segments = N;
    h.num_intra_connections = (uint32_t)(N-1); h.num_synapses = 0;

    FILE* f = fopen(path, "wb"); REQUIRE(f);
    fwrite(&h, sizeof(h), 1, f);

    for (int i = 0; i < N; i++) {
        Neuron n = {}; n.id = (uint32_t)i; n.segment_start = (uint32_t)i; n.segment_count = 1; n.threshold = -20;
        fwrite(&n, sizeof(n), 1, f);
    }

    // Сеть с осевыми связями (может генерировать большие токи)
    for (int i = 0; i < N; i++) {
        Segment s = {}; s.id = (uint32_t)i; s.neuron_id = (uint32_t)i; s.segment_type = (i == 0 ? 1u : 0u);
        s.radius = 5; s.length = 10; s.V_m = -65.0f + (i % 3) * 10.0f; // разный V_m для тока
        s.position = {0, (float)i*20, 0}; s.direction = {0, 1, 0};
        s.intra_ions[0]=0.015f; s.intra_ions[1]=0.140f; s.intra_ions[2]=0.0001f; s.intra_ions[3]=0.005f;
        s.extra_ions[0]=0.140f; s.extra_ions[1]=0.005f; s.extra_ions[2]=0.0012f; s.extra_ions[3]=0.130f;
        s.modifier_count = 3;
        s.modifiers[0].type=0; s.modifiers[0].density=1;
        s.modifiers[1].type=7; s.modifiers[1].density=0.2f;
        s.modifiers[2].type=107; s.modifiers[2].density=0.03f;
        fwrite(&s, sizeof(s), 1, f);
    }

    // Осевые связи с малым сопротивлением (большие токи)
    for (int i = 0; i < N-1; i++) {
        IntraConnection c = {(uint32_t)i, (uint32_t)(i+1), 1}; // R=0.5 — сильная связь
        fwrite(&c, sizeof(c), 1, f);
    }
    fclose(f);

    // Тест на CPU
    {
        BionData d; memset(&d, 0, sizeof(d));
        REQUIRE(bion_load_host(path, &d) == 0);

        uint64_t tc = 0;
        for (int t = 0; t < 1000; t++) {
            core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                          d.synapses, d.num_synapses, 0, &tc, 0.1f,
                          nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr, nullptr, nullptr);
        }

        int violations = 0;
        for (uint32_t i = 0; i < d.num_segments; i++) {
            if (std::isnan(d.segments[i].V_m)) violations++;
            if (d.segments[i].V_m < -200.0f) violations++;
            if (d.segments[i].V_m > 150.0f) violations++;
        }
        CHECK(violations == 0);
        printf("[CLAMP-MANY] CPU: %d segments, 1000 ticks, %d violations\n", N, violations);
        bion_unload_host(&d);
    }

    std::remove(path);
}

// ── test_modifier_count_guard: modifier_count=200 → нет порчи памяти ──

TEST_CASE("Clamp: modifier_count beyond MAX does not corrupt memory", "[clamp]") {
    char path[256]; snprintf(path, sizeof(path), "/tmp/clamp_modcnt.bion");
    NetworkHeader h = {}; h.magic = BION_MAGIC; h.version = BION_VERSION;
    h.num_neurons = 1; h.num_segments = 2; h.num_intra_connections = 1; h.num_synapses = 0;

    FILE* f = fopen(path, "wb"); REQUIRE(f);
    fwrite(&h, sizeof(h), 1, f);

    for (int i = 0; i < 2; i++) {
        Neuron n = {}; n.id = (uint32_t)i; n.segment_start = (uint32_t)i; n.segment_count = 1; n.threshold = -20;
        fwrite(&n, sizeof(n), 1, f);
    }

    // Первый сегмент — нормальный, второй — с завышенным modifier_count
    Segment s0 = {}; s0.id = 0; s0.neuron_id = 0; s0.segment_type = 1;
    s0.position = {0, 50, 0}; s0.direction = {0, 1, 0};
    s0.radius = 5; s0.length = 10; s0.V_m = -65;
    s0.intra_ions[0]=0.015f; s0.intra_ions[1]=0.140f; s0.intra_ions[2]=0.0001f; s0.intra_ions[3]=0.005f;
    s0.extra_ions[0]=0.140f; s0.extra_ions[1]=0.005f; s0.extra_ions[2]=0.0012f; s0.extra_ions[3]=0.130f;
    s0.modifier_count = 3;
    s0.modifiers[0].type=0; s0.modifiers[0].density=1;
    s0.modifiers[1].type=7; s0.modifiers[1].density=0.2f;
    s0.modifiers[2].type=107; s0.modifiers[2].density=0.03f;
    fwrite(&s0, sizeof(s0), 1, f);

    Segment s1 = s0; s1.id = 1; s1.neuron_id = 1; s1.segment_type = 0;
    s1.modifier_count = 200; // завышено — guard должен ограничить до MAX_MODIFIERS_PER_SEGMENT
    // Заполняем модификаторы за пределами массива мусором
    s1.position = {1, 2, 3}; s1.direction = {0, 1, 0};
    fwrite(&s1, sizeof(s1), 1, f);

    IntraConnection c = {0, 1, 100};
    fwrite(&c, sizeof(c), 1, f);
    fclose(f);

    BionData d; memset(&d, 0, sizeof(d));
    REQUIRE(bion_load_host(path, &d) == 0);

    uint64_t tc = 0;
    for (int t = 0; t < 100; t++) {
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr, nullptr, nullptr);
    }

    // После 100 тиков симуляция не должна упасть и не должно быть NaN
    CHECK_FALSE(std::isnan(d.segments[0].V_m));
    CHECK_FALSE(std::isnan(d.segments[1].V_m));
    CHECK_FALSE(std::isnan(d.segments[0].I_buffer));
    CHECK_FALSE(std::isnan(d.segments[1].I_buffer));
    printf("[CLAMP-MODCNT] modifier_count=200 guarded: Vm[0]=%.2f Vm[1]=%.2f OK\n",
           d.segments[0].V_m, d.segments[1].V_m);

    bion_unload_host(&d); std::remove(path);
}

// ── test_ion_clamp: intra/extra_ions ≥ 1e-12, без NaN ──

TEST_CASE("Clamp: ion concentrations stay ≥ 1e-12 and NaN-free", "[clamp]") {
    std::string path = make_extreme_bion("ion", 1000.0f); // Большой I_buffer — может дёрнуть ионы
    BionData d; memset(&d, 0, sizeof(d));
    REQUIRE(bion_load_host(path.c_str(), &d) == 0);

    uint64_t tc = 0;
    for (int t = 0; t < 500; t++) {
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr, nullptr, nullptr);
    }

    for (uint32_t i = 0; i < d.num_segments; i++) {
        for (int j = 0; j < ION_COUNT; j++) {
            CHECK_FALSE(std::isnan(d.segments[i].intra_ions[j]));
            CHECK_FALSE(std::isnan(d.segments[i].extra_ions[j]));
            CHECK(d.segments[i].intra_ions[j] >= 1e-12f);
            CHECK(d.segments[i].extra_ions[j] >= 1e-12f);
            CHECK(d.segments[i].intra_ions[j] <= 10.0f);
            CHECK(d.segments[i].extra_ions[j] <= 10.0f);
        }
    }
    printf("[CLAMP-ION] All ions in [1e-12, 10.0] after 500 ticks OK\n");
    bion_unload_host(&d); std::remove(path.c_str());
}

// ── Clamp на GPU (CUDA) ──

TEST_CASE("Clamp: GPU (CUDA) V_m stays in [-200,+150]", "[clamp][gpu]") {
    CudaBackend bk;
    if (!bk.load()) { WARN("CUDA not available"); return; }

    std::string path = make_extreme_bion("gpu", 1e6f);
    bk.sim = bk.create(); REQUIRE(bk.sim);
    REQUIRE(bk.load_bion(bk.sim, path.c_str()) == 0);
    BionData* bd = (BionData*)bk.bion_data(bk.sim); REQUIRE(bd);

    bk.run_tick(bk.sim, 0.1f);

    CHECK(bd->segments[0].V_m <= 150.0f);
    CHECK(bd->segments[0].V_m >= -200.0f);
    CHECK_FALSE(std::isnan(bd->segments[0].V_m));
    printf("[CLAMP-GPU] CUDA: I_buf=1e6 nA → V_m=%.2f mV\n", bd->segments[0].V_m);

    bk.destroy(bk.sim); std::remove(path.c_str()); bk.unload();
}
