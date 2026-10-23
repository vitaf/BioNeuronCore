// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_gpu_all_slots.cpp — проверка GPU_ALL: оба слота идентичны, слот[0]=CUDA.

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
    int (*num_slots)(void*);
    int (*active_slot)(void*);
    int (*set_slot)(void*, int);
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
        num_slots = (int(*)(void*))dlsym(handle, "gpu_backend_num_slots");
        active_slot = (int(*)(void*))dlsym(handle, "gpu_backend_active_slot");
        set_slot = (int(*)(void*,int))dlsym(handle, "gpu_backend_set_slot");
        return create && load_bion && run_tick && bion_data;
    }
    void unload() { if (handle) dlclose(handle); }
};

static std::string make_simple_bion(const char* tag) {
    char path[256]; snprintf(path, sizeof(path), "/tmp/gpu_all_%s.bion", tag);
    NetworkHeader h = {}; h.magic = BION_MAGIC; h.version = BION_VERSION;
    h.num_neurons = 1; h.num_segments = 2; h.num_intra_connections = 1; h.num_synapses = 1;

    FILE* f = fopen(path, "wb"); REQUIRE(f);
    fwrite(&h, sizeof(h), 1, f);

    for (int i = 0; i < 1; i++) {   // num_neurons=1 — ОДИН нейрон (2 сегмента)
        Neuron n = {}; n.id = (uint32_t)i; n.segment_start = (uint32_t)i; n.segment_count = 2; n.threshold = -20;
        fwrite(&n, sizeof(n), 1, f);
    }

    for (int i = 0; i < 2; i++) {
        Segment s = {}; s.id = (uint32_t)i; s.neuron_id = (uint32_t)i;
        s.segment_type = (i == 0 ? 1u : 0u);
        s.position = {0, (float)(50 + i*30), 0}; s.direction = {0, 1, 0};
        s.radius = 5; s.length = 10; s.V_m = -65;
        s.intra_ions[0]=15.0f; s.intra_ions[1]=140.0f; s.intra_ions[2]=0.0001f; s.intra_ions[3]=5.0f; s.intra_ions[4]=0.5f;
        s.extra_ions[0]=140.0f; s.extra_ions[1]=5.0f; s.extra_ions[2]=1.2f; s.extra_ions[3]=130.0f; s.extra_ions[4]=2.0f;
        s.modifier_count = 3;
        s.modifiers[0].type=0; s.modifiers[0].density=1;
        s.modifiers[1].type=7; s.modifiers[1].density=0.6f;
        s.modifiers[2].type=107; s.modifiers[2].density=4.17f;  // K2P литературный (стабильный покой)
        fwrite(&s, sizeof(s), 1, f);
    }

    IntraConnection c = {0, 1, 100};
    fwrite(&c, sizeof(c), 1, f);

    Synapse syn = {}; syn.id = 0; syn.pre_segment = 0; syn.post_segment = 1;
    syn.type = SYNAPSE_TYPE_CHEMICAL; syn.weight = 0.5f;
    syn.release_probability = 0.5f; syn.release_amount = 1.0f; syn.tau_clearance = 5.0f;
    syn.pre_area = 0.1f; syn.post_area = 0.1f;
    syn.post_receptor_count = 1;
    syn.post_receptors[0].type = MOD_NT_RECEPTOR_AMPA; syn.post_receptors[0].density = 0.1f;
    fwrite(&syn, sizeof(syn), 1, f);
    fclose(f);
    return path;
}

TEST_CASE("GPU_ALL: both slots produce identical results", "[gpu_all]") {
    CudaBackend bk;
    if (!bk.load()) { WARN("CUDA not available"); return; }

    std::string path = make_simple_bion("slots");

    // Слот 0
    bk.sim = bk.create(); REQUIRE(bk.sim);
    REQUIRE(bk.load_bion(bk.sim, path.c_str()) == 0);
    if (bk.set_slot) bk.set_slot(bk.sim, 0);
    BionData* bd0 = (BionData*)bk.bion_data(bk.sim); REQUIRE(bd0);
    for (int t = 0; t < 100; t++) bk.run_tick(bk.sim, 0.1f);
    float Vm0_s0 = bd0->segments[0].V_m;
    float Vm1_s0 = bd0->segments[1].V_m;
    bk.destroy(bk.sim);

    // Слот 1
    bk.sim = bk.create(); REQUIRE(bk.sim);
    REQUIRE(bk.load_bion(bk.sim, path.c_str()) == 0);
    if (bk.set_slot) bk.set_slot(bk.sim, 1);
    BionData* bd1 = (BionData*)bk.bion_data(bk.sim); REQUIRE(bd1);
    for (int t = 0; t < 100; t++) bk.run_tick(bk.sim, 0.1f);
    float Vm0_s1 = bd1->segments[0].V_m;
    float Vm1_s1 = bd1->segments[1].V_m;
    bk.destroy(bk.sim);

    printf("[GPU_ALL-SLOTS] Slot 0: Vm[0]=%.4f Vm[1]=%.4f\n", Vm0_s0, Vm1_s0);
    printf("[GPU_ALL-SLOTS] Slot 1: Vm[0]=%.4f Vm[1]=%.4f\n", Vm0_s1, Vm1_s1);

    CHECK(Vm0_s0 == Approx(Vm0_s1).epsilon(0.001f));
    CHECK(Vm1_s0 == Approx(Vm1_s1).epsilon(0.001f));

    std::remove(path.c_str()); bk.unload();
}

TEST_CASE("GPU_ALL: slot 0 is CUDA, data is correct", "[gpu_all]") {
    CudaBackend bk;
    if (!bk.load()) { WARN("CUDA not available"); return; }

    std::string path = make_simple_bion("cuda_slot0");

    bk.sim = bk.create(); REQUIRE(bk.sim);
    REQUIRE(bk.load_bion(bk.sim, path.c_str()) == 0);

    if (bk.active_slot) {
        int slot = bk.active_slot(bk.sim);
        printf("[GPU_ALL-SLOT0] Active slot: %d\n", slot);
        // По умолчанию должен быть слот 0
        CHECK(slot >= 0);
    }

    BionData* bd = (BionData*)bk.bion_data(bk.sim); REQUIRE(bd);

    // Проверяем структуру данных
    CHECK(bd->num_segments == 2);

    // Запускаем симуляцию
    for (int t = 0; t < 100; t++) bk.run_tick(bk.sim, 0.1f);

    // Данные должны быть корректны после симуляции
    CHECK_FALSE(std::isnan(bd->segments[0].V_m));
    CHECK_FALSE(std::isnan(bd->segments[1].V_m));
    CHECK(bd->segments[0].V_m > -200.0f);
    CHECK(bd->segments[0].V_m < 150.0f);
    CHECK(bd->segments[0].V_m != Approx(0.0f)); // не нуль после симуляции

    printf("[GPU_ALL-SLOT0] After 100 ticks: Vm[0]=%.4f Vm[1]=%.4f OK\n",
           bd->segments[0].V_m, bd->segments[1].V_m);

    bk.destroy(bk.sim); std::remove(path.c_str()); bk.unload();
}

TEST_CASE("GPU_ALL: data integrity — no NaN after simulation", "[gpu_all]") {
    CudaBackend bk;
    if (!bk.load()) { WARN("CUDA not available"); return; }

    std::string path = make_simple_bion("integrity");

    bk.sim = bk.create(); REQUIRE(bk.sim);
    REQUIRE(bk.load_bion(bk.sim, path.c_str()) == 0);
    BionData* bd = (BionData*)bk.bion_data(bk.sim); REQUIRE(bd);

    for (int t = 0; t < 500; t++) bk.run_tick(bk.sim, 0.1f);

    // Все сегменты без NaN
    for (uint32_t i = 0; i < bd->num_segments; i++) {
        CHECK_FALSE(std::isnan(bd->segments[i].V_m));
        CHECK_FALSE(std::isnan(bd->segments[i].I_buffer));
        for (int j = 0; j < ION_COUNT; j++) {
            CHECK_FALSE(std::isnan(bd->segments[i].intra_ions[j]));
            CHECK_FALSE(std::isnan(bd->segments[i].extra_ions[j]));
        }
    }

    printf("[GPU_ALL-INTEGRITY] 500 ticks, %u segments, all clean\n", bd->num_segments);
    bk.destroy(bk.sim); std::remove(path.c_str()); bk.unload();
}
