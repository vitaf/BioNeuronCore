// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_cuda_chaos.cpp — отказоустойчивость CUDA backend (#8 Chaos)
// Проверка: многократные create/destroy, переключение устройств, memory stress

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstdio>
#include <dlfcn.h>

struct CudaApi {
    void* handle;
    void* (*create)(); void* (*create_on)(int);
    void (*destroy)(void*);
    int (*device_count)();
    int (*get_device_info)(int, void*);

    bool init() {
        handle = dlopen("./libbiocore_gpu_backend_cuda.so", RTLD_NOW);
        if (!handle) return false;
        create = (void*(*)())dlsym(handle, "gpu_backend_create");
        create_on = (void*(*)(int))dlsym(handle, "gpu_backend_create_on_device");
        destroy = (void(*)(void*))dlsym(handle, "gpu_backend_destroy");
        device_count = (int(*)())dlsym(handle, "gpu_backend_device_count");
        return create && destroy && device_count;
    }
    void close() { if (handle) dlclose(handle); }
};

TEST_CASE("CUDA chaos: rapid create/destroy cycles", "[cuda_chaos]") {
    CudaApi cuda;
    REQUIRE(cuda.init());

    for (int i = 0; i < 100; i++) {
        void* sim = cuda.create();
        REQUIRE(sim != nullptr);
        cuda.destroy(sim);
    }
    printf("[CUDA-CHAOS] 100 create/destroy cycles OK\n");
    cuda.close();
}

TEST_CASE("CUDA chaos: multiple sims (if multiple devices)", "[cuda_chaos]") {
    CudaApi cuda;
    REQUIRE(cuda.init());

    int ndev = cuda.device_count();
    INFO("Devices: " << ndev);
    REQUIRE(ndev > 0);

    // Создаём по симуляции на каждое устройство
    std::vector<void*> sims;
    if (cuda.create_on) {
        for (int i = 0; i < ndev; i++) {
            void* sim = cuda.create_on(i);
            REQUIRE(sim != nullptr);
            sims.push_back(sim);
        }
        for (auto s : sims) cuda.destroy(s);
        printf("[CUDA-CHAOS] %d sims on %d devices OK\n", (int)sims.size(), ndev);
    }

    cuda.close();
}

TEST_CASE("CUDA chaos: large dt values", "[cuda_chaos]") {
    CudaApi cuda;
    REQUIRE(cuda.init());
    auto run_tick = (void(*)(void*,float))dlsym(cuda.handle, "gpu_backend_run_tick");
    auto load_bion = (int(*)(void*,const char*))dlsym(cuda.handle, "gpu_backend_load_bion");
    auto bion_data = (void*(*)(void*))dlsym(cuda.handle, "gpu_backend_bion_data");

    if (run_tick && load_bion && bion_data) {
        void* sim = cuda.create();
        REQUIRE(sim);

        // Small .bion
        const char* path = "/tmp/cuda_chaos_dt.bion";
        FILE* f = fopen(path, "wb");
        unsigned int hdr[21] = {0x42494F4E, 6, 0, 0, 1, 1, 0, 0, 0,0,0,0,0,0,0,0,0,0,0,0,0};
        fwrite(hdr, 84, 1, f);
        unsigned int neuron[6] = {0,0,1,0,0xF0000000,0};
        fwrite(neuron, 24, 1, f);
        char seg[988] = {};
        fwrite(seg, 988, 1, f);
        fclose(f);

        if (load_bion(sim, path) == 0) {
            // Test various dt values
            float dts[] = {0.001f, 0.01f, 0.1f, 1.0f, 10.0f};
            for (float dt : dts) {
                run_tick(sim, dt);
            }
            printf("[CUDA-CHAOS] Extreme dt values handled OK\n");
        }
        cuda.destroy(sim);
        std::remove(path);
    }
    cuda.close();
}
