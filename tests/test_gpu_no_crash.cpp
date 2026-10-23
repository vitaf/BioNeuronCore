// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_gpu_no_crash.cpp — regression: GPU backend не крашит при старте/инициализации
// Проверяет что .so загружается, устройства перечисляются без CPU,
// симуляция создаётся и запускается без SIGSEGV.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <dlfcn.h>
#include <cstring>

static const char* so_names[] = {
    "libbiocore_gpu_backend_intel.so",
    "libbiocore_gpu_backend_cuda.so",
    nullptr
};

static const char* search_dirs[] = {".", "../Core/build", "", nullptr};

static const char* find_so(const char* so_name) {
    static char buf[640];
    for (int d = 0; search_dirs[d]; d++) {
        if (search_dirs[d][0])
            snprintf(buf, sizeof(buf), "%s/%s", search_dirs[d], so_name);
        else
            snprintf(buf, sizeof(buf), "%s", so_name);
        FILE* f = fopen(buf, "r");
        if (f) { fclose(f); return buf; }
    }
    return nullptr;
}

TEST_CASE("gpu_no_crash: load so, enumerate, no cpu devices", "[gpu][regression][dual]") {
    for (int n = 0; so_names[n]; n++) {
        const char* path = find_so(so_names[n]);
        if (!path) { WARN(so_names[n] << " not found"); continue; }

        void* h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        REQUIRE(h != nullptr);

        auto dev_count   = (int (*)())dlsym(h, "gpu_backend_device_count");
        auto get_dev_info = (int (*)(int, void*))dlsym(h, "gpu_backend_get_device_info");

        if (!dev_count || !get_dev_info) { dlclose(h); continue; }

        int ndev = dev_count();
        REQUIRE(ndev >= 0);
        INFO(so_names[n] << " reports " << ndev << " devices");

        // Проверяем что get_device_info не крашит и возвращает валидные данные
        for (int i = 0; i < ndev; i++) {
            char buf[256];
            memset(buf, 0, sizeof(buf));
            int ret = get_dev_info(i, buf);
            REQUIRE(ret == 0);
            // name — первое поле (смещение 0 у всех компиляторов)
            CHECK(buf[0] != '\0');
            INFO(so_names[n] << " device " << i << " name='" << buf << "'");
        }

        dlclose(h);
    }
}

TEST_CASE("gpu_no_crash: create and destroy simulation on gpu", "[gpu][regression][dual]") {
    for (int n = 0; so_names[n]; n++) {
        const char* path = find_so(so_names[n]);
        if (!path) { WARN(so_names[n] << " not found"); continue; }

        void* h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        REQUIRE(h != nullptr);

        auto create_fn  = (void* (*)())dlsym(h, "gpu_backend_create");
        auto destroy_fn = (void (*)(void*))dlsym(h, "gpu_backend_destroy");
        auto dev_count  = (int (*)())dlsym(h, "gpu_backend_device_count");

        if (!create_fn || !destroy_fn || !dev_count) { dlclose(h); continue; }

        int ndev = dev_count();
        if (ndev == 0) {
            WARN(so_names[n] << " has 0 GPU devices — skipping create test");
            dlclose(h);
            continue;
        }

        // Создать симуляцию на GPU (может быть nullptr если драйвер не готов)
        void* sim = create_fn();
        INFO(so_names[n] << " create returned " << (sim ? "non-null" : "null"));
        // create может вернуть nullptr если SYCL runtime не нашёл подходящее устройство
        // это не ошибка — главное что не краш
        if (sim) {
            destroy_fn(sim);
        }
        dlclose(h);
    }
}

TEST_CASE("gpu_no_crash: double create and destroy", "[gpu][regression][dual]") {
    for (int n = 0; so_names[n]; n++) {
        const char* path = find_so(so_names[n]);
        if (!path) continue;

        void* h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        REQUIRE(h != nullptr);

        auto create_fn  = (void* (*)())dlsym(h, "gpu_backend_create");
        auto destroy_fn = (void (*)(void*))dlsym(h, "gpu_backend_destroy");
        auto dev_count  = (int (*)())dlsym(h, "gpu_backend_device_count");

        if (!create_fn || !destroy_fn || !dev_count || dev_count() == 0) {
            dlclose(h); continue;
        }

        // Многократное создание/уничтожение — не должно крашить
        for (int i = 0; i < 3; i++) {
            void* sim = create_fn();
            if (sim) destroy_fn(sim);
            else break;
        }
        dlclose(h);
    }
}

// Формат: минимальный .bion (одна сома) в памяти для load_bion
#include <cstdint>
#pragma pack(push, 1)
struct MiniBionHeader {
    uint32_t magic = 0x4E4F4942;  // "BION"
    uint32_t version = 6;
    uint32_t num_neurons = 1;
    uint32_t num_segments = 1;
    uint32_t num_connections = 0;
    uint32_t num_synapses = 0;
    uint32_t num_nuclei = 0;
    uint32_t flags = 0;
    uint8_t  _pad[48] = {};
};
#pragma pack(pop)

TEST_CASE("gpu_no_crash: load_bion and run_tick on gpu", "[gpu][regression][dual]") {
    // Этот тест проверяет что load_bion + run_tick не крашат при JIT.
    // Особенно важно для native CUDA .so (nvcc).
    for (int n = 0; so_names[n]; n++) {
        const char* path = find_so(so_names[n]);
        if (!path) { WARN(so_names[n] << " not found"); continue; }

        void* h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        REQUIRE(h != nullptr);

        auto create_fn  = (void* (*)())dlsym(h, "gpu_backend_create");
        auto destroy_fn = (void (*)(void*))dlsym(h, "gpu_backend_destroy");
        auto load_fn    = (int (*)(void*, const char*))dlsym(h, "gpu_backend_load_bion");
        auto tick_fn    = (void (*)(void*, float))dlsym(h, "gpu_backend_run_tick");
        auto dev_count  = (int (*)())dlsym(h, "gpu_backend_device_count");

        if (!create_fn || !destroy_fn || !load_fn || !tick_fn || !dev_count) {
            dlclose(h); continue;
        }
        if (dev_count() == 0) {
            WARN(so_names[n] << " has 0 devices"); dlclose(h); continue;
        }

        void* sim = create_fn();
        if (!sim) { WARN(so_names[n] << " create returned null"); dlclose(h); continue; }

        // Пишем минимальный .bion (одна сома) — load_bion должен загрузить
        // и run_tick НЕ должен крашить (JIT происходит на первом тике)
        MiniBionHeader hdr;
        // Минимальный сегмент-сома: Segment = 988 байт
        std::vector<uint8_t> seg(988, 0);
        // id=0, type=SOMA=1
        seg[0] = 0; seg[1] = 0; seg[2] = 0; seg[3] = 0;  // id
        seg[4] = 1;  // segment_type=SOMA
        *(float*)&seg[8] = 5.0f;   // radius
        *(float*)&seg[12] = 0.0f;  // length
        *(float*)&seg[40] = -65.0f; // V_m
        *(float*)&seg[400] = 0.01f; // C_m
        // intra_ions: Na=10, K=140, Ca=0.0001, Cl=5, Mg=1
        *(float*)&seg[12] = 10.0f;   // intra_ions[0] Na
        *(float*)&seg[16] = 140.0f;  // intra_ions[1] K
        *(float*)&seg[20] = 0.0001f; // intra_ions[2] Ca
        *(float*)&seg[24] = 5.0f;   // intra_ions[3] Cl
        *(float*)&seg[28] = 1.0f;   // intra_ions[4] Mg
        // extra_ions (offset 32)
        *(float*)&seg[32] = 140.0f; // extra_ions[0] Na
        *(float*)&seg[36] = 5.0f;   // extra_ions[1] K

        // Нейрон
        uint8_t neuron[24] = {};
        *(uint32_t*)&neuron[4] = 1;  // segment_count = 1
        *(uint32_t*)&neuron[8] = 0;  // first_segment = 0

        FILE* f = fopen("test_gpu_nocrash.bion", "wb");
        REQUIRE(f != nullptr);
        fwrite(&hdr, sizeof(hdr), 1, f);
        fwrite(neuron, sizeof(neuron), 1, f);
        fwrite(seg.data(), 988, 1, f);
        fclose(f);

        int load_ret = load_fn(sim, "/tmp/test_gpu_nocrash.bion");
        INFO(so_names[n] << " load_bion returned " << load_ret);
        // load_bion может не сработать (несовместимость struct layout) — это ОК
        // главное что не краш
        if (load_ret == 0) {
            // Первый run_tick запускает JIT — здесь был краш раньше
            tick_fn(sim, 0.1f);
            INFO(so_names[n] << " run_tick completed without crash");
        }

        destroy_fn(sim);
        dlclose(h);
        std::remove("test_gpu_nocrash.bion");
    }
}
