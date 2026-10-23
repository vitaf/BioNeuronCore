// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_glial_parity.cpp — CPU vs GPU сравнение для Kir4.1 + NaK-ATPase.
// Проверяет K⁺-буферинг и стабилизацию K⁺_o.
// GPU тестируется через dlopen CUDA backend (не требует SYCL-хидеров при сборке GCC).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <dlfcn.h>

#include "types.h"
#include "bion_format.h"
#include "bion_loader.h"

extern "C" {
    void core_run_tick(Segment* segs, uint32_t N, IntraConnection* conns, uint32_t M,
                       Synapse* syns, uint32_t K, int mode, uint64_t* tick, float dt,
                       void*,void*,void*,void*,void*,void*,void*,void*,void*,void*, MechanoStimulus* mechano, uint32_t* mechano_count);
}

struct CudaBackend {
    void* handle; void* sim;
    void* (*create)(); void (*destroy)(void*);
    int (*load_bion)(void*, const char*);
    void (*run_tick)(void*, float);
    void* (*bion_data)(void*);
    bool load() {
        const char* paths[] = {"./libbiocore_gpu_backend_cuda.so",
                               "../BioNeuronCore/build/libbiocore_gpu_backend_cuda.so",
                               "libbiocore_gpu_backend_cuda.so"};
        for (int i = 0; i < 3; i++) { handle = dlopen(paths[i], RTLD_NOW); if (handle) break; }
        if (!handle) return false;
        create = (void*(*)())dlsym(handle, "gpu_backend_create");
        destroy = (void(*)(void*))dlsym(handle, "gpu_backend_destroy");
        load_bion = (int(*)(void*,const char*))dlsym(handle, "gpu_backend_load_bion");
        run_tick = (void(*)(void*,float))dlsym(handle, "gpu_backend_run_tick");
        bion_data = (void*(*)(void*))dlsym(handle, "gpu_backend_bion_data");
        return create && load_bion && run_tick && bion_data;
    }
    void unload() { if (handle) dlclose(handle); }
};

// ── Создать .bion с одним глиальным сегментом (Kir4.1 + NaK-pump) ──
static std::string make_glial_bion(const char* tag, float extra_K = 0.010f) {
    char path[256]; snprintf(path, sizeof(path), "/tmp/glial_%s.bion", tag);
    NetworkHeader h = {}; h.magic = BION_MAGIC; h.version = BION_VERSION;
    h.num_neurons = 1; h.num_segments = 1;
    h.num_intra_connections = 0; h.num_synapses = 0;

    FILE* f = fopen(path, "wb"); REQUIRE(f);
    fwrite(&h, sizeof(h), 1, f);

    Neuron n = {}; n.id = 0; n.segment_count = 1; n.threshold = -20;
    fwrite(&n, sizeof(n), 1, f);

    Segment s = {}; s.id = 0; s.neuron_id = 0; s.segment_type = 1;
    s.position = {0, 50, 0}; s.direction = {0, 1, 0};
    s.radius = 3.0f; s.length = 6.0f; s.V_m = -80.0f;
    s.intra_ions[0]=0.015f; s.intra_ions[1]=0.140f; s.intra_ions[2]=0.00005f; s.intra_ions[3]=0.005f;
    s.extra_ions[0]=0.140f; s.extra_ions[1]=extra_K; s.extra_ions[2]=0.0012f; s.extra_ions[3]=0.130f;
    s.modifier_count = 3;
    s.modifiers[0].type = MOD_GLIAL_KIR41;    s.modifiers[0].density = 0.4f;
    s.modifiers[1].type = MOD_GLIAL_NAK_PUMP; s.modifiers[1].density = 0.25f;
    s.modifiers[2].type = MOD_K2P_LEAK;       s.modifiers[2].density = 0.02f;
    fwrite(&s, sizeof(s), 1, f);
    fclose(f);
    return path;
}

struct GlialResult { float Vm_avg, Vm_final, Ko_initial, Ko_final; };

// ── CPU ──
static GlialResult run_glial_cpu(const std::string& path, int ticks) {
    BionData d; memset(&d, 0, sizeof(d));
    REQUIRE(bion_load_host(path.c_str(), &d) == 0);

    float Ko_init = d.segments[0].extra_ions[ION_K];
    uint64_t tc = 0;
    float Vm_sum = 0; int samples = 0;
    for (int t = 0; t < ticks; t++) {
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr, nullptr, nullptr);
        Vm_sum += d.segments[0].V_m; samples++;
    }
    GlialResult r = {Vm_sum/samples, d.segments[0].V_m, Ko_init, d.segments[0].extra_ions[ION_K]};
    bion_unload_host(&d);
    return r;
}

// ── GPU через CUDA dlopen ──
static GlialResult run_glial_gpu_cuda(CudaBackend& bk, const std::string& path, int ticks) {
    bk.sim = bk.create();
    REQUIRE(bk.sim);
    REQUIRE(bk.load_bion(bk.sim, path.c_str()) == 0);
    BionData* bd = (BionData*)bk.bion_data(bk.sim);
    REQUIRE(bd != nullptr);

    float Ko_init = bd->segments[0].extra_ions[ION_K];
    float Vm_sum = 0; int samples = 0;
    for (int t = 0; t < ticks; t++) {
        bk.run_tick(bk.sim, 0.1f);
        Vm_sum += bd->segments[0].V_m; samples++;
    }
    GlialResult r = {Vm_sum/samples, bd->segments[0].V_m, Ko_init, bd->segments[0].extra_ions[ION_K]};
    bk.destroy(bk.sim); bk.sim = nullptr;
    return r;
}

TEST_CASE("Glial GPU-fallback: CPU vs GPU parity (Kir4.1 + NaK-ATPase)", "[glial]") {
    CudaBackend bk;
    if (!bk.load()) { WARN("CUDA not available — skipping GPU parity"); }

    std::string path = make_glial_bion("parity", 0.010f);
    int ticks = 100;

    GlialResult cpu = run_glial_cpu(path, ticks);
    printf("[GLIAL-PARITY] CPU: Vm_avg=%.2f Vm_final=%.2f Ko: %.6f → %.6f\n",
           cpu.Vm_avg, cpu.Vm_final, cpu.Ko_initial, cpu.Ko_final);

    if (bk.handle) {
        GlialResult gpu = run_glial_gpu_cuda(bk, path, ticks);
        printf("[GLIAL-PARITY] GPU: Vm_avg=%.2f Vm_final=%.2f Ko: %.6f → %.6f\n",
               gpu.Vm_avg, gpu.Vm_final, gpu.Ko_initial, gpu.Ko_final);

        float delta_Vm = fabsf(cpu.Vm_avg - gpu.Vm_avg);
        printf("[GLIAL-PARITY] delta_Vm=%.4f mV\n", delta_Vm);

        REQUIRE_FALSE(std::isnan(gpu.Vm_avg));
        CHECK(delta_Vm < 5.0f);
    }

    REQUIRE_FALSE(std::isnan(cpu.Vm_avg));
    std::remove(path.c_str()); bk.unload();
}

TEST_CASE("Glial K+ buffering: K+_o stabilizes, does not grow indefinitely", "[glial]") {
    std::string path = make_glial_bion("kbuf", 0.020f);

    BionData d; memset(&d, 0, sizeof(d));
    REQUIRE(bion_load_host(path.c_str(), &d) == 0);

    float Ko_start = d.segments[0].extra_ions[ION_K];
    float Ko_max = Ko_start;
    uint64_t tc = 0;

    for (int t = 0; t < 500; t++) {
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr, nullptr, nullptr);
        float ko = d.segments[0].extra_ions[ION_K];
        if (ko > Ko_max) Ko_max = ko;
        CHECK_FALSE(std::isnan(ko));
    }

    float Ko_end = d.segments[0].extra_ions[ION_K];
    printf("[GLIAL-KBUF] K+_o: %.6f → %.6f (max=%.6f) after 500 ticks\n",
           Ko_start, Ko_end, Ko_max);

    CHECK(Ko_end < Ko_start * 5.0f);
    CHECK(Ko_end >= 1e-12f);

    bion_unload_host(&d); std::remove(path.c_str());
}

TEST_CASE("Glial: V_m stays near resting (-80mV) on CPU", "[glial]") {
    std::string path = make_glial_bion("rest", 0.005f);
    BionData d; memset(&d, 0, sizeof(d));
    REQUIRE(bion_load_host(path.c_str(), &d) == 0);

    CHECK(d.segments[0].V_m == Approx(-80.0f));

    uint64_t tc = 0;
    for (int t = 0; t < 200; t++) {
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr, nullptr, nullptr);
    }

    float Vm = d.segments[0].V_m;
    printf("[GLIAL-REST] V_m after 200 ticks: %.2f mV (started at -80mV)\n", Vm);
    CHECK(Vm > -100.0f);
    CHECK(Vm < -40.0f);

    bion_unload_host(&d); std::remove(path.c_str());
}

TEST_CASE("Glial: K+ buffering on CUDA GPU", "[glial][gpu]") {
    CudaBackend bk;
    if (!bk.load()) { WARN("CUDA not available"); return; }

    std::string path = make_glial_bion("cuda_kbuf", 0.020f);

    bk.sim = bk.create(); REQUIRE(bk.sim);
    REQUIRE(bk.load_bion(bk.sim, path.c_str()) == 0);
    BionData* bd = (BionData*)bk.bion_data(bk.sim); REQUIRE(bd);

    float Ko_start = bd->segments[0].extra_ions[ION_K];
    float Ko_max = Ko_start;

    for (int t = 0; t < 500; t++) {
        bk.run_tick(bk.sim, 0.1f);
        float ko = bd->segments[0].extra_ions[ION_K];
        if (ko > Ko_max) Ko_max = ko;
        CHECK_FALSE(std::isnan(ko));
        CHECK_FALSE(std::isnan(bd->segments[0].V_m));
    }

    float Ko_end = bd->segments[0].extra_ions[ION_K];
    printf("[GLIAL-CUDA] K+_o: %.6f → %.6f (max=%.6f) Vm=%.2f after 500 ticks\n",
           Ko_start, Ko_end, Ko_max, bd->segments[0].V_m);

    CHECK(Ko_end < Ko_start * 5.0f);
    CHECK(Ko_end >= 1e-12f);

    bk.destroy(bk.sim); std::remove(path.c_str()); bk.unload();
}

// ── .bion: pre→post синапс + глиальная клетка (GLAST) для посегментного клиренса NT ──
static std::string make_glial_nt_bion() {
    const char* path = "/tmp/glial_nt_clearance.bion";
    NetworkHeader h = {}; h.magic = BION_MAGIC; h.version = BION_VERSION;
    h.num_neurons = 3; h.num_segments = 3;
    h.num_intra_connections = 0; h.num_synapses = 1;

    Neuron n0 = {}; n0.id = 0; n0.segment_start = 0; n0.segment_count = 1; n0.threshold = -20;
    Neuron n1 = {}; n1.id = 1; n1.segment_start = 1; n1.segment_count = 1; n1.threshold = -20;
    Neuron n2 = {}; n2.id = 2; n2.segment_start = 2; n2.segment_count = 1; n2.threshold = -20;

    // pre
    Segment s0 = {}; s0.id = 0; s0.neuron_id = 0; s0.segment_type = 1;
    s0.position = {0, 50, 0}; s0.direction = {0, 1, 0};
    s0.radius = 5.0f; s0.length = 10.0f; s0.V_m = -65.0f;
    s0.intra_ions[0]=0.015f; s0.intra_ions[1]=0.140f; s0.intra_ions[2]=0.00005f; s0.intra_ions[3]=0.005f;
    s0.extra_ions[0]=0.140f; s0.extra_ions[1]=0.005f; s0.extra_ions[2]=0.0012f; s0.extra_ions[3]=0.130f;
    s0.modifier_count = 2; s0.modifiers[0].type = 107; s0.modifiers[0].density = 0.03f;  // K2P (тип 0 удалён в #69)
    s0.modifiers[1].type = 7; s0.modifiers[1].density = 0.2f;

    // post
    Segment s1 = {}; s1.id = 1; s1.neuron_id = 1; s1.segment_type = 1;
    s1.position = {0, 60, 0}; s1.direction = {0, 1, 0};
    s1.radius = 3.0f; s1.length = 20.0f; s1.V_m = -65.0f;
    s1.intra_ions[0]=0.015f; s1.intra_ions[1]=0.140f; s1.intra_ions[2]=0.00005f; s1.intra_ions[3]=0.005f;
    s1.extra_ions[0]=0.140f; s1.extra_ions[1]=0.005f; s1.extra_ions[2]=0.0012f; s1.extra_ions[3]=0.130f;
    s1.modifier_count = 2; s1.modifiers[0].type = 107; s1.modifiers[0].density = 0.03f;  // K2P (тип 0 удалён в #69)
    s1.modifiers[1].type = 7; s1.modifiers[1].density = 0.2f;

    // glial (GLAST) — та же position.z, что у post (60)
    Segment s2 = {}; s2.id = 2; s2.neuron_id = 2; s2.segment_type = 1;
    s2.position = {0, 60, 0}; s2.direction = {0, 1, 0};
    s2.radius = 3.0f; s2.length = 6.0f; s2.V_m = -80.0f;
    s2.intra_ions[0]=0.015f; s2.intra_ions[1]=0.140f; s2.intra_ions[2]=0.00005f; s2.intra_ions[3]=0.005f;
    s2.extra_ions[0]=0.140f; s2.extra_ions[1]=0.005f; s2.extra_ions[2]=0.0012f; s2.extra_ions[3]=0.130f;
    s2.modifier_count = 1; s2.modifiers[0].type = MOD_GLAST; s2.modifiers[0].density = 0.1f;

    // Синапс pre→post: начальный глутамат в щели (без нового выброса) — тест роутинга.
    Synapse syn = {};
    syn.id = 0; syn.pre_segment = 0; syn.post_segment = 1;
    syn.type = SYNAPSE_TYPE_CHEMICAL;
    syn.release_probability = 0.0f;
    syn.release_amount = 1.0f;
    syn.tau_clearance = 5.0f;
    syn.cleft_neurotransmitter[NT_GLUTAMATE] = 1.0f;

    FILE* f = fopen(path, "wb"); REQUIRE(f);
    fwrite(&h, sizeof(h), 1, f);
    fwrite(&n0, sizeof(n0), 1, f); fwrite(&n1, sizeof(n1), 1, f); fwrite(&n2, sizeof(n2), 1, f);
    fwrite(&s0, sizeof(s0), 1, f); fwrite(&s1, sizeof(s1), 1, f); fwrite(&s2, sizeof(s2), 1, f);
    fwrite(&syn, sizeof(syn), 1, f);
    fclose(f);
    return path;
}

TEST_CASE("Glial NT clearance: CPU vs CUDA parity (GLAST routing)", "[glial][gpu]") {
    std::string path = make_glial_nt_bion();

    // ── CPU ──
    float cpu_glial = -1, cpu_cleft = -1, cpu_post = -1;
    {
        BionData d; memset(&d, 0, sizeof(d));
        REQUIRE(bion_load_host(path.c_str(), &d) == 0);
        REQUIRE(d.num_segments == 3);
        REQUIRE(d.num_synapses == 1);
        uint64_t tc = 0;
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr, nullptr, nullptr);
        cpu_glial = d.segments[2].neurotransmitter[NT_GLUTAMATE];
        cpu_cleft = d.synapses[0].cleft_neurotransmitter[NT_GLUTAMATE];
        cpu_post  = d.segments[1].neurotransmitter[NT_GLUTAMATE];
        bion_unload_host(&d);
    }

    // ── CUDA ──
    CudaBackend bk;
    if (!bk.load()) { WARN("CUDA not available"); std::remove(path.c_str()); return; }
    float gpu_glial = -1, gpu_cleft = -1, gpu_post = -1;
    {
        bk.sim = bk.create(); REQUIRE(bk.sim);
        REQUIRE(bk.load_bion(bk.sim, path.c_str()) == 0);
        BionData* bd = (BionData*)bk.bion_data(bk.sim); REQUIRE(bd);
        bk.run_tick(bk.sim, 0.1f);
        gpu_glial = bd->segments[2].neurotransmitter[NT_GLUTAMATE];
        gpu_cleft = bd->synapses[0].cleft_neurotransmitter[NT_GLUTAMATE];
        gpu_post  = bd->segments[1].neurotransmitter[NT_GLUTAMATE];
        bk.destroy(bk.sim); bk.sim = nullptr;
    }

    printf("[GLIAL-NT] CPU: glial_glu=%.4f cleft=%.4f post=%.4f\n", cpu_glial, cpu_cleft, cpu_post);
    printf("[GLIAL-NT] GPU: glial_glu=%.4f cleft=%.4f post=%.4f\n", gpu_glial, gpu_cleft, gpu_post);

    // Роутинг сработал: глутамат ушёл из щели в глию (не через tau_clearance)
    CHECK(cpu_glial > 0.5f);
    CHECK(cpu_cleft < 0.01f);
    CHECK(cpu_post  > 0.5f);
    CHECK(gpu_glial > 0.5f);
    CHECK(gpu_cleft < 0.01f);
    CHECK(gpu_post  > 0.5f);

    // Паритет CPU/GPU
    CHECK(fabsf(cpu_glial - gpu_glial) < 0.01f);
    CHECK(fabsf(cpu_cleft - gpu_cleft) < 0.01f);
    CHECK(fabsf(cpu_post  - gpu_post)  < 0.01f);

    std::remove(path.c_str()); bk.unload();
}

TEST_CASE("Glial NT clearance: CPU vs Intel SYCL parity (GLAST routing)", "[glial][sycl]") {
    // dlopen Intel SYCL backend (libsycl требует oneAPI env при запуске)
    const char* paths[] = {"./libbiocore_gpu_backend_intel.so",
                           "../BioNeuronCore/build/libbiocore_gpu_backend_intel.so",
                           "libbiocore_gpu_backend_intel.so"};
    void* handle = nullptr;
    for (int i = 0; i < 3; i++) { handle = dlopen(paths[i], RTLD_NOW); if (handle) break; }
    if (!handle) { WARN("Intel SYCL backend not available — skipping"); return; }

    auto create     = (void*(*)())dlsym(handle, "gpu_backend_create");
    auto destroy    = (void(*)(void*))dlsym(handle, "gpu_backend_destroy");
    auto load_bion  = (int(*)(void*,const char*))dlsym(handle, "gpu_backend_load_bion");
    auto run_tick   = (void(*)(void*,float))dlsym(handle, "gpu_backend_run_tick");
    auto bion_data  = (void*(*)(void*))dlsym(handle, "gpu_backend_bion_data");
    bool all_symbols = create && destroy && load_bion && run_tick && bion_data;
    REQUIRE(all_symbols);

    std::string path = make_glial_nt_bion();
    void* sim = create();
    if (!sim) { WARN("Intel SYCL device not available — skipping"); dlclose(handle); std::remove(path.c_str()); return; }
    REQUIRE(load_bion(sim, path.c_str()) == 0);
    BionData* bd = (BionData*)bion_data(sim); REQUIRE(bd);

    run_tick(sim, 0.1f);
    float gpu_glial = bd->segments[2].neurotransmitter[NT_GLUTAMATE];
    float gpu_cleft = bd->synapses[0].cleft_neurotransmitter[NT_GLUTAMATE];
    float gpu_post  = bd->segments[1].neurotransmitter[NT_GLUTAMATE];
    printf("[GLIAL-NT-INTEL] glial_glu=%.4f cleft=%.4f post=%.4f\n", gpu_glial, gpu_cleft, gpu_post);

    CHECK(gpu_glial > 0.5f);
    CHECK(gpu_cleft < 0.01f);
    CHECK(gpu_post  > 0.5f);

    destroy(sim);
    dlclose(handle);
    std::remove(path.c_str());
}
