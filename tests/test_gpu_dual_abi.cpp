// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_gpu_dual_abi.cpp — ABI-совместимость двух GPU бекендов
// Проверяет что libbiocore_gpu_backend_intel.so и libbiocore_gpu_backend_cuda.so
// экспортируют одинаковые символы и возвращают одинаковые struct offsets.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <dlfcn.h>
#include <cstring>

static const char* so_names[] = {
    "libbiocore_gpu_backend_intel.so",
    "libbiocore_gpu_backend_cuda.so",
    nullptr
};

static const char* expected_symbols[] = {
    "gpu_backend_create",
    "gpu_backend_destroy",
    "gpu_backend_load_bion",
    "gpu_backend_run_tick",
    "gpu_backend_bion_data",
    "gpu_backend_device_count",
    "gpu_backend_get_device_info",
    "gpu_backend_create_on_device",
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

TEST_CASE("dual_abi: both .so export identical symbols", "[gpu][dual][abi]") {
    int loaded = 0;
    for (int n = 0; so_names[n]; n++) {
        const char* path = find_so(so_names[n]);
        if (!path) {
            WARN(so_names[n] << " not found — skipping");
            continue;
        }
        void* h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        REQUIRE(h != nullptr);
        for (int s = 0; expected_symbols[s]; s++) {
            void* sym = dlsym(h, expected_symbols[s]);
            INFO(so_names[n] << " missing symbol: " << expected_symbols[s]);
            REQUIRE(sym != nullptr);
        }
        dlclose(h);
        loaded++;
    }
    REQUIRE(loaded > 0);  // хотя бы один .so должен быть
}

TEST_CASE("dual_abi: offset consistency between .so files", "[gpu][dual][abi]") {
    typedef int (*IntFn)();
    struct SoData {
        void* handle;
        IntFn ibuf_offset;
        IntFn vm_offset;
        IntFn seg_size;
        IntFn dev_count;
        IntFn dev_info;
    };
    SoData sos[2];
    int loaded = 0;

    for (int n = 0; n < 2; n++) {
        const char* path = find_so(so_names[n]);
        if (!path) continue;
        auto& sd = sos[loaded];
        sd.handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (!sd.handle) continue;
        sd.ibuf_offset = (IntFn)dlsym(sd.handle, "gpu_backend_ibuf_offset");
        sd.vm_offset   = (IntFn)dlsym(sd.handle, "gpu_backend_vm_offset");
        sd.seg_size    = (IntFn)dlsym(sd.handle, "gpu_backend_seg_size");
        sd.dev_count   = (IntFn)dlsym(sd.handle, "gpu_backend_device_count");
        sd.dev_info    = (IntFn)dlsym(sd.handle, "gpu_backend_get_device_info");
        if (sd.ibuf_offset && sd.vm_offset && sd.seg_size) loaded++;
    }

    if (loaded < 2) {
        WARN("Only " << loaded << " .so found — skipping cross-so offset comparison");
        for (int i = 0; i < loaded; i++) dlclose(sos[i].handle);
        return;
    }

    // Оба .so должны возвращать одинаковые offset-ы
    REQUIRE(sos[0].ibuf_offset() == sos[1].ibuf_offset());
    REQUIRE(sos[0].vm_offset()   == sos[1].vm_offset());
    REQUIRE(sos[0].seg_size()    == sos[1].seg_size());

    dlclose(sos[0].handle);
    dlclose(sos[1].handle);
}

TEST_CASE("dual_abi: GpuDeviceInfo.device_count and get_device_info work", "[gpu][dual][abi]") {
    typedef int (*DeviceCountFn)();
    typedef int (*GetDeviceInfoFn)(int, void*);

    for (int n = 0; so_names[n]; n++) {
        const char* path = find_so(so_names[n]);
        if (!path) { WARN(so_names[n] << " not found"); continue; }
        void* h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (!h) continue;

        auto dev_count   = (DeviceCountFn)dlsym(h, "gpu_backend_device_count");
        auto get_dev_info = (GetDeviceInfoFn)dlsym(h, "gpu_backend_get_device_info");

        REQUIRE(dev_count != nullptr);
        REQUIRE(get_dev_info != nullptr);

        int ndev = dev_count();
        CHECK(ndev >= 0);
        INFO(so_names[n] << " reports " << ndev << " devices");

        // Проверяем что get_device_info не крашит и возвращает 0
        // (поля struct зависят от компилятора — не проверяем значения)
        for (int i = 0; i < ndev; i++) {
            char buf[256];
            memset(buf, 0, sizeof(buf));
            int ret = get_dev_info(i, buf);
            CHECK(ret == 0);
            // Хотя бы name не пустой (первое поле, смещение 0 у всех компиляторов)
            CHECK(buf[0] != '\0');
        }
        dlclose(h);
    }
}

TEST_CASE("dual_abi: single .so survives other missing", "[gpu][dual][abi]") {
    for (int n = 0; so_names[n]; n++) {
        const char* path = find_so(so_names[n]);
        if (!path) { WARN(so_names[n] << " not found — skipping single-load test"); continue; }

        void* h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        REQUIRE(h != nullptr);

        auto create_fn = (void* (*)())dlsym(h, "gpu_backend_create");
        auto destroy_fn = (void (*)(void*))dlsym(h, "gpu_backend_destroy");
        auto dev_count  = (int (*)())dlsym(h, "gpu_backend_device_count");

        REQUIRE(create_fn != nullptr);
        REQUIRE(destroy_fn != nullptr);
        REQUIRE(dev_count != nullptr);

        // Создать симуляцию — может быть nullptr если нет устройств (это ОК)
        void* sim = create_fn();
        INFO(so_names[n] << " gpu_backend_create returned " << (sim ? "non-null" : "null"));
        if (sim) {
            int ndev = dev_count();
            CHECK(ndev >= 0);
            destroy_fn(sim);
        }
        dlclose(h);
    }
}
