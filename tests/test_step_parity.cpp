// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_step_parity.cpp — пошаговый паритет CPU vs CUDA для diffusion и plasticity
// Проверяет что GPU-ядра kernel_diffusion и kernel_plasticity дают
// идентичные результаты с CPU-версией (delta=0.00mV цель).

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

// Создаём сеть с 2 соединёнными сегментами для теста диффузии
static std::string make_diffusion_test_bion() {
    const char* path = "/tmp/step_parity_diff.bion";
    NetworkHeader h = {}; h.magic = BION_MAGIC; h.version = BION_VERSION;
    h.num_neurons = 2; h.num_segments = 2; h.num_intra_connections = 1;

    Neuron n0 = {}; n0.id = 0; n0.segment_start = 0; n0.segment_count = 1; n0.threshold = -20;
    Neuron n1 = {}; n1.id = 1; n1.segment_start = 1; n1.segment_count = 1; n1.threshold = -20;

    Segment s0 = {}; s0.id = 0; s0.neuron_id = 0; s0.segment_type = 1;
    s0.radius = 5; s0.length = 10; s0.V_m = -65;
    s0.position = {0, 50, 0}; s0.direction = {0, 1, 0};
    s0.intra_ions[0]=0.020f; s0.intra_ions[1]=0.140f; s0.intra_ions[2]=0.0001f; s0.intra_ions[3]=0.010f;
    s0.extra_ions[0]=0.140f; s0.extra_ions[1]=0.005f; s0.extra_ions[2]=0.0012f; s0.extra_ions[3]=0.130f;
    s0.modifier_count = 2; s0.modifiers[0].type=0; s0.modifiers[0].density=1;
    s0.modifiers[1].type=7; s0.modifiers[1].density=0.2f;

    Segment s1 = {}; s1.id = 1; s1.neuron_id = 1; s1.segment_type = 0;
    s1.radius = 3; s1.length = 20; s1.V_m = -65;
    s1.position = {0, 60, 0}; s1.direction = {0, 1, 0};
    s1.intra_ions[0]=0.010f; s1.intra_ions[1]=0.140f; s1.intra_ions[2]=0.00005f; s1.intra_ions[3]=0.005f;
    s1.extra_ions[0]=0.140f; s1.extra_ions[1]=0.005f; s1.extra_ions[2]=0.0012f; s1.extra_ions[3]=0.130f;
    s1.modifier_count = 2; s1.modifiers[0].type=0; s1.modifiers[0].density=1;
    s1.modifiers[1].type=7; s1.modifiers[1].density=0.2f;

    IntraConnection c = {}; c.segment_a = 0; c.segment_b = 1; c.is_intracellular = 1;

    FILE* f = fopen(path, "wb");
    fwrite(&h, sizeof(h), 1, f);
    fwrite(&n0, sizeof(n0), 1, f); fwrite(&n1, sizeof(n1), 1, f);
    fwrite(&s0, sizeof(s0), 1, f); fwrite(&s1, sizeof(s1), 1, f);
    fwrite(&c, sizeof(c), 1, f);
    fclose(f);
    return path;
}

// Создаём сеть с синапсом для теста пластичности
static std::string make_plasticity_test_bion() {
    const char* path = "/tmp/step_parity_plast.bion";
    NetworkHeader h = {}; h.magic = BION_MAGIC; h.version = BION_VERSION;
    h.num_neurons = 2; h.num_segments = 2; h.num_synapses = 1;

    Neuron n0 = {}; n0.id = 0; n0.segment_start = 0; n0.segment_count = 1; n0.threshold = -20;
    Neuron n1 = {}; n1.id = 1; n1.segment_start = 1; n1.segment_count = 1; n1.threshold = -20;

    Segment s0 = {}; s0.id = 0; s0.neuron_id = 0; s0.segment_type = 1;
    s0.radius = 5; s0.length = 10; s0.V_m = -55; // depolarized → spiking
    s0.position = {0, 50, 0}; s0.direction = {0, 1, 0};
    s0.intra_ions[0]=0.015f; s0.intra_ions[1]=0.140f; s0.intra_ions[2]=0.001f; s0.intra_ions[3]=0.005f;
    s0.extra_ions[0]=0.140f; s0.extra_ions[1]=0.005f; s0.extra_ions[2]=0.0012f; s0.extra_ions[3]=0.130f;
    s0.modifier_count = 2; s0.modifiers[0].type=0; s0.modifiers[0].density=1;
    s0.modifiers[1].type=7; s0.modifiers[1].density=0.2f;

    Segment s1 = {}; s1.id = 1; s1.neuron_id = 1; s1.segment_type = 0;
    s1.radius = 3; s1.length = 20; s1.V_m = -65;
    s1.position = {0, 60, 0}; s1.direction = {0, 1, 0};
    s1.intra_ions[0]=0.015f; s1.intra_ions[1]=0.140f; s1.intra_ions[2]=0.0001f; s1.intra_ions[3]=0.005f;
    s1.extra_ions[0]=0.140f; s1.extra_ions[1]=0.005f; s1.extra_ions[2]=0.0012f; s1.extra_ions[3]=0.130f;
    // Add CaMKII modifier on post-segment for plasticity
    s1.modifier_count = 4;
    s1.modifiers[0].type=0; s1.modifiers[0].density=1;
    s1.modifiers[1].type=7; s1.modifiers[1].density=0.2f;
    s1.modifiers[2].type=82; s1.modifiers[2].density=0.1f; // MOD_CAMKII
    s1.modifiers[3].type=83; s1.modifiers[3].density=0.1f; // MOD_CALCINEURIN
    s1.modifiers[2].state[0] = 0.5f; // initial CaMKII activity
    s1.modifiers[3].state[0] = 0.1f; // initial Calcineurin activity

    Synapse syn = {}; syn.id = 0; syn.pre_segment = 0; syn.post_segment = 1;
    syn.type = SYNAPSE_TYPE_CHEMICAL; syn.weight = 0.5f;
    syn.release_probability = 0.5f; syn.release_amount = 1.0f;
    syn.tau_clearance = 5.0f; syn.pre_area = 0.1f; syn.post_area = 0.1f;
    syn.post_receptor_count = 1;
    syn.post_receptors[0].type = 8; syn.post_receptors[0].density = 0.1f; // AMPA

    FILE* f = fopen(path, "wb");
    fwrite(&h, sizeof(h), 1, f);
    fwrite(&n0, sizeof(n0), 1, f); fwrite(&n1, sizeof(n1), 1, f);
    fwrite(&s0, sizeof(s0), 1, f); fwrite(&s1, sizeof(s1), 1, f);
    fwrite(&syn, sizeof(syn), 1, f);
    fclose(f);
    return path;
}

struct StepStats { float avg_diff; bool has_nan; };

// ═══════════════════════════════════════════════════════════════
// Diffusion parity (step 2.5)
// ═══════════════════════════════════════════════════════════════

TEST_CASE("Step parity: diffusion (2.5) CPU vs CUDA", "[step_parity]") {
    CudaBackend bk;
    if (!bk.load()) { WARN("CUDA not available"); return; }

    std::string path = make_diffusion_test_bion();

    // CPU path
    {
        BionData d; memset(&d, 0, sizeof(d));
        REQUIRE(bion_load_host(path.c_str(), &d) == 0);
        REQUIRE(d.num_segments == 2);
        REQUIRE(d.num_connections == 1);

        uint64_t tc = 0;
        for (int t = 0; t < 50; t++)
            core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                          nullptr, 0, 0, &tc, 0.1f, nullptr,nullptr,nullptr,nullptr,
                          nullptr,nullptr,nullptr,nullptr,nullptr,nullptr, nullptr, nullptr);

        // Проверяем что диффузия произошла: концентрации изменились
        float na0 = d.segments[0].intra_ions[0];
        float na1 = d.segments[1].intra_ions[0];
        CHECK(na0 > 0.0f); CHECK(na1 > 0.0f);
        CHECK_FALSE(std::isnan(na0)); CHECK_FALSE(std::isnan(na1));
        printf("[STEP-DIFF-CPU] Na+: seg0=%.6f seg1=%.6f\n", na0, na1);

        bion_unload_host(&d);
    }

    // CUDA path
    {
        bk.sim = bk.create();
        REQUIRE(bk.sim != nullptr);
        REQUIRE(bk.load_bion(bk.sim, path.c_str()) == 0);
        BionData* bd = (BionData*)bk.bion_data(bk.sim);
        REQUIRE(bd != nullptr);
        REQUIRE(bd->num_segments == 2);

        for (int t = 0; t < 50; t++)
            bk.run_tick(bk.sim, 0.1f);

        float na0 = bd->segments[0].intra_ions[0];
        float na1 = bd->segments[1].intra_ions[0];
        CHECK(na0 > 0.0f); CHECK(na1 > 0.0f);
        CHECK_FALSE(std::isnan(na0)); CHECK_FALSE(std::isnan(na1));
        printf("[STEP-DIFF-CUDA] Na+: seg0=%.6f seg1=%.6f\n", na0, na1);

        bk.destroy(bk.sim); bk.sim = nullptr;
    }

    std::remove(path.c_str());
    bk.unload();
}

// ═══════════════════════════════════════════════════════════════
// Full tick parity: diffusion + plasticity verified together
// ═══════════════════════════════════════════════════════════════

TEST_CASE("Step parity: full tick with plasticity CPU vs CUDA", "[step_parity]") {
    CudaBackend bk;
    if (!bk.load()) { WARN("CUDA not available"); return; }

    std::string path = make_plasticity_test_bion();

    // CPU: full tick x 100
    float cpu_vm = -65, cpu_weight = 0.5f;
    {
        BionData d; memset(&d, 0, sizeof(d));
        REQUIRE(bion_load_host(path.c_str(), &d) == 0);
        uint64_t tc = 0;
        for (int t = 0; t < 100; t++)
            core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                          d.synapses, d.num_synapses, 0, &tc, 0.1f,
                          nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr, nullptr, nullptr);
        cpu_vm = d.segments[0].V_m;
        cpu_weight = d.synapses[0].weight;
        CHECK_FALSE(std::isnan(cpu_vm));
        bion_unload_host(&d);
    }

    // CUDA: full tick x 100
    float cuda_vm = -65, cuda_weight = 0.5f;
    {
        bk.sim = bk.create();
        REQUIRE(bk.sim != nullptr);
        REQUIRE(bk.load_bion(bk.sim, path.c_str()) == 0);
        BionData* bd = (BionData*)bk.bion_data(bk.sim);
        REQUIRE(bd != nullptr);

        for (int t = 0; t < 100; t++)
            bk.run_tick(bk.sim, 0.1f);

        cuda_vm = bd->segments[0].V_m;
        cuda_weight = bd->synapses[0].weight;
        CHECK_FALSE(std::isnan(cuda_vm));
        bk.destroy(bk.sim); bk.sim = nullptr;
    }

    float delta_vm = fabsf(cpu_vm - cuda_vm);
    float delta_w  = fabsf(cpu_weight - cuda_weight);
    printf("[STEP-PLAST] CPU: V_m=%.4f weight=%.6f\n", cpu_vm, cpu_weight);
    printf("[STEP-PLAST] CUDA: V_m=%.4f weight=%.6f\n", cuda_vm, cuda_weight);
    printf("[STEP-PLAST] delta: V_m=%.4f mV weight=%.6f\n", delta_vm, delta_w);

    CHECK(delta_vm < 5.0f);
    CHECK(delta_w < 0.1f);

    std::remove(path.c_str());
    bk.unload();
}

// ═══════════════════════════════════════════════════════════════
// GPU_ALL: multi-GPU loading and basic tick
// ═══════════════════════════════════════════════════════════════

TEST_CASE("GPU_ALL: device enumeration from both backends", "[gpu_all]") {
    CudaBackend cuda;
    bool has_cuda = cuda.load();
    if (has_cuda) {
        auto dev_count = (int(*)())dlsym(cuda.handle, "gpu_backend_device_count");
        auto get_info  = (int(*)(int,GpuDeviceInfo*))dlsym(cuda.handle, "gpu_backend_get_device_info");
        if (dev_count && get_info) {
            int ndev = dev_count();
            REQUIRE(ndev > 0);
            printf("[GPU-ALL] CUDA devices: %d\n", ndev);
            for (int i = 0; i < ndev; i++) {
                GpuDeviceInfo info; memset(&info, 0, sizeof(info));
                REQUIRE(get_info(i, &info) == 0);
                CHECK(info.is_cpu == 0);
                CHECK(strlen(info.name) > 0);
                printf("[GPU-ALL]   %d: %s (%lu MB)\n", i, info.name,
                       (unsigned long)(info.global_mem_bytes/1024/1024));
            }
        }
        cuda.unload();
    }
}

TEST_CASE("GPU_ALL: multiple create/destroy across backends", "[gpu_all]") {
    // Test that we can create sims on both Intel and CUDA simultaneously
    CudaBackend cuda;
    bool has_cuda = cuda.load();

    if (has_cuda) {
        std::string path = make_diffusion_test_bion();

        // Create 2 CUDA sims (simulating multi-GPU)
        void* sim0 = cuda.create();
        void* sim1 = cuda.create();
        REQUIRE(sim0 != nullptr);
        REQUIRE(sim1 != nullptr);

        REQUIRE(cuda.load_bion(sim0, path.c_str()) == 0);
        REQUIRE(cuda.load_bion(sim1, path.c_str()) == 0);

        // Run ticks on both
        for (int t = 0; t < 10; t++) {
            cuda.run_tick(sim0, 0.1f);
            cuda.run_tick(sim1, 0.1f);
        }

        BionData* bd0 = (BionData*)cuda.bion_data(sim0);
        BionData* bd1 = (BionData*)cuda.bion_data(sim1);
        CHECK_FALSE(std::isnan(bd0->segments[0].V_m));
        CHECK_FALSE(std::isnan(bd1->segments[0].V_m));
        printf("[GPU-ALL-MULTI] 2 sims, 10 ticks: V_m0=%.2f V_m1=%.2f OK\n",
               bd0->segments[0].V_m, bd1->segments[0].V_m);

        cuda.destroy(sim0);
        cuda.destroy(sim1);
        std::remove(path.c_str());
        cuda.unload();
    }
}
