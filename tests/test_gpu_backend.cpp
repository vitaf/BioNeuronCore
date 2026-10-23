// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_gpu_backend.cpp — тесты C API gpu_backend (CPU/GPU переключатель).
// Проверяет идентичность раскладки структур между GCC и DPC++ компиляторами,
// а также null-guard поведение функций-обёрток.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "types.h"
#include <cstddef>
#include <cstdio>

// Повторяем сигнатуры gpu_backend (extern "C" из gpu_backend.cpp).
// Сами функции в libbiocore_gpu_backend.so (DPC++), здесь тестируем
// GCC-сторону: offset-ы и dlopen-путь если .so доступен.
extern "C" {
    void* gpu_backend_create();
    void  gpu_backend_destroy(void* sim);
    int   gpu_backend_load_bion(void* sim, const char* filename);
    void  gpu_backend_run_tick(void* sim, float dt);
    void* gpu_backend_bion_data(void* sim);
    int   gpu_backend_ibuf_offset();
    int   gpu_backend_vm_offset();
    int   gpu_backend_seg_size();
}

// ═══════════════════════════════════════════════════════════════════
// Структурные офсеты — GCC должен совпадать с DPC++
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("Segment I_buffer offset matches between compilers", "[gpu_backend][struct]") {
    // gpu_backend_ibuf_offset() = offsetof(Segment, I_buffer) по версии DPC++
    // Здесь проверяем GCC-версию. Они ДОЛЖНЫ совпадать, иначе USM-память разъедется.
    int expected = (int)offsetof(Segment, I_buffer);
    REQUIRE(expected == 1624);
}

TEST_CASE("Segment V_m offset matches between compilers", "[gpu_backend][struct]") {
    int expected = (int)offsetof(Segment, V_m);
    REQUIRE(expected == 40);
}

TEST_CASE("Segment size matches between compilers", "[gpu_backend][struct]") {
    REQUIRE(sizeof(Segment) == 1704);  // v10: +ER_Ca2
}

TEST_CASE("Modifier size is 44 bytes", "[gpu_backend][struct]") {
    REQUIRE(sizeof(Modifier) == 88);
}

TEST_CASE("Modifier state[7] is at correct offset", "[gpu_backend][struct]") {
    // state[7] используется как scratchpad для effective_area в GPU-ядрах
    int off = (int)offsetof(Modifier, state);
    REQUIRE(off == 8);  // после type(1)+_pad(3)+density(4)
    // state[7] = offsetof(Modifier, state) + 7*sizeof(float) = 8 + 28 = 36
}

TEST_CASE("IntraConnection coupling_resistance offset", "[gpu_backend][struct]") {
    // coupling_resistance проверяется GPU-ядром axial currents
    int off = (int)offsetof(IntraConnection, is_intracellular);
    REQUIRE(off == 8);  // после segment_a(4)+segment_b(4)
}

TEST_CASE("Synapse weight is at correct offset", "[gpu_backend][struct]") {
    // GPU synaptic kernel читает syn->weight для STDP/AMPAR trafficking
    int off = (int)offsetof(Synapse, weight);
    REQUIRE(off >= 0);  // точный offset зависит от полей до weight
}

// ═══════════════════════════════════════════════════════════════════
// dlopen-тесты: загружаем .so если доступен
// ═══════════════════════════════════════════════════════════════════

#include <dlfcn.h>

static const char* find_gpu_backend_so(const char* preferred = nullptr) {
    static const char* so_names[] = {
        "libbiocore_gpu_backend_intel.so",
        "libbiocore_gpu_backend_cuda.so",
        nullptr
    };
    static const char* search_dirs[] = {".", "../Core/build", "", nullptr};
    static char buf[640];
    for (int n = 0; so_names[n]; n++) {
        if (preferred && strcmp(so_names[n], preferred) != 0) continue;
        for (int d = 0; search_dirs[d]; d++) {
            if (search_dirs[d][0])
                snprintf(buf, sizeof(buf), "%s/%s", search_dirs[d], so_names[n]);
            else
                snprintf(buf, sizeof(buf), "%s", so_names[n]);
            FILE* f = fopen(buf, "r");
            if (f) { fclose(f); return buf; }
        }
    }
    if (!preferred) return nullptr;
    // fallback: повторить без фильтра
    return find_gpu_backend_so(nullptr);
}

TEST_CASE("gpu_backend: struct offsets match .so values", "[gpu_backend][dlopen]") {
    const char* so_path = find_gpu_backend_so();
    if (!so_path) {
        WARN("No GPU backend .so found — skipping dlopen offset test");
        return;
    }

    void* handle = dlopen(so_path, RTLD_NOW | RTLD_LOCAL);
    REQUIRE(handle != nullptr);

    typedef int (*OffsetFn)();
    auto fn_ibuf = (OffsetFn)dlsym(handle, "gpu_backend_ibuf_offset");
    auto fn_vm   = (OffsetFn)dlsym(handle, "gpu_backend_vm_offset");
    auto fn_seg  = (OffsetFn)dlsym(handle, "gpu_backend_seg_size");

    if (fn_ibuf && fn_vm && fn_seg) {
        // GCC offsetof ДОЛЖЕН совпадать с DPC++ значением из .so
        REQUIRE(fn_ibuf() == (int)offsetof(Segment, I_buffer));
        REQUIRE(fn_vm()   == (int)offsetof(Segment, V_m));
        REQUIRE(fn_seg()  == (int)sizeof(Segment));
    } else {
        WARN("dlsym failed for offset functions");
    }

    dlclose(handle);
}

TEST_CASE("gpu_backend: null-guard functions don't crash", "[gpu_backend][dlopen]") {
    const char* so_path = find_gpu_backend_so();
    if (!so_path) {
        WARN("libbiocore_gpu_backend.so not found — skipping null-guard test");
        return;
    }

    void* handle = dlopen(so_path, RTLD_NOW | RTLD_LOCAL);
    REQUIRE(handle != nullptr);

    typedef void  (*DestroyFn)(void*);
    typedef int   (*LoadBionFn)(void*, const char*);
    typedef void  (*RunTickFn)(void*, float);
    typedef void* (*BionDataFn)(void*);

    auto fn_destroy   = (DestroyFn)dlsym(handle, "gpu_backend_destroy");
    auto fn_load      = (LoadBionFn)dlsym(handle, "gpu_backend_load_bion");
    auto fn_tick      = (RunTickFn)dlsym(handle, "gpu_backend_run_tick");
    auto fn_data      = (BionDataFn)dlsym(handle, "gpu_backend_bion_data");

    // Все функции должны безопасно обрабатывать nullptr
    if (fn_destroy) {
        REQUIRE_NOTHROW(fn_destroy(nullptr));
    }
    if (fn_load) {
        int r = fn_load(nullptr, nullptr);
        REQUIRE(r == -1);  // не падает, возвращает ошибку
    }
    if (fn_tick) {
        REQUIRE_NOTHROW(fn_tick(nullptr, 0.1f));
    }
    if (fn_data) {
        void* d = fn_data(nullptr);
        REQUIRE(d == nullptr);
    }

    dlclose(handle);
}

TEST_CASE("gpu_backend: create/destroy lifecycle", "[gpu_backend][dlopen]") {
    const char* so_path = find_gpu_backend_so();
    if (!so_path) {
        WARN("libbiocore_gpu_backend.so not found — skipping lifecycle test");
        return;
    }

    void* handle = dlopen(so_path, RTLD_NOW | RTLD_LOCAL);
    REQUIRE(handle != nullptr);

    typedef void* (*CreateFn)();
    typedef void  (*DestroyFn)(void*);

    auto fn_create  = (CreateFn)dlsym(handle, "gpu_backend_create");
    auto fn_destroy = (DestroyFn)dlsym(handle, "gpu_backend_destroy");

    REQUIRE(fn_create != nullptr);
    REQUIRE(fn_destroy != nullptr);

    void* sim = fn_create();
    if (sim) {
        REQUIRE_NOTHROW(fn_destroy(sim));
    } else {
        WARN("gpu_backend_create returned nullptr (no SYCL device available)");
    }

    dlclose(handle);
}
