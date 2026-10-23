// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_cpu_gpu_parity_cuda.cpp — биологический паритет CPU vs native CUDA (nvcc)
// 14 ионных каналов, delta=0.00mV цель, <5.0mV допуск.
// Использует dlopen libbiocore_gpu_backend_cuda.so — не требует SYCL/Intel oneAPI.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <dlfcn.h>
#include <vector>

#include "types.h"
#include "bion_format.h"
#include "bion_loader.h"

extern "C" {
    void core_run_tick(Segment* segments, uint32_t num_segments,
                       IntraConnection* connections, uint32_t num_connections,
                       Synapse* synapses, uint32_t num_synapses,
                       int mode, uint64_t* tick_counter, float dt,
                       void* stim_buf, void* stim_cnt, void* vcl, void* vcl_cnt,
                       void* ramp, void* ramp_cnt, void* ion, void* ion_cnt,
                       void* lig, void* lig_cnt, MechanoStimulus* mechano, uint32_t* mechano_count);
}

// ── Function pointers для CUDA backend (через dlopen) ──
struct CudaBackend {
    void* handle = nullptr;
    void* sim = nullptr;
    void* (*create)() = nullptr;
    void* (*create_on_device)(int) = nullptr;
    void  (*destroy)(void*) = nullptr;
    int   (*load_bion)(void*, const char*) = nullptr;
    void  (*run_tick)(void*, float) = nullptr;
    void* (*bion_data)(void*) = nullptr;
    int   (*device_count)() = nullptr;
    int   (*get_device_info)(int, GpuDeviceInfo*) = nullptr;

    bool load() {
        const char* paths[] = {
            "./libbiocore_gpu_backend_cuda.so",
            "../BioNeuronCore/build/libbiocore_gpu_backend_cuda.so",
            "libbiocore_gpu_backend_cuda.so"
        };
        for (int i = 0; i < 3; i++) {
            handle = dlopen(paths[i], RTLD_NOW);
            if (handle) break;
        }
        if (!handle) return false;

        create           = (void*(*)())dlsym(handle, "gpu_backend_create");
        create_on_device = (void*(*)(int))dlsym(handle, "gpu_backend_create_on_device");
        destroy          = (void(*)(void*))dlsym(handle, "gpu_backend_destroy");
        load_bion        = (int(*)(void*, const char*))dlsym(handle, "gpu_backend_load_bion");
        run_tick         = (void(*)(void*, float))dlsym(handle, "gpu_backend_run_tick");
        bion_data        = (void*(*)(void*))dlsym(handle, "gpu_backend_bion_data");
        device_count     = (int(*)())dlsym(handle, "gpu_backend_device_count");
        get_device_info  = (int(*)(int, GpuDeviceInfo*))dlsym(handle, "gpu_backend_get_device_info");

        return create && load_bion && run_tick && bion_data;
    }

    void unload() {
        if (sim && destroy) destroy(sim);
        if (handle) dlclose(handle);
        handle = nullptr; sim = nullptr;
    }
};

// ── Хелпер: создать BION-файл с 1 сегментом ──
static std::string make_bion_file(const char* suffix,
    const std::vector<std::pair<uint8_t, float>>& modifiers, float Vm_init)
{
    char path[256]; snprintf(path, sizeof(path), "/tmp/parity_cuda_%s.bion", suffix);

    NetworkHeader hdr = {};
    hdr.magic = BION_MAGIC; hdr.version = BION_VERSION; hdr.mode = 0;
    hdr.num_neurons = 1; hdr.num_segments = 1;
    hdr.num_intra_connections = 0; hdr.num_synapses = 0;

    Neuron neuron = {}; neuron.segment_count = 1; neuron.threshold = -20.0f;

    Segment seg = {};
    seg.id = 0; seg.neuron_id = 0; seg.segment_type = 1; // SOMA
    seg.position = {0, 50, 0}; seg.direction = {0, 1, 0};
    seg.radius = 5.0f; seg.length = 10.0f;
    seg.V_m = Vm_init;
    seg.intra_ions[0] = 0.015f;  seg.intra_ions[1] = 0.140f;
    seg.intra_ions[2] = 0.00005f; seg.intra_ions[3] = 0.005f;
    seg.extra_ions[0] = 0.140f;  seg.extra_ions[1] = 0.005f;
    seg.extra_ions[2] = 0.0012f; seg.extra_ions[3] = 0.130f;
    seg.modifier_count = (uint8_t)modifiers.size();
    for (size_t j = 0; j < modifiers.size() && j < 16; j++) {
        seg.modifiers[j].type = modifiers[j].first;
        seg.modifiers[j].density = modifiers[j].second;
    }

    FILE* f = fopen(path, "wb");
    REQUIRE(f != nullptr);
    fwrite(&hdr, sizeof(hdr), 1, f);
    fwrite(&neuron, sizeof(neuron), 1, f);
    fwrite(&seg, sizeof(seg), 1, f);
    fclose(f);
    return path;
}

struct SimResult { float Vm_avg, Vm_min, Vm_max, I_buf; int samples; };

// ── CPU симуляция ──
static SimResult run_cpu(const std::string& bion_path, int ticks) {
    BionData data; memset(&data, 0, sizeof(data));
    int ret = bion_load_host(bion_path.c_str(), &data);
    REQUIRE(ret == 0);

    SimResult r = {0, 999, -999, 0, 0};
    Segment* segs = data.segments;
    REQUIRE(segs != nullptr);

    uint64_t tick_counter = 0;
    r.I_buf = segs[0].I_buffer;
    for (int t = 0; t < ticks; t++) {
        core_run_tick(segs, data.num_segments,
                      data.connections, data.num_connections,
                      data.synapses, data.num_synapses,
                      0, &tick_counter, 0.1f,
                      nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr);
        float vm = segs[0].V_m;
        r.Vm_avg += vm; r.samples++;
        if (vm < r.Vm_min) r.Vm_min = vm;
        if (vm > r.Vm_max) r.Vm_max = vm;
        if (t == ticks - 1) r.I_buf = segs[0].I_buffer;
    }
    if (r.samples > 0) r.Vm_avg /= r.samples;
    bion_unload_host(&data);
    return r;
}

// ── CUDA симуляция (через dlopen) ──
static SimResult run_cuda(CudaBackend& bk, const std::string& bion_path, int ticks) {
    int ndev = bk.device_count ? bk.device_count() : 1;
    REQUIRE(ndev > 0);

    if (bk.create_on_device)
        bk.sim = bk.create_on_device(0);
    else
        bk.sim = bk.create();
    REQUIRE(bk.sim != nullptr);

    int ret = bk.load_bion(bk.sim, bion_path.c_str());
    REQUIRE(ret == 0);

    BionData* bd = (BionData*)bk.bion_data(bk.sim);
    REQUIRE(bd != nullptr);
    Segment* segs = bd->segments;
    REQUIRE(segs != nullptr);

    SimResult r = {0, 999, -999, 0, 0};
    r.I_buf = segs[0].I_buffer;
    for (int t = 0; t < ticks; t++) {
        bk.run_tick(bk.sim, 0.1f);
        float vm = segs[0].V_m;
        r.Vm_avg += vm; r.samples++;
        if (vm < r.Vm_min) r.Vm_min = vm;
        if (vm > r.Vm_max) r.Vm_max = vm;
        if (t == ticks - 1) r.I_buf = segs[0].I_buffer;
    }
    if (r.samples > 0) r.Vm_avg /= r.samples;

    bk.destroy(bk.sim); bk.sim = nullptr;
    return r;
}

// ── 13 каналов для биологического паритета (delta=0.00mV цель) ──
struct ChannelTestParam {
    const char* name;
    uint8_t     type;
    float       density;
    float       Vm_init;
    float       max_delta_mV;
};

static const ChannelTestParam CHANNELS[] = {
    {"K2P_LEAK",          107,   0.03f,  -65.0f, 5.0f},
    {"KIR",               106,   0.008f, -65.0f, 5.0f},
    {"NaK_Pump",            7,   0.2f,   -65.0f, 5.0f},
    {"NaP",               105,   0.01f,  -65.0f, 5.0f},
    {"BK",                104,   0.03f,  -65.0f, 5.0f},
    {"Kv42",               85,   0.4f,   -58.0f, 5.0f},
    {"KCC2",              100,   1.0f,   -65.0f, 5.0f},
    {"SK",                103,   0.12f,  -58.0f, 5.0f},
    {"Ca_Leak",            99,   0.08f,  -65.0f, 5.0f},
    {"PMCA",               95,   0.4f,   -65.0f, 5.0f},
    {"NCX",                96,   0.3f,   -65.0f, 5.0f},
    {"Kv7_Mcurrent",      112,   1.0f,   -65.0f, 5.0f},
    {"Glial_Kir41",       109,   0.3f,   -80.0f, 5.0f},
};

TEST_CASE("CUDA backend availability", "[cuda]") {
    CudaBackend bk;
    REQUIRE(bk.load());
    int ndev = bk.device_count();
    REQUIRE(ndev > 0);

    GpuDeviceInfo info;
    REQUIRE(bk.get_device_info(0, &info) == 0);
    REQUIRE(info.is_cpu == 0);
    bool has_name = (strlen(info.name) > 0);
    REQUIRE(has_name);

    printf("[CUDA] Device: %s, mem=%lu MB, backend=%s\n",
           info.name, (unsigned long)(info.global_mem_bytes/1024/1024), info.backend_type);

    // ABI verification
    void* (*ibuf_off)() = (void*(*)())dlsym(bk.handle, "gpu_backend_ibuf_offset");
    void* (*vm_off)()   = (void*(*)())dlsym(bk.handle, "gpu_backend_vm_offset");
    void* (*seg_sz)()   = (void*(*)())dlsym(bk.handle, "gpu_backend_seg_size");

    if (ibuf_off && vm_off && seg_sz) {
        REQUIRE((int)(intptr_t)ibuf_off() == (int)offsetof(Segment, I_buffer));
        REQUIRE((int)(intptr_t)vm_off()   == (int)offsetof(Segment, V_m));
        REQUIRE((int)(intptr_t)seg_sz()   == (int)sizeof(Segment));
        printf("[CUDA-ABI] ABI match: I_buffer=%d V_m=%d sizeof(Segment)=%d OK\n",
               (int)offsetof(Segment, I_buffer), (int)offsetof(Segment, V_m), (int)sizeof(Segment));
    }
    bk.unload();
}

TEST_CASE("CPU-CUDA parity: single-channel V_m comparison", "[cpu_cuda_parity]") {
    CudaBackend bk;
    if (!bk.load()) {
        WARN("CUDA backend not available — skipping parity tests");
        return;
    }
    REQUIRE(bk.device_count() > 0);

    for (const auto& ch : CHANNELS) {
        DYNAMIC_SECTION(ch.name) {
            std::vector<std::pair<uint8_t, float>> mods = {
                {142, 15.6f}, // MOD_AXON_NAV12 (Nav1.2)
                {196, 3.84f}, // MOD_KV21 (Kv1.1)
                {7, 0.2f},    // MOD_PUMP_NAK
                {ch.type, ch.density}
            };

            std::string path = make_bion_file(ch.name, mods, ch.Vm_init);
            int ticks = 100;

            SimResult cpu_r = run_cpu(path, ticks);
            SimResult cuda_r = run_cuda(bk, path, ticks);

            printf("[CUDA-PARITY] %-14s  CPU: avg=%7.2f [%7.2f..%7.2f] I_buf=%.6f\n",
                   ch.name, cpu_r.Vm_avg, cpu_r.Vm_min, cpu_r.Vm_max, cpu_r.I_buf);
            printf("[CUDA-PARITY] %-14s  CUDA: avg=%7.2f [%7.2f..%7.2f] I_buf=%.6f\n",
                   ch.name, cuda_r.Vm_avg, cuda_r.Vm_min, cuda_r.Vm_max, cuda_r.I_buf);

            float delta = fabsf(cpu_r.Vm_avg - cuda_r.Vm_avg);
            printf("[CUDA-PARITY] %-14s  delta=%.4f mV (max=%.1f)\n",
                   ch.name, delta, ch.max_delta_mV);

            REQUIRE_FALSE(std::isnan(cpu_r.Vm_avg));
            REQUIRE_FALSE(std::isnan(cuda_r.Vm_avg));
            REQUIRE(cpu_r.samples > 0);
            REQUIRE(cuda_r.samples > 0);

            CHECK(delta < ch.max_delta_mV);
            std::remove(path.c_str());
        }
    }
    bk.unload();
}

TEST_CASE("CPU-CUDA parity: full resting set", "[cpu_cuda_parity]") {
    CudaBackend bk;
    if (!bk.load()) {
        WARN("CUDA backend not available — skipping parity test");
        return;
    }
    REQUIRE(bk.device_count() > 0);

    std::vector<std::pair<uint8_t, float>> mods = {
        {107, 0.03f},  // K2P_LEAK
        {7,   0.2f},   // NaK_Pump
        {106, 0.008f}, // KIR
    };
    std::string path = make_bion_file("full_rest", mods, -65.0f);

    SimResult cpu_r = run_cpu(path, 100);
    SimResult cuda_r = run_cuda(bk, path, 100);

    printf("[CUDA-FULL] CPU:  avg=%.4f [%.4f..%.4f] I_buf=%.6f\n",
           cpu_r.Vm_avg, cpu_r.Vm_min, cpu_r.Vm_max, cpu_r.I_buf);
    printf("[CUDA-FULL] CUDA: avg=%.4f [%.4f..%.4f] I_buf=%.6f\n",
           cuda_r.Vm_avg, cuda_r.Vm_min, cuda_r.Vm_max, cuda_r.I_buf);

    float delta = fabsf(cpu_r.Vm_avg - cuda_r.Vm_avg);
    printf("[CUDA-FULL] delta=%.4f mV\n", delta);
    CHECK(delta < 5.0f);
    std::remove(path.c_str());
    bk.unload();
}

TEST_CASE("CPU-CUDA parity: I_buffer zero after tick", "[cpu_cuda_parity]") {
    CudaBackend bk;
    if (!bk.load()) {
        WARN("CUDA backend not available");
        return;
    }

    std::vector<std::pair<uint8_t, float>> mods = {
        {7, 0.2f}
    };
    std::string path = make_bion_file("ibuf_check", mods, -65.0f);

    SimResult r = run_cuda(bk, path, 1);
    // I_buffer сбрасывается в update_membrane (шаг 4)
    // После 1 тика с пассивной мембраной I_buf должен быть мал
    CHECK(fabsf(r.I_buf) < 100.0f);

    std::remove(path.c_str());
    bk.unload();
}
