// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_cuda_security.cpp — безопасность CUDA backend (#6 Security)
// Проверка: bad .bion, nullptr, пустая сеть, переполнения

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstdio>
#include <dlfcn.h>

struct CudaApi {
    void* handle; void* sim;
    void* (*create)(); void (*destroy)(void*);
    int (*load_bion)(void*, const char*);
    void (*run_tick)(void*, float);
    void* (*get_info)(int, void*);

    bool init() {
        handle = dlopen("./libbiocore_gpu_backend_cuda.so", RTLD_NOW);
        if (!handle) return false;
        create = (void*(*)())dlsym(handle, "gpu_backend_create");
        destroy = (void(*)(void*))dlsym(handle, "gpu_backend_destroy");
        load_bion = (int(*)(void*,const char*))dlsym(handle, "gpu_backend_load_bion");
        run_tick = (void(*)(void*,float))dlsym(handle, "gpu_backend_run_tick");
        return create && destroy && load_bion && run_tick;
    }
    void close() { if (sim) destroy(sim); if (handle) dlclose(handle); }
};

TEST_CASE("CUDA security: null and invalid inputs", "[cuda_security]") {
    CudaApi cuda;
    REQUIRE(cuda.init());

    SECTION("create_on_device with out-of-range falls back to device 0") {
        auto create_on = (void*(*)(int))dlsym(cuda.handle, "gpu_backend_create_on_device");
        if (create_on) {
            void* s = create_on(9999);  // out of range — должен fallback на device 0
            CHECK(s != nullptr);        // fallback behaviour — OK
            if (s) cuda.destroy(s);     // clean up
            void* s2 = create_on(-1);   // negative — same fallback
            CHECK(s2 != nullptr);
            if (s2) cuda.destroy(s2);
        }
    }

    SECTION("load_bion with nullptr") {
        CHECK(cuda.load_bion(nullptr, "/tmp/test.bion") < 0);
        cuda.sim = cuda.create();
        REQUIRE(cuda.sim);
        CHECK(cuda.load_bion(cuda.sim, nullptr) < 0);
    }

    SECTION("load_bion with nonexistent file") {
        cuda.sim = cuda.create();
        REQUIRE(cuda.sim);
        CHECK(cuda.load_bion(cuda.sim, "/nonexistent/file.bion") < 0);
    }

    SECTION("run_tick with nullptr") {
        cuda.run_tick(nullptr, 0.1f);  // should not crash
        SUCCEED("nullptr run_tick handled");
    }

    SECTION("run_tick without loaded network") {
        cuda.sim = cuda.create();
        REQUIRE(cuda.sim);
        cuda.run_tick(cuda.sim, 0.1f);  // should not crash
        SUCCEED("no-network run_tick handled");
    }

    SECTION("destroy with nullptr") {
        cuda.destroy(nullptr);  // should not crash
        SUCCEED("nullptr destroy handled");
    }

    cuda.destroy(cuda.sim); cuda.sim = nullptr;
    cuda.close();
}

TEST_CASE("CUDA security: bad .bion format", "[cuda_security]") {
    CudaApi cuda;
    REQUIRE(cuda.init());
    cuda.sim = cuda.create();
    REQUIRE(cuda.sim);

    SECTION("empty file") {
        FILE* f = fopen("/tmp/cuda_sec_empty.bion", "wb"); fclose(f);
        CHECK(cuda.load_bion(cuda.sim, "/tmp/cuda_sec_empty.bion") < 0);
        std::remove("/tmp/cuda_sec_empty.bion");
    }

    SECTION("bad magic") {
        FILE* f = fopen("/tmp/cuda_sec_bad.bion", "wb");
        int bad_magic = 0xDEADBEEF;
        fwrite(&bad_magic, 4, 1, f); fclose(f);
        CHECK(cuda.load_bion(cuda.sim, "/tmp/cuda_sec_bad.bion") < 0);
        std::remove("/tmp/cuda_sec_bad.bion");
    }

    SECTION("truncated file") {
        FILE* f = fopen("/tmp/cuda_sec_trunc.bion", "wb");
        char buf[100] = {};
        fwrite(buf, 100, 1, f); fclose(f);
        CHECK(cuda.load_bion(cuda.sim, "/tmp/cuda_sec_trunc.bion") < 0);
        std::remove("/tmp/cuda_sec_trunc.bion");
    }

    cuda.destroy(cuda.sim); cuda.sim = nullptr;
    cuda.close();
}
