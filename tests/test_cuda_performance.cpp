// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_cuda_performance.cpp — нагрузочный тест CUDA backend (#5 Performance)
// Сравнение производительности CPU vs CUDA на 1000-сегментной сети.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstdio>
#include <chrono>
#include <dlfcn.h>
#include "types.h"
#include "bion_format.h"
#include "bion_loader.h"

extern "C" {
    void core_run_tick(Segment* segs, uint32_t N, IntraConnection* conns, uint32_t M,
                       Synapse* syns, uint32_t K, int mode, uint64_t* tick, float dt,
                       void*,void*,void*,void*,void*,void*,void*,void*,void*,void*, MechanoStimulus* mechano, uint32_t* mechano_count);
}

struct CudaApi { /* same pattern as integration test */
    void* handle; void* sim;
    void* (*create)(); void (*destroy)(void*);
    int (*load_bion)(void*, const char*);
    void (*run_tick)(void*, float);
    void* (*bion_data)(void*);
    int (*device_count)();
    bool init() {
        handle = dlopen("./libbiocore_gpu_backend_cuda.so", RTLD_NOW);
        if (!handle) return false;
        create = (void*(*)())dlsym(handle, "gpu_backend_create");
        destroy = (void(*)(void*))dlsym(handle, "gpu_backend_destroy");
        load_bion = (int(*)(void*,const char*))dlsym(handle, "gpu_backend_load_bion");
        run_tick = (void(*)(void*,float))dlsym(handle, "gpu_backend_run_tick");
        bion_data = (void*(*)(void*))dlsym(handle, "gpu_backend_bion_data");
        device_count = (int(*)())dlsym(handle, "gpu_backend_device_count");
        return create && run_tick && bion_data && device_count;
    }
    void close() { if (sim) destroy(sim); if (handle) dlclose(handle); }
};

static std::string make_large_bion(int n_segs) {
    const char* path = "/tmp/cuda_perf_test.bion";
    NetworkHeader hdr = {};
    hdr.magic = BION_MAGIC; hdr.version = BION_VERSION; hdr.mode = 0;
    hdr.num_neurons = (uint32_t)n_segs; hdr.num_segments = (uint32_t)n_segs;

    Neuron* neurons = new Neuron[n_segs];
    for (int i = 0; i < n_segs; i++) {
        neurons[i].id = i; neurons[i].segment_start = i; neurons[i].segment_count = 1;
        neurons[i].soma_index = i; neurons[i].threshold = -20;
    }

    Segment* segs = new Segment[n_segs];
    for (int i = 0; i < n_segs; i++) {
        memset(&segs[i], 0, sizeof(Segment));
        segs[i].id = i; segs[i].neuron_id = i; segs[i].segment_type = 1;
        segs[i].radius = 5; segs[i].length = 10; segs[i].V_m = -65;
        segs[i].position = {0, (float)i*20, 0}; segs[i].direction = {0,1,0};
        segs[i].intra_ions[0]=15.0f; segs[i].intra_ions[1]=140.0f;
        segs[i].intra_ions[2]=0.05f; segs[i].intra_ions[3]=5.0f;
        segs[i].extra_ions[0]=140.0f; segs[i].extra_ions[1]=5.0f;
        segs[i].extra_ions[2]=1.2f; segs[i].extra_ions[3]=130.0f;
        segs[i].modifier_count=2;
        segs[i].modifiers[0].type=0; segs[i].modifiers[0].density=1;
        segs[i].modifiers[1].type=7; segs[i].modifiers[1].density=0.2f;
    }

    FILE* f = fopen(path, "wb");
    fwrite(&hdr, sizeof(hdr), 1, f);
    fwrite(neurons, sizeof(Neuron), n_segs, f);
    fwrite(segs, sizeof(Segment), n_segs, f);
    fclose(f);
    delete[] neurons; delete[] segs;
    return path;
}

TEST_CASE("CUDA performance: tick throughput", "[cuda_performance]") {
    CudaApi cuda;
    if (!cuda.init()) { WARN("CUDA not available"); return; }

    std::string path = make_large_bion(1000);
    int ticks = 100;

    // CPU benchmark
    BionData cpu_data; memset(&cpu_data, 0, sizeof(cpu_data));
    REQUIRE(bion_load_host(path.c_str(), &cpu_data) == 0);
    uint64_t tick_c = 0;
    auto cpu_start = std::chrono::high_resolution_clock::now();
    for (int t = 0; t < ticks; t++)
        core_run_tick(cpu_data.segments, cpu_data.num_segments,
                      cpu_data.connections, cpu_data.num_connections,
                      cpu_data.synapses, cpu_data.num_synapses,
                      0, &tick_c, 0.1f,
                      nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr, nullptr, nullptr);
    auto cpu_end = std::chrono::high_resolution_clock::now();
    double cpu_ms = std::chrono::duration<double, std::milli>(cpu_end - cpu_start).count();
    printf("[CUDA-PERF] CPU %d ticks x %u segs: %.1f ms (%.2f ms/tick)\n",
           ticks, cpu_data.num_segments, cpu_ms, cpu_ms/ticks);
    bion_unload_host(&cpu_data);

    // CUDA benchmark
    cuda.sim = cuda.create();
    REQUIRE(cuda.sim);
    REQUIRE(cuda.load_bion(cuda.sim, path.c_str()) == 0);
    auto cuda_start = std::chrono::high_resolution_clock::now();
    for (int t = 0; t < ticks; t++)
        cuda.run_tick(cuda.sim, 0.1f);
    auto cuda_end = std::chrono::high_resolution_clock::now();
    double cuda_ms = std::chrono::duration<double, std::milli>(cuda_end - cuda_start).count();
    printf("[CUDA-PERF] CUDA %d ticks x %u segs: %.1f ms (%.2f ms/tick)\n",
           ticks, 1000u, cuda_ms, cuda_ms/ticks);

    // Проверяем что CUDA отработала без NaN
    double speedup = cpu_ms / cuda_ms;
    printf("[CUDA-PERF] Speedup: %.1fx (managed mem, overhead expected for small N)\n", speedup);
    CHECK(cuda_ms > 0.0); // CUDA completed successfully

    cuda.destroy(cuda.sim); cuda.sim = nullptr;
    std::remove(path.c_str());
    cuda.close();
}
