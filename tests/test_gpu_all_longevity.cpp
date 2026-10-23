// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_gpu_all_longevity.cpp — автономный тест GPU_ALL на 75000 тиков
// Загружает реальный .bion, гонит на всех GPU, ловит NaN и дампит состояние.
// Запуск: ./test_gpu_all_longevity [network.bion]
// Без аргументов — создаёт тестовую сеть (сома+дендрит+аксон, 3 сегмента, синапс).

#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <dlfcn.h>
#include <vector>
#include <thread>
#include <algorithm>

#include "types.h"
#include "bion_format.h"
#include "bion_loader.h"

extern "C" {
    void core_run_tick(Segment*, uint32_t, IntraConnection*, uint32_t,
                       Synapse*, uint32_t, int, uint64_t*, float,
                       void*,void*,void*,void*,void*,void*,void*,void*,void*,void*, MechanoStimulus* mechano, uint32_t* mechano_count);
}

struct GpuSlot {
    void* handle;
    void* sim;
    const char* name;
    void* (*create)();  void (*destroy)(void*);
    int (*load_bion)(void*, const char*);
    void (*run_tick)(void*, float);
    void* (*bion_data)(void*);
};

static bool load_backend(GpuSlot& s, const char* so) {
    s.handle = dlopen(so, RTLD_NOW);
    if (!s.handle) { printf("[%s] dlopen FAILED: %s\n", so, dlerror()); return false; }
    s.create    = (void*(*)())dlsym(s.handle, "gpu_backend_create");
    s.destroy   = (void(*)(void*))dlsym(s.handle, "gpu_backend_destroy");
    s.load_bion = (int(*)(void*,const char*))dlsym(s.handle, "gpu_backend_load_bion");
    s.run_tick  = (void(*)(void*,float))dlsym(s.handle, "gpu_backend_run_tick");
    s.bion_data = (void*(*)(void*))dlsym(s.handle, "gpu_backend_bion_data");
    if (!s.create || !s.load_bion || !s.run_tick || !s.bion_data) {
        printf("[%s] missing symbols\n", so); dlclose(s.handle); s.handle = nullptr; return false;
    }
    s.name = so;
    return true;
}

// Чтение float через сырой offset — обходит возможные проблемы с ABI/padding
static float raw_read_float(const Segment* seg, int byte_offset) {
    float val;
    memcpy(&val, (const char*)seg + byte_offset, 4);
    return val;
}

static void dump_segment(Segment* seg, uint32_t idx, const char* label) {
    printf("[%s DUMP seg=%u]\n", label, idx);
    printf("  V_m=%.4f I_buf=%.6f\n", raw_read_float(seg, 40), raw_read_float(seg, 912));
    printf("  pos x=%.1f y=%.1f z=%.1f r=%.1f len=%.1f\n",
           raw_read_float(seg, 8), raw_read_float(seg, 12), raw_read_float(seg, 16),
           raw_read_float(seg, 36), raw_read_float(seg, 32));
    printf("  intra: Na=%.6f K=%.6f Ca=%.6f Cl=%.6f Mg=%.6f\n",
           raw_read_float(seg, 44), raw_read_float(seg, 48), raw_read_float(seg, 52),
           raw_read_float(seg, 56), raw_read_float(seg, 60));
    printf("  extra: Na=%.6f K=%.6f Ca=%.6f Cl=%.6f Mg=%.6f\n",
           raw_read_float(seg, 64), raw_read_float(seg, 68), raw_read_float(seg, 72),
           raw_read_float(seg, 76), raw_read_float(seg, 80));
    printf("  ATP=%.4f temp=%.2f txn=%.6f\n",
           raw_read_float(seg, 904), raw_read_float(seg, 908), raw_read_float(seg, 916));
    // modifier_count read via raw offset too
    uint8_t mc = *(const uint8_t*)((const char*)seg + 900);
    for (int j = 0; j < mc && j < 16; j++) {
        const char* mod_base = (const char*)seg + 196 + j * 44;
        uint8_t mtype = *(const uint8_t*)(mod_base);  // type at offset 0 of Modifier
        float mdens = raw_read_float((Segment*)mod_base, 4);  // density at offset 4
        printf("  mod[%d] type=%d density=%.3f state: ", j, mtype, mdens);
        for (int k = 0; k < 8; k++) printf("%.4f ", raw_read_float((Segment*)mod_base, 8 + k*4));
        printf("\n");
    }
}

static bool has_nan(Segment* segs, uint32_t N) {
    for (uint32_t i = 0; i < N; i++) {
        if (std::isnan(raw_read_float(&segs[i], 40))) return true;   // V_m
        if (std::isnan(raw_read_float(&segs[i], 912))) return true;  // I_buffer
        for (int j = 0; j < 5; j++) {  // ION_COUNT=5
            if (std::isnan(raw_read_float(&segs[i], 44 + 4*j))) return true;  // intra_ions
            if (std::isnan(raw_read_float(&segs[i], 64 + 4*j))) return true;  // extra_ions
        }
    }
    return false;
}

static void print_stats(Segment* segs, uint32_t N, uint64_t tick, const char* label) {
    float v_min = 999, v_max = -999, v_sum = 0;
    int nan_count = 0;
    for (uint32_t i = 0; i < N; i++) {
        float v = raw_read_float(&segs[i], 40);  // offsetof(V_m) = 40
        if (std::isnan(v)) { nan_count++; continue; }
        v_sum += v;
        if (v < v_min) v_min = v;
        if (v > v_max) v_max = v;
    }
    // intra_ions[0..4] на offset 44, extra_ions[0..4] на offset 64
    float na_i = raw_read_float(&segs[0], 44);   // intra_ions[0] = Na+
    float k_i  = raw_read_float(&segs[0], 48);   // intra_ions[1] = K+
    float k_o  = raw_read_float(&segs[0], 68);   // extra_ions[1] = K+
    printf("[%s tick=%llu] V_m: avg=%.2f [%.2f..%.2f] NaN=%d Na_i=%.4f K_i=%.4f K_o=%.4f\n",
           label, (unsigned long long)tick, v_sum / (N - nan_count), v_min, v_max, nan_count,
           na_i, k_i, k_o);
}

static std::string make_test_network(const char* path) {
    NetworkHeader h = {}; h.magic = BION_MAGIC; h.version = BION_VERSION;
    h.num_neurons = 3; h.num_segments = 3; h.num_intra_connections = 2; h.num_synapses = 1;

    Neuron n0 = {}; n0.segment_start = 0; n0.segment_count = 1; n0.threshold = -20;
    Neuron n1 = {}; n1.id = 1; n1.segment_start = 1; n1.segment_count = 1; n1.threshold = -20;
    Neuron n2 = {}; n2.id = 2; n2.segment_start = 2; n2.segment_count = 1; n2.threshold = -20;

    auto make_seg = [](Segment& s, uint32_t id, uint32_t nid, uint8_t stype, float r, float len,
                        float x, float y, float Vm) {
        s.id = id; s.neuron_id = nid; s.segment_type = stype;
        s.radius = r; s.length = len; s.V_m = Vm;
        s.position = {x, y, 0}; s.direction = {0, 1, 0};
        s.intra_ions[0]=0.015f; s.intra_ions[1]=0.140f; s.intra_ions[2]=0.00005f;
        s.intra_ions[3]=0.005f; s.intra_ions[4]=0.0005f;
        s.extra_ions[0]=0.140f; s.extra_ions[1]=0.005f; s.extra_ions[2]=0.0012f;
        s.extra_ions[3]=0.130f; s.extra_ions[4]=0.002f;
        s.modifier_count = 6;
        s.modifiers[0].type = MOD_AXON_NAV12;     s.modifiers[0].density = 15.6f;  // Nav1.2 (Na)
        s.modifiers[1].type = MOD_KV21;           s.modifiers[1].density = 3.84f;  // Kv1.1 (K DR)
        s.modifiers[2].type = MOD_PUMP_NAK;       s.modifiers[2].density = 0.2f;   // Na/K pump
        s.modifiers[3].type = MOD_K2P_LEAK;       s.modifiers[3].density = 0.03f;  // K2P leak
        s.modifiers[4].type = MOD_GLIAL_KIR41;    s.modifiers[4].density = 0.3f;   // Glial Kir4.1 (K+ uptake)
        s.modifiers[5].type = MOD_GLIAL_NAK_PUMP; s.modifiers[5].density = 0.2f;   // Glial Na/K-ATPase α2 (K+ return)
    };

    Segment s0, s1, s2;
    make_seg(s0, 0, 0, 1, 5.0f, 10.0f, 0, 50, -65.0f);    // SOMA
    make_seg(s1, 1, 1, 0, 3.0f, 20.0f, 0, 60, -65.0f);    // Dendrite
    make_seg(s2, 2, 2, 0, 1.5f, 30.0f, 0, 70, -55.0f);    // Axon (depolarized → spiking)
    s2.modifiers[4].density = 0.5f;  // more Kir4.1 on axon
    s2.modifiers[5].density = 0.3f;  // more Na/K-ATPase on axon

    IntraConnection c0 = {}; c0.segment_a = 0; c0.segment_b = 1; c0.is_intracellular = 1;
    IntraConnection c1 = {}; c1.segment_a = 1; c1.segment_b = 2; c1.is_intracellular = 1;

    Synapse syn = {}; syn.id = 0; syn.pre_segment = 2; syn.post_segment = 0;
    syn.type = SYNAPSE_TYPE_CHEMICAL; syn.weight = 0.3f;
    syn.release_probability = 0.3f; syn.release_amount = 1.0f;
    syn.tau_clearance = 3.0f; syn.pre_area = 0.1f; syn.post_area = 0.1f;
    syn.post_receptor_count = 1;
    syn.post_receptors[0].type = MOD_NT_RECEPTOR_AMPA; syn.post_receptors[0].density = 0.1f;

    FILE* f = fopen(path, "wb");
    fwrite(&h, sizeof(h), 1, f);
    fwrite(&n0, sizeof(n0), 1, f); fwrite(&n1, sizeof(n1), 1, f); fwrite(&n2, sizeof(n2), 1, f);
    fwrite(&s0, sizeof(s0), 1, f); fwrite(&s1, sizeof(s1), 1, f); fwrite(&s2, sizeof(s2), 1, f);
    fwrite(&c0, sizeof(c0), 1, f); fwrite(&c1, sizeof(c1), 1, f);
    fwrite(&syn, sizeof(syn), 1, f);
    fclose(f);
    return path;
}

TEST_CASE("GPU_ALL longevity: 75000 ticks NaN-free", "[gpu_all_longevity]") {
    const int MAX_TICKS = 75000;
    const int CHECK_EVERY = 1000;

    // Загружаем бэкенды
    std::vector<GpuSlot> slots;
    const char* so_paths[] = {
        "./libbiocore_gpu_backend_cuda.so",
        "../BioNeuronCore/build/libbiocore_gpu_backend_cuda.so",
        "libbiocore_gpu_backend_cuda.so",
        nullptr
    };

    GpuSlot cuda_slot = {};
    bool has_cuda = false;
    for (int i = 0; so_paths[i]; i++) {
        if (load_backend(cuda_slot, so_paths[i])) { has_cuda = true; cuda_slot.name = "CUDA"; break; }
    }

    GpuSlot intel_slot = {};
    bool has_intel = false;
    const char* intel_paths[] = {
        "./libbiocore_gpu_backend_intel.so",
        "../BioNeuronCore/build/libbiocore_gpu_backend_intel.so",
        nullptr
    };
    for (int i = 0; intel_paths[i]; i++) {
        if (load_backend(intel_slot, intel_paths[i])) {
            // Проверим работоспособность Intel
            void* test_sim = intel_slot.create();
            if (test_sim) { has_intel = true; intel_slot.destroy(test_sim); intel_slot.name = "Intel"; break; }
            dlclose(intel_slot.handle); intel_slot.handle = nullptr;
        }
    }

    if (!has_cuda && !has_intel) {
        WARN("No GPU backends available — skipping GPU_ALL longevity test");
        return;
    }

    printf("\n=== GPU_ALL Longevity Test: %d ticks ===\n", MAX_TICKS);
    printf("Backends: CUDA=%s Intel=%s\n", has_cuda ? "YES" : "NO", has_intel ? "YES" : "NO");

    // Создаём тестовую сеть
    std::string bion_path = make_test_network("/tmp/gpu_all_longevity_test.bion");
    printf("Test network: %s (3 seg, 2 conn, 1 syn, Glial Kir4.1 on all)\n", bion_path.c_str());

    // Создаём слоты для GPU_ALL
    struct Slot { GpuSlot* bk; void* sim; BionData* data; };
    std::vector<Slot> all_slots;

    if (has_cuda) {
        void* sim = cuda_slot.create();
        REQUIRE(sim != nullptr);
        REQUIRE(cuda_slot.load_bion(sim, bion_path.c_str()) == 0);
        all_slots.push_back({&cuda_slot, sim, (BionData*)cuda_slot.bion_data(sim)});
        printf("[CUDA] loaded: %u segs\n", all_slots.back().data->num_segments);
    }
    if (has_intel) {
        void* sim = intel_slot.create();
        REQUIRE(sim != nullptr);
        int ret = intel_slot.load_bion(sim, bion_path.c_str());
        if (ret == 0) {
            all_slots.push_back({&intel_slot, sim, (BionData*)intel_slot.bion_data(sim)});
            printf("[Intel] loaded: %u segs\n", all_slots.back().data->num_segments);
        } else {
            printf("[Intel] load_bion FAILED: %d — skipping\n", ret);
            intel_slot.destroy(sim);
        }
    }

    REQUIRE(!all_slots.empty());
    printf("GPU_ALL: %zu slots active\n", all_slots.size());

    // ── Основной цикл: 75000 тиков ──
    bool nan_found = false;
    for (int tick = 0; tick < MAX_TICKS && !nan_found; tick++) {
        // Параллельный запуск (как в simulation_bridge)
        std::vector<std::thread> threads;
        for (size_t i = 0; i < all_slots.size(); i++) {
            threads.emplace_back([&all_slots, i]() {
                all_slots[i].bk->run_tick(all_slots[i].sim, 0.1f);
            });
        }
        for (auto& t : threads) t.join();

        // Проверка каждые CHECK_EVERY тиков
        if (tick % CHECK_EVERY == 0) {
            for (size_t si = 0; si < all_slots.size(); si++) {
                auto& sl = all_slots[si];
                if (has_nan(sl.data->segments, sl.data->num_segments)) {
                    printf("\n!!! NaN DETECTED at tick=%d on slot %zu (%s) !!!\n", tick, si, sl.bk->name);
                    for (uint32_t i = 0; i < sl.data->num_segments; i++) {
                        if (std::isnan(sl.data->segments[i].V_m) ||
                            std::isnan(sl.data->segments[i].I_buffer) ||
                            std::isnan(sl.data->segments[i].intra_ions[0]))
                            dump_segment(&sl.data->segments[i], i, sl.bk->name);
                    }
                    // Дамп всех слотов для сравнения
                    for (size_t sj = 0; sj < all_slots.size(); sj++) {
                        printf("\n--- Full state slot %zu (%s) at tick %d ---\n", sj, all_slots[sj].bk->name, tick);
                        BionData* d = all_slots[sj].data;
                        for (uint32_t i = 0; i < d->num_segments; i++)
                            print_stats(d->segments, d->num_segments, tick,
                                (std::string(all_slots[sj].bk->name) + " seg" + std::to_string(i)).c_str());
                    }
                    nan_found = true;
                    break;
                }
            }
            // Краткий статус
            if (!nan_found && (tick % 10000 == 0)) {
                for (size_t si = 0; si < all_slots.size(); si++)
                    print_stats(all_slots[si].data->segments, all_slots[si].data->num_segments,
                                tick, all_slots[si].bk->name);
            }
        }
    }

    // ── Финальный статус ──
    printf("\n=== Final state after %d ticks ===\n", nan_found ? -1 : MAX_TICKS);
    for (size_t si = 0; si < all_slots.size(); si++) {
        print_stats(all_slots[si].data->segments, all_slots[si].data->num_segments,
                    nan_found ? 0 : (uint64_t)MAX_TICKS, all_slots[si].bk->name);
    }

    CHECK_FALSE(nan_found);

    // Очистка
    for (auto& sl : all_slots) { sl.bk->destroy(sl.sim); }
    if (has_cuda)  dlclose(cuda_slot.handle);
    if (has_intel) dlclose(intel_slot.handle);
    std::remove(bion_path.c_str());
}

int main(int argc, char** argv) {
    return Catch::Session().run(argc, argv);
}
