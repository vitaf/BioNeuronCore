// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_cpu_gpu_parity.cpp — сравнение CPU vs GPU V_m и I_buffer
// после N тиков симуляции для каждого ионного канала.
// Использует прямой доступ к Simulation (GPU) и bion_load_host+core_run_tick (CPU)
// в обход SimulationBridge для исключения промежуточных гонок.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <vector>
#include <sycl/sycl.hpp>

#include "types.h"
#include "simulation.h"
#include "bion_format.h"
#include "bion_loader.h"
#include "kernel_dispatch.h"

extern "C" {
    void core_run_tick(Segment* segments, uint32_t num_segments,
                       IntraConnection* connections, uint32_t num_connections,
                       Synapse* synapses, uint32_t num_synapses,
                       int mode, uint64_t* tick_counter, float dt,
                       void* stim_buf, void* stim_cnt, void* vcl, void* vcl_cnt,
                       void* ramp, void* ramp_cnt, void* ion, void* ion_cnt,
                       void* lig, void* lig_cnt, MechanoStimulus* mechano, uint32_t* mechano_count);
}

// ── Хелпер: создать BION-файл с 1 сегментом и заданными модификаторами ──
static std::string make_bion_file(const char* suffix,
    const std::vector<std::pair<uint8_t, float>>& modifiers, float Vm_init)
{
    char path[256]; snprintf(path, sizeof(path), "/tmp/parity_%s.bion", suffix);

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

// ── Статистика ──
struct SimResult { float Vm_avg, Vm_min, Vm_max, I_buf; int samples; };

// ── GPU симуляция (SYCL) ──
static SimResult run_gpu(const std::string& bion_path, int ticks) {
    sycl::device dev(sycl::default_selector_v);
    Simulation sim(dev);

    int ret = sim.load_bion(bion_path.c_str());
    REQUIRE(ret == 0);

    SimResult r = {0, 999, -999, 0, 0};
    Segment* segs = (Segment*)sim.get_bion_data()->segments;
    REQUIRE(segs != nullptr);

    r.I_buf = segs[0].I_buffer;
    for (int t = 0; t < ticks; t++) {
        sim.run_tick(0.1f);
        float vm = segs[0].V_m;
        r.Vm_avg += vm; r.samples++;
        if (vm < r.Vm_min) r.Vm_min = vm;
        if (vm > r.Vm_max) r.Vm_max = vm;
        if (t == ticks - 1) r.I_buf = segs[0].I_buffer;
    }
    if (r.samples > 0) r.Vm_avg /= r.samples;
    return r;
}

// ── CPU симуляция (хост) ──
static SimResult run_cpu(const std::string& bion_path, int ticks) {
    BionData data;
    memset(&data, 0, sizeof(data));

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

// ── Параметризованный тест по каналам ──
struct ChannelTestParam {
    const char* name;
    uint8_t     type;
    float       density;
    float       Vm_init;
    float       max_delta_mV;   // допустимая CPU-GPU разница по avg V_m
};

static const ChannelTestParam CHANNELS[] = {
    // Базовые каналы
    {"K2P_LEAK",          107,   0.03f,  -65.0f, 5.0f},
    {"KIR",               106,   0.008f, -65.0f, 5.0f},
    {"NaK_Pump",            7,   0.2f,   -65.0f, 5.0f},
    // 4FIXES (v0.19.4)
    {"NaP",               105,   0.01f,  -65.0f, 5.0f},
    {"BK",                104,   0.03f,  -65.0f, 5.0f},
    {"Kv42",               85,   0.4f,   -58.0f, 5.0f},
    {"KCC2",              100,   1.0f,   -65.0f, 5.0f},
    // Ca²⁺-зависимые
    {"SK",                103,   0.12f,  -58.0f, 5.0f},
    {"Ca_Leak",            99,   0.08f,  -65.0f, 5.0f},
    {"Ca_Buffer",          98,   0.6f,   -65.0f, 5.0f},
    {"PMCA",               95,   0.4f,   -65.0f, 5.0f},
    {"NCX",                96,   0.3f,   -65.0f, 5.0f},
    {"Kv7_Mcurrent",      112,   1.0f,   -65.0f, 5.0f},
    // PIR (dIN-специфичные)
    {"PIR_Ttype_Ca",       96,   0.7f,   -58.0f, 5.0f},
    {"PIR_HCN",            97,   0.2f,   -58.0f, 5.0f},
    {"PIR_Rebound",        93,   0.7f,   -58.0f, 5.0f},
    // Глиальные
    {"Glial_Kir41",       109,   0.3f,   -80.0f, 5.0f},
    {"Glial_NaK_Pump",    110,   0.2f,   -80.0f, 5.0f},
};

TEST_CASE("CPU-GPU parity: single-channel V_m comparison", "[cpu_gpu_parity]") {
    for (const auto& ch : CHANNELS) {
        DYNAMIC_SECTION(ch.name) {
            // Базовые модификаторы: Nav1.2 + Kv1.1 + Pump
            std::vector<std::pair<uint8_t, float>> mods = {
                {142, 15.6f}, // MOD_AXON_NAV12 (Nav1.2)
                {196, 3.84f}, // MOD_KV21 (Kv1.1)
                {7, 0.2f},    // MOD_PUMP_NAK
                {ch.type, ch.density}
            };

            std::string path = make_bion_file(ch.name, mods, ch.Vm_init);
            int ticks = 100;

            SimResult cpu_r = run_cpu(path, ticks);
            SimResult gpu_r = run_gpu(path, ticks);

            printf("[PARITY] %-14s  CPU: avg=%7.2f [%7.2f..%7.2f] I_buf=%.6f\n",
                   ch.name, cpu_r.Vm_avg, cpu_r.Vm_min, cpu_r.Vm_max, cpu_r.I_buf);
            printf("[PARITY] %-14s  GPU: avg=%7.2f [%7.2f..%7.2f] I_buf=%.6f\n",
                   ch.name, gpu_r.Vm_avg, gpu_r.Vm_min, gpu_r.Vm_max, gpu_r.I_buf);

            float delta = fabsf(cpu_r.Vm_avg - gpu_r.Vm_avg);
            printf("[PARITY] %-14s  delta=%.2f mV (max=%.1f)\n",
                   ch.name, delta, ch.max_delta_mV);

            REQUIRE_FALSE(std::isnan(cpu_r.Vm_avg));
            REQUIRE_FALSE(std::isnan(gpu_r.Vm_avg));
            REQUIRE(cpu_r.samples > 0);
            REQUIRE(gpu_r.samples > 0);

            CHECK(delta < ch.max_delta_mV);
            std::remove(path.c_str());
        }
    }
}

// ── Бинарная совместимость загрузки: все поля сегмента CPU vs GPU ──
TEST_CASE("CPU-GPU parity: binary load identity", "[cpu_gpu_parity]") {
    std::vector<std::pair<uint8_t, float>> mods = {
        {7,   0.2f},   // NaK_Pump
        {107, 0.03f},  // K2P_LEAK
        {105, 0.01f},  // NaP
        {104, 0.03f},  // BK
    };
    std::string path = make_bion_file("bincheck", mods, -65.0f);

    // Загружаем на CPU
    BionData cpu_data;
    memset(&cpu_data, 0, sizeof(cpu_data));
    REQUIRE(bion_load_host(path.c_str(), &cpu_data) == 0);
    REQUIRE(cpu_data.num_segments == 1);
    Segment* cpu_seg = &cpu_data.segments[0];

    // Загружаем на GPU
    sycl::device dev(sycl::default_selector_v);
    Simulation gpu_sim(dev);
    REQUIRE(gpu_sim.load_bion(path.c_str()) == 0);
    Segment* gpu_seg = gpu_sim.get_bion_data()->segments;
    REQUIRE(gpu_seg != nullptr);

    // Сравниваем критичные поля
    CHECK(cpu_seg->id == gpu_seg->id);
    CHECK(cpu_seg->neuron_id == gpu_seg->neuron_id);
    CHECK(cpu_seg->segment_type == gpu_seg->segment_type);
    CHECK(cpu_seg->V_m == Approx(-65.0f));
    CHECK(gpu_seg->V_m == Approx(-65.0f));
    CHECK(cpu_seg->radius == Approx(5.0f));
    CHECK(gpu_seg->radius == Approx(5.0f));
    CHECK(cpu_seg->length == Approx(10.0f));
    CHECK(gpu_seg->length == Approx(10.0f));
    CHECK(cpu_seg->modifier_count == gpu_seg->modifier_count);
    CHECK(cpu_seg->modifier_count == 4);

    // Сравниваем ионные концентрации
    for (int i = 0; i < 4; i++) {
        CHECK(cpu_seg->intra_ions[i] == Approx(gpu_seg->intra_ions[i]));
        CHECK(cpu_seg->extra_ions[i] == Approx(gpu_seg->extra_ions[i]));
    }

    // Сравниваем модификаторы
    for (uint8_t j = 0; j < cpu_seg->modifier_count; j++) {
        INFO("Modifier " << (int)j);
        CHECK(cpu_seg->modifiers[j].type == gpu_seg->modifiers[j].type);
        CHECK(cpu_seg->modifiers[j].density == Approx(gpu_seg->modifiers[j].density));
    }

    printf("[PARITY-BIN] All fields match: CPU↔GPU binary load identity verified\n");
    bion_unload_host(&cpu_data);
    std::remove(path.c_str());
}

// ── Полный набор покоя (K2P + Pump + KIR) ──
TEST_CASE("CPU-GPU parity: full resting set", "[cpu_gpu_parity]") {
    std::vector<std::pair<uint8_t, float>> mods = {
        {107, 0.03f},  // K2P_LEAK
        {7,   0.2f},   // NaK_Pump
        {106, 0.008f}, // KIR
    };
    std::string path = make_bion_file("full_rest", mods, -65.0f);

    SimResult cpu_r = run_cpu(path, 100);
    SimResult gpu_r = run_gpu(path, 100);

    printf("[PARITY-FULL] CPU: avg=%.2f [%.2f..%.2f] I_buf=%.6f\n",
           cpu_r.Vm_avg, cpu_r.Vm_min, cpu_r.Vm_max, cpu_r.I_buf);
    printf("[PARITY-FULL] GPU: avg=%.2f [%.2f..%.2f] I_buf=%.6f\n",
           gpu_r.Vm_avg, gpu_r.Vm_min, gpu_r.Vm_max, gpu_r.I_buf);

    float delta = fabsf(cpu_r.Vm_avg - gpu_r.Vm_avg);
    printf("[PARITY-FULL] delta=%.2f mV\n", delta);
    CHECK(delta < 5.0f);
    std::remove(path.c_str());
}
