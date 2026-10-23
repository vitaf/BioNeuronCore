// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_din_parity.cpp — сравнивает CPU vs GPU для одиночного dIN
// c параметрами из debug-сети. Ищет источник гиперполяризации.

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
    void core_run_tick(Segment*, uint32_t, IntraConnection*, uint32_t,
                       Synapse*, uint32_t, int, uint64_t*, float,
                       void*,void*,void*,void*,void*,void*,void*,void*,void*,void*, MechanoStimulus* mechano, uint32_t* mechano_count);
}

struct CudaBackend {
    void *handle, *sim;
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
};

// Безопасное чтение V_m через raw offset
static float vm(Segment* s) { return s->V_m; }
static float ion(Segment* s, int idx) { return (float)s->intra_ions[idx]; }
static float extra(Segment* s, int idx) { return (float)s->extra_ions[idx]; }

static std::string make_din_bion(float Vm_init, bool with_glia) {
    const char* path = "/tmp/din_parity.bion";
    NetworkHeader h = {}; h.magic = BION_MAGIC; h.version = BION_VERSION;
    h.num_neurons = with_glia ? 2 : 1;
    h.num_segments = with_glia ? 2 : 1;

    Neuron n0 = {}; n0.segment_count = 1; n0.threshold = -20;

    // dIN из debug-сети: минимальный K+ leak, все 13 модификаторов
    Segment s = {}; s.id = 0; s.neuron_id = 0;
    s.segment_type = 1; s.radius = 5.0f; s.length = 10.0f; s.V_m = Vm_init;
    s.position = {0, 70, 0}; s.direction = {0, 1, 0};
    s.intra_ions[0]=15.0f; s.intra_ions[1]=140.0f; s.intra_ions[2]=0.0001f;
    s.intra_ions[3]=5.0f; s.intra_ions[4]=0.5f;
    s.extra_ions[0]=140.0f; s.extra_ions[1]=5.0f; s.extra_ions[2]=1.2f;
    s.extra_ions[3]=130.0f; s.extra_ions[4]=2.0f;

    // Точные параметры dIN из debug-сети
    s.modifier_count = 14;
    s.modifiers[0].type = MOD_AXON_NAV12;      s.modifiers[0].density = 18.97f;
    s.modifiers[1].type = MOD_KV21;            s.modifiers[1].density = 4.67f;
    s.modifiers[2].type = MOD_PUMP_NAK;        s.modifiers[2].density = 0.211f;
    s.modifiers[2].state[2] = 0.4f;  // pump_rate
    s.modifiers[3].type = MOD_K2P_LEAK;        s.modifiers[3].density = 0.03f;
    s.modifiers[4].type = MOD_KIR_CHANNEL;     s.modifiers[4].density = 0.008f;
    s.modifiers[5].type = MOD_NAP_CHANNEL;     s.modifiers[5].density = 0.0146f;
    s.modifiers[6].type = MOD_KCC2;            s.modifiers[6].density = 1.134f;
    s.modifiers[7].type = MOD_CAV13;        s.modifiers[7].density = 0.536f;
    s.modifiers[8].type = MOD_CA_LEAK;         s.modifiers[8].density = 0.434f;
    s.modifiers[9].type = MOD_BK_CHANNEL;      s.modifiers[9].density = 0.0916f;
    s.modifiers[10].type = MOD_CA_BUFFER;       s.modifiers[10].density = 0.764f;
    s.modifiers[11].type = MOD_PIR_TTYPE_CALCIUM; s.modifiers[11].density = 0.805f;
    s.modifiers[12].type = MOD_HCN2;   s.modifiers[12].density = 0.258f;
    s.modifiers[13].type = MOD_SK_CHANNEL;     s.modifiers[13].density = 0.337f;

    FILE* f = fopen(path, "wb");
    fwrite(&h, sizeof(h), 1, f);
    fwrite(&n0, sizeof(n0), 1, f);
    if (with_glia) {
        Neuron n1 = {}; n1.id = 1; n1.segment_start = 1; n1.segment_count = 1; n1.threshold = -20;
        fwrite(&n1, sizeof(n1), 1, f);
    }
    fwrite(&s, sizeof(s), 1, f);

    if (with_glia) {
        Segment g = {}; g.id = 1; g.neuron_id = 1;
        g.segment_type = 1; g.radius = 3.0f; g.length = 6.0f; g.V_m = -80.0f;
        g.position = {10, 50, 0}; g.direction = {0, 1, 0};
        g.intra_ions[0]=15.0f; g.intra_ions[1]=140.0f; g.intra_ions[2]=0.0001f;
        g.intra_ions[3]=5.0f; g.intra_ions[4]=0.5f;
        g.extra_ions[0]=140.0f; g.extra_ions[1]=5.0f; g.extra_ions[2]=1.2f;
        g.extra_ions[3]=130.0f; g.extra_ions[4]=2.0f;
        g.modifier_count = 2;
        g.modifiers[0].type = MOD_GLIAL_KIR41; g.modifiers[0].density = 0.4f;
        g.modifiers[1].type = MOD_GLIAL_NAK_PUMP; g.modifiers[1].density = 0.25f;
        fwrite(&g, sizeof(g), 1, f);

        IntraConnection c = {}; c.segment_a = 1; c.segment_b = 0;
        c.is_intracellular = 0; // EXTRA_ONLY
        fwrite(&c, sizeof(c), 1, f);
    }
    fclose(f);
    return path;
}

TEST_CASE("dIN parity: CPU vs GPU without glia", "[din_parity]") {
    CudaBackend bk;
    if (!bk.load()) { WARN("CUDA not available"); return; }

    std::string path = make_din_bion(-58.0f, false);

    // CPU
    float cpu_vm_100, cpu_ki, cpu_ko;
    {
        BionData d; memset(&d, 0, sizeof(d));
        REQUIRE(bion_load_host(path.c_str(), &d) == 0);
        uint64_t tc = 0;
        for (int t = 0; t < 100; t++)
            core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                          nullptr, 0, 0, &tc, 0.1f,
                          nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr, nullptr, nullptr);
        cpu_vm_100 = vm(&d.segments[0]);
        cpu_ki = ion(&d.segments[0], 1);
        cpu_ko = extra(&d.segments[0], 1);
        printf("[CPU] tick=100 Vm=%.2f K_i=%.4f K_o=%.4f\n", cpu_vm_100, cpu_ki, cpu_ko);
        bion_unload_host(&d);
    }

    // CUDA
    float cuda_vm_100, cuda_ki, cuda_ko;
    {
        bk.sim = bk.create();
        REQUIRE(bk.load_bion(bk.sim, path.c_str()) == 0);
        BionData* bd = (BionData*)bk.bion_data(bk.sim);
        for (int t = 0; t < 100; t++) bk.run_tick(bk.sim, 0.1f);
        cuda_vm_100 = vm(&bd->segments[0]);
        cuda_ki = ion(&bd->segments[0], 1);
        cuda_ko = extra(&bd->segments[0], 1);
        printf("[CUDA] tick=100 Vm=%.2f K_i=%.4f K_o=%.4f\n", cuda_vm_100, cuda_ki, cuda_ko);
        bk.destroy(bk.sim); bk.sim = nullptr;
    }

    float delta = fabsf(cpu_vm_100 - cuda_vm_100);
    printf("delta=%.2f mV\n", delta);
    CHECK(delta < 5.0f); // CPU и GPU должны совпадать в пределах 5mV

    // Проверяем что dIN НЕ улетает в -166mV (было бы багом)
    CHECK(cpu_vm_100 > -100.0f);
    CHECK(cuda_vm_100 > -100.0f);

    std::remove(path.c_str());
    if (bk.sim) bk.destroy(bk.sim);
    if (bk.handle) dlclose(bk.handle);
}

TEST_CASE("dIN parity: CPU vs GPU with glia", "[din_parity]") {
    CudaBackend bk;
    if (!bk.load()) { WARN("CUDA not available"); return; }

    std::string path = make_din_bion(-58.0f, true);

    // CPU
    float cpu_vm_100, cpu_ki, cpu_ko, glia_vm;
    {
        BionData d; memset(&d, 0, sizeof(d));
        REQUIRE(bion_load_host(path.c_str(), &d) == 0);
        uint64_t tc = 0;
        for (int t = 0; t < 100; t++)
            core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                          nullptr, 0, 0, &tc, 0.1f,
                          nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr, nullptr, nullptr);
        cpu_vm_100 = vm(&d.segments[0]);
        cpu_ki = ion(&d.segments[0], 1);
        cpu_ko = extra(&d.segments[0], 1);
        glia_vm = vm(&d.segments[1]);
        printf("[CPU+glia] tick=100 Vm=%.2f K_i=%.4f K_o=%.4f glia_Vm=%.2f\n",
               cpu_vm_100, cpu_ki, cpu_ko, glia_vm);
        bion_unload_host(&d);
    }

    // CUDA
    float cuda_vm_100, cuda_ki, cuda_ko, glia_vm_c;
    {
        bk.sim = bk.create();
        REQUIRE(bk.load_bion(bk.sim, path.c_str()) == 0);
        BionData* bd = (BionData*)bk.bion_data(bk.sim);
        for (int t = 0; t < 100; t++) bk.run_tick(bk.sim, 0.1f);
        cuda_vm_100 = vm(&bd->segments[0]);
        cuda_ki = ion(&bd->segments[0], 1);
        cuda_ko = extra(&bd->segments[0], 1);
        glia_vm_c = vm(&bd->segments[1]);
        printf("[CUDA+glia] tick=100 Vm=%.2f K_i=%.4f K_o=%.4f glia_Vm=%.2f\n",
               cuda_vm_100, cuda_ki, cuda_ko, glia_vm_c);
        bk.destroy(bk.sim); bk.sim = nullptr;
    }

    float delta = fabsf(cpu_vm_100 - cuda_vm_100);
    printf("delta=%.2f mV\n", delta);
    CHECK(delta < 5.0f);

    std::remove(path.c_str());
    if (bk.sim) bk.destroy(bk.sim); if (bk.handle) dlclose(bk.handle);
}

// Пошаговый трейсинг первых 10 тиков — видим ЧТО вызывает гиперполяризацию
TEST_CASE("dIN trace: first 10 ticks CPU vs GPU", "[din_trace]") {
    CudaBackend bk;
    if (!bk.load()) { WARN("CUDA not available"); return; }

    std::string path = make_din_bion(-58.0f, false);

    printf("\n=== dIN Vm trace (no glia, Vm_start=-58mV) ===\n");
    printf("%5s %8s %8s %8s\n", "tick", "CPU_Vm", "CUDA_Vm", "delta");

    BionData d; memset(&d, 0, sizeof(d));
    REQUIRE(bion_load_host(path.c_str(), &d) == 0);
    uint64_t tc = 0;

    bk.sim = bk.create();
    REQUIRE(bk.load_bion(bk.sim, path.c_str()) == 0);
    BionData* bd = (BionData*)bk.bion_data(bk.sim);

    for (int t = 0; t <= 10; t++) {
        float cpu_v = vm(&d.segments[0]);
        float cuda_v = vm(&bd->segments[0]);
        printf("%5d %8.2f %8.2f %8.2f\n", t, cpu_v, cuda_v, fabsf(cpu_v - cuda_v));

        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      nullptr, 0, 0, &tc, 0.1f,
                      nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr, nullptr, nullptr);
        bk.run_tick(bk.sim, 0.1f);
    }

    // Если CPU тоже падает — это модель, не GPU баг
    float cpu_final = vm(&d.segments[0]);
    if (cpu_final < -100.0f)
        printf("\n!!! CPU dIN also hyperpolarizes to %.1f — MODEL ISSUE, not GPU bug !!!\n", cpu_final);

    bion_unload_host(&d);
    if (bk.sim) { bk.destroy(bk.sim); bk.sim = nullptr; }
    std::remove(path.c_str());
    if (bk.handle) { dlclose(bk.handle); bk.handle = nullptr; }
}
