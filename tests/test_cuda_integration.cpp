// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_cuda_integration.cpp — интеграционный тест CUDA backend (#2 Integration)
// Полный жизненный цикл: dlopen→create→load_bion→run_tick (100 тиков)→destroy→dlclose

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <dlfcn.h>
#include "types.h"
#include "bion_format.h"
#include "bion_loader.h"

struct CudaApi {
    void* handle; void* sim;
    void* (*create)(); void (*destroy)(void*);
    int (*load_bion)(void*, const char*);
    void (*run_tick)(void*, float);
    void* (*bion_data)(void*);
    int (*device_count)();
    int (*get_device_info)(int, GpuDeviceInfo*);

    bool init() {
        handle = dlopen("./libbiocore_gpu_backend_cuda.so", RTLD_NOW);
        if (!handle) return false;
        create = (void*(*)())dlsym(handle, "gpu_backend_create");
        destroy = (void(*)(void*))dlsym(handle, "gpu_backend_destroy");
        load_bion = (int(*)(void*,const char*))dlsym(handle, "gpu_backend_load_bion");
        run_tick = (void(*)(void*,float))dlsym(handle, "gpu_backend_run_tick");
        bion_data = (void*(*)(void*))dlsym(handle, "gpu_backend_bion_data");
        device_count = (int(*)())dlsym(handle, "gpu_backend_device_count");
        get_device_info = (int(*)(int,GpuDeviceInfo*))dlsym(handle, "gpu_backend_get_device_info");
        return create && destroy && load_bion && run_tick && bion_data && device_count;
    }
    void close() { if (sim) destroy(sim); if (handle) dlclose(handle); }
};

static std::string make_test_bion() {
    const char* path = "/tmp/cuda_integration_test.bion";
    NetworkHeader hdr = {}; hdr.magic = BION_MAGIC; hdr.version = BION_VERSION;
    hdr.num_neurons = 1; hdr.num_segments = 1;
    Neuron n = {}; n.segment_count = 1; n.threshold = -20;
    Segment s = {};
    s.id = 0; s.radius = 5; s.length = 10; s.segment_type = 1;
    s.V_m = -65; s.position = {0,50,0}; s.direction = {0,1,0};
    s.intra_ions[0]=0.015f; s.intra_ions[1]=0.140f; s.intra_ions[2]=0.00005f; s.intra_ions[3]=0.005f;
    s.extra_ions[0]=0.140f; s.extra_ions[1]=0.005f; s.extra_ions[2]=0.0012f; s.extra_ions[3]=0.130f;
    s.modifier_count=3;
    s.modifiers[0]={0,{},1,{},{},0,0,{}}; s.modifiers[0].type=0; s.modifiers[0].density=1;
    s.modifiers[1]={7,{},0.2f,{},{},0,0,{}}; s.modifiers[1].type=7; s.modifiers[1].density=0.2f;
    s.modifiers[2]={107,{},0.03f,{},{},0,0,{}}; s.modifiers[2].type=107; s.modifiers[2].density=0.03f;

    FILE* f = fopen(path, "wb");
    fwrite(&hdr, sizeof(hdr), 1, f);
    fwrite(&n, sizeof(n), 1, f);
    fwrite(&s, sizeof(s), 1, f);
    fclose(f);
    return path;
}

TEST_CASE("CUDA integration: full lifecycle", "[cuda_integration]") {
    CudaApi cuda;
    REQUIRE(cuda.init());
    REQUIRE(cuda.device_count() > 0);

    SECTION("create/destroy without network") {
        void* sim = cuda.create();
        REQUIRE(sim != nullptr);
        cuda.destroy(sim);
    }

    SECTION("load network and run ticks") {
        std::string path = make_test_bion();
        void* sim = cuda.create();
        REQUIRE(sim != nullptr);

        REQUIRE(cuda.load_bion(sim, path.c_str()) == 0);

        BionData* bd = (BionData*)cuda.bion_data(sim);
        REQUIRE(bd != nullptr);
        REQUIRE(bd->num_segments == 1);
        REQUIRE(bd->segments[0].V_m == Approx(-65.0f));

        // Run 100 ticks
        for (int t = 0; t < 100; t++) {
            cuda.run_tick(sim, 0.1f);
            REQUIRE_FALSE(std::isnan(bd->segments[0].V_m));
        }

        // V_m should stay in biological range
        float v = bd->segments[0].V_m;
        REQUIRE(v > -200.0f);
        REQUIRE(v < 100.0f);

        printf("[CUDA-INTEGRATION] 100 ticks OK, V_m=%.2f mV\n", v);
        cuda.destroy(sim);
        std::remove(path.c_str());
    }

    SECTION("repeated load/unload cycles") {
        std::string path = make_test_bion();
        for (int cycle = 0; cycle < 5; cycle++) {
            void* sim = cuda.create();
            REQUIRE(sim != nullptr);
            REQUIRE(cuda.load_bion(sim, path.c_str()) == 0);
            cuda.run_tick(sim, 0.1f);
            cuda.destroy(sim);
        }
        printf("[CUDA-INTEGRATION] 5 load/unload cycles OK\n");
        std::remove(path.c_str());
    }

    cuda.close();
}

TEST_CASE("CUDA integration: device enumeration", "[cuda_integration]") {
    CudaApi cuda;
    REQUIRE(cuda.init());

    int ndev = cuda.device_count();
    REQUIRE(ndev > 0);

    for (int i = 0; i < ndev; i++) {
        GpuDeviceInfo info;
        REQUIRE(cuda.get_device_info(i, &info) == 0);
        REQUIRE(info.is_cpu == 0);
        REQUIRE(strcmp(info.backend_type, "cuda") == 0);
        REQUIRE(info.global_mem_bytes > 0);
        printf("[CUDA-DEVICE] %d: %s, %lu MB\n", i, info.name, (unsigned long)(info.global_mem_bytes/1024/1024));
    }
    cuda.close();
}
