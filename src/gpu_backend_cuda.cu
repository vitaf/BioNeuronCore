// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// gpu_backend_cuda.cu — нативный CUDA-бекенд (nvcc)
// Содержит: CUDA-вариант класса Simulation + extern "C" C-обёртку.
// Ядра — в kernels/kernels_cuda.cu, модификаторы — в hh_biological.cpp,
// receptors_biological.cpp, dispatch_generated.cpp.
// Все компилируются раздельно и device-линкуются (-rdc=true).

#include <cuda_runtime.h>
#include <cstring>
#include <cstdlib>
#include <cmath>

#include "types.h"
#include "kernel_dispatch.h"
#include "modifier_interface.h"
#include "bion_loader.h"
#include "delay_buffer.h"

// ── Декларации CUDA-ядер (host-лаунчеры из kernels_cuda.cu) ──

extern void compute_modifier_currents(void* stream, Segment* segs, uint32_t N,
                                       uint8_t mode, float dt,
                                       const GpuStimulusData* stim_data);
extern void update_membrane(void* stream, Segment* segs, uint32_t N, float dt, uint64_t tick,
                             const GpuStimulusData* stim_data);
extern void compute_synaptic_currents(void* stream, Segment* segs, Synapse* syns,
                                       uint32_t K, uint32_t num_segs, uint8_t mode, uint64_t tick, float dt,
                                       DelayedRelease* delay_buffer,
                                       uint32_t* glial_indices, uint32_t glial_count);
extern void compute_diffusion(void* stream, Segment* segs, IntraConnection* conns, uint32_t M, uint32_t num_segs, float dt,
                              float* extra_delta, float* intra_delta, float* intra_nt_delta, float* nt_delta,
                              float* atp_delta, float* temp_delta, float* txn_delta, float* i_np_accum);
extern void compute_plasticity(void* stream, Segment* segs, Synapse* syns, uint32_t K, float dt);
extern void compute_clamp_segments(void* stream, Segment* segs, uint32_t N);

// ── ABI-верификация (должна совпадать с GCC-сборкой Studio/Daemon) ──

static_assert(sizeof(Modifier) == 88, "Modifier size mismatch with GCC");
static_assert(sizeof(Segment) == 3216, "Segment size mismatch with GCC (v14: I_buffer 8-aligned)");
static_assert(sizeof(Synapse) == 1568, "Synapse size mismatch with GCC");
static_assert(sizeof(IntraConnection) == 16, "IntraConnection size mismatch with GCC (v9: +_pad)");
static_assert(sizeof(Neuron) == 24, "Neuron size mismatch with GCC");
static_assert(sizeof(Nucleus) == 500, "Nucleus size mismatch with GCC (GENE_COUNT=57)");
static_assert(sizeof(ConsolidationEvent) == 36, "ConsolidationEvent size mismatch with GCC");
static_assert(sizeof(NetworkHeader) == 100, "NetworkHeader size mismatch with GCC (v10: +file_sz fields)");
static_assert(sizeof(DelayedRelease) == 16, "DelayedRelease size mismatch with GCC");

// ── Класс Simulation (CUDA) ──────────────────────────────────────────

class Simulation {
public:
    cudaStream_t stream;
    Segment* segments = nullptr;
    IntraConnection* connections = nullptr;
    Synapse* synapses = nullptr;
    Neuron* neurons = nullptr;
    NetworkHeader header;
    uint32_t num_segments = 0;
    uint32_t num_connections = 0;
    uint32_t num_synapses = 0;
    uint32_t num_neurons = 0;
    uint64_t tick_count = 0;
    BionData bion_data;
    bool bion_loaded = false;
    DelayedRelease* delay_buffer = nullptr;
    GpuStimulusData* stim_data = nullptr;  // USM-буфер стимулов
    uint32_t* glial_indices = nullptr;     // индексы глиальных сегментов (с MOD_GLAST) для шага 3
    uint32_t glial_count = 0;
    uint32_t glial_capacity = 0;

    // Temp-буферы для двухпроходной диффузии (CPU-GPU паритет)
    float* extra_delta     = nullptr;  // num_segments * ION_COUNT
    float* intra_delta     = nullptr;  // num_segments * ION_COUNT
    float* intra_nt_delta  = nullptr;  // num_segments * NT_COUNT
    float* nt_delta        = nullptr;  // num_segments * NT_COUNT
    float* atp_delta       = nullptr;  // num_segments
    float* temp_delta      = nullptr;  // num_segments (temperature)
    float* txn_delta       = nullptr;  // num_segments (transcription)
    float* i_np_accum      = nullptr;  // num_segments (заряд-флюкс сабстеппинга)

    Simulation(int device_index = 0) {
        int ndev = 0;
        cudaGetDeviceCount(&ndev);
        if (ndev == 0) {
            stream = nullptr;
            return;
        }
        if (device_index < 0 || device_index >= ndev) device_index = 0;
        cudaSetDevice(device_index);
        cudaStreamCreate(&stream);
        memset(&header, 0, sizeof(header));
        memset(&bion_data, 0, sizeof(bion_data));
    }

    ~Simulation() {
        if (delay_buffer)   { cudaFree(delay_buffer);   delay_buffer = nullptr; }
        if (stim_data)      { cudaFree(stim_data);      stim_data = nullptr; }
        if (glial_indices)  { cudaFree(glial_indices);  glial_indices = nullptr; }
        if (extra_delta)    { cudaFree(extra_delta);    extra_delta = nullptr; }
        if (intra_delta)    { cudaFree(intra_delta);    intra_delta = nullptr; }
        if (intra_nt_delta) { cudaFree(intra_nt_delta); intra_nt_delta = nullptr; }
        if (nt_delta)       { cudaFree(nt_delta);       nt_delta = nullptr; }
        if (atp_delta)      { cudaFree(atp_delta);      atp_delta = nullptr; }
        if (temp_delta)     { cudaFree(temp_delta);     temp_delta = nullptr; }
        if (txn_delta)      { cudaFree(txn_delta);      txn_delta = nullptr; }
        if (i_np_accum)     { cudaFree(i_np_accum);     i_np_accum = nullptr; }
        if (bion_loaded) {
            bion_unload_cuda(&bion_data);
        } else {
            if (segments)    { cudaFree(segments); segments = nullptr; }
            if (connections) { cudaFree(connections); connections = nullptr; }
            if (synapses)    { cudaFree(synapses); synapses = nullptr; }
            if (neurons)     { cudaFree(neurons); neurons = nullptr; }
        }
        if (stream) { cudaStreamDestroy(stream); stream = nullptr; }
    }

    int load_network(const char* filename) {
        return load_bion(filename);
    }

    int load_bion(const char* filename) {
        if (bion_loaded) {
            bion_unload_cuda(&bion_data);
        } else {
            if (segments)    { cudaFree(segments); segments = nullptr; }
            if (connections) { cudaFree(connections); connections = nullptr; }
            if (synapses)    { cudaFree(synapses); synapses = nullptr; }
            if (neurons)     { cudaFree(neurons); neurons = nullptr; }
        }
        bion_loaded = false;
        memset(&bion_data, 0, sizeof(bion_data));

        int ret = bion_load_cuda(filename, &bion_data);
        if (ret != 0) return ret;

        header         = bion_data.header;
        neurons        = bion_data.neurons;
        segments       = bion_data.segments;
        connections    = bion_data.connections;
        synapses       = bion_data.synapses;
        num_neurons    = bion_data.num_neurons;
        num_segments   = bion_data.num_segments;
        num_connections = bion_data.num_connections;
        num_synapses   = bion_data.num_synapses;
        bion_loaded    = true;

        if (delay_buffer) { cudaFree(delay_buffer); delay_buffer = nullptr; }
        if (num_synapses > 0) {
            size_t db_size = (size_t)num_synapses * MAX_DELAY_SLOTS;
            cudaMallocManaged(&delay_buffer, db_size * sizeof(DelayedRelease));
            if (delay_buffer)
                memset(delay_buffer, 0, db_size * sizeof(DelayedRelease));
        }

        // Глиальные индексы (сегменты с MOD_GLAST) для шага 3
        if (glial_indices) { cudaFree(glial_indices); glial_indices = nullptr; }
        if (num_segments > 0) {
            cudaMallocManaged(&glial_indices, (size_t)num_segments * sizeof(uint32_t));
            if (glial_indices) memset(glial_indices, 0, (size_t)num_segments * sizeof(uint32_t));
            glial_capacity = num_segments;
        } else {
            glial_capacity = 0;
        }
        glial_count = 0;

        // Temp-буферы для двухпроходной диффузии (CPU-GPU паритет)
        if (extra_delta)    { cudaFree(extra_delta);    extra_delta = nullptr; }
        if (intra_delta)    { cudaFree(intra_delta);    intra_delta = nullptr; }
        if (intra_nt_delta) { cudaFree(intra_nt_delta); intra_nt_delta = nullptr; }
        if (nt_delta)       { cudaFree(nt_delta);       nt_delta = nullptr; }
        if (atp_delta)      { cudaFree(atp_delta);      atp_delta = nullptr; }
        if (temp_delta)     { cudaFree(temp_delta);     temp_delta = nullptr; }
        if (txn_delta)      { cudaFree(txn_delta);      txn_delta = nullptr; }
        if (i_np_accum)     { cudaFree(i_np_accum);     i_np_accum = nullptr; }
        if (num_segments > 0) {
            cudaMallocManaged(&extra_delta,    (size_t)num_segments * ION_COUNT * sizeof(float));
            cudaMallocManaged(&intra_delta,    (size_t)num_segments * ION_COUNT * sizeof(float));
            cudaMallocManaged(&intra_nt_delta, (size_t)num_segments * NT_COUNT * sizeof(float));
            cudaMallocManaged(&nt_delta,       (size_t)num_segments * NT_COUNT * sizeof(float));
            cudaMallocManaged(&atp_delta,      (size_t)num_segments * sizeof(float));
            cudaMallocManaged(&temp_delta,     (size_t)num_segments * sizeof(float));
            cudaMallocManaged(&txn_delta,      (size_t)num_segments * sizeof(float));
            cudaMallocManaged(&i_np_accum,     (size_t)num_segments * sizeof(float));
            if (extra_delta)    memset(extra_delta,    0, (size_t)num_segments * ION_COUNT * sizeof(float));
            if (intra_delta)    memset(intra_delta,    0, (size_t)num_segments * ION_COUNT * sizeof(float));
            if (intra_nt_delta) memset(intra_nt_delta, 0, (size_t)num_segments * NT_COUNT * sizeof(float));
            if (nt_delta)       memset(nt_delta,       0, (size_t)num_segments * NT_COUNT * sizeof(float));
            if (atp_delta)      memset(atp_delta,      0, (size_t)num_segments * sizeof(float));
            if (temp_delta)     memset(temp_delta,     0, (size_t)num_segments * sizeof(float));
            if (txn_delta)      memset(txn_delta,      0, (size_t)num_segments * sizeof(float));
            if (i_np_accum)     memset(i_np_accum,     0, (size_t)num_segments * sizeof(float));
        }

        // Prefetch всех данных на GPU — убирает page-fault штраф на первом тике
        int device = 0; cudaGetDevice(&device);
        cudaMemLocation loc = {}; loc.type = cudaMemLocationTypeDevice; loc.id = device;
        if (num_segments > 0)   cudaMemPrefetchAsync(segments,    num_segments * sizeof(Segment),          loc, 0, stream);
        if (num_connections > 0) cudaMemPrefetchAsync(connections, num_connections * sizeof(IntraConnection), loc, 0, stream);
        if (num_synapses > 0)   cudaMemPrefetchAsync(synapses,    num_synapses * sizeof(Synapse),          loc, 0, stream);
        if (delay_buffer)       cudaMemPrefetchAsync(delay_buffer, (size_t)num_synapses * MAX_DELAY_SLOTS * sizeof(DelayedRelease), loc, 0, stream);

        active_library = biological_library;
        active_modulators = biological_modulators;

        return validate_bion_data();
    }

    int validate_bion_data() {
        if (num_segments == 0) return -1;
        return 0;
    }

    int save_checkpoint(const char* filename) {
        header.num_segments = num_segments;
        header.num_intra_connections = num_connections;
        header.num_synapses = num_synapses;
        header.num_neurons = num_neurons;

        BionData save_data;
        memset(&save_data, 0, sizeof(save_data));
        save_data.header       = header;
        save_data.neurons      = neurons;
        save_data.segments     = segments;
        save_data.connections  = connections;
        save_data.synapses     = synapses;
        save_data.num_neurons    = num_neurons;
        save_data.num_segments   = num_segments;
        save_data.num_connections = num_connections;
        save_data.num_synapses   = num_synapses;

        return bion_save(filename, &save_data);
    }

    int export_bion(const char* filename) {
        return save_checkpoint(filename);
    }

    void upload_stimuli(const GpuStimulusData* src) {
        if (!stim_data) {
            cudaMallocManaged(&stim_data, sizeof(GpuStimulusData));
            if (!stim_data) return;
        }
        memcpy(stim_data, src, sizeof(GpuStimulusData));
    }

    void run_tick(float dt) {
        if (!stream) return;
        uint8_t mode = header.mode;
        uint64_t tick = tick_count++;

        if (stim_data) stim_data->current_tick = tick;

        // ── Шаг 1-2.5: модификаторы, осевые токи, диффузия (GPU, один stream) ──
        if (num_segments > 0) {
            compute_modifier_currents((void*)stream, segments, num_segments, mode, dt, stim_data);
        }
        if (num_connections > 0) {
            // Осевой ток (Ом) убран — заряд-флюкс теперь в compute_diffusion (Нернст-Планк).
            compute_diffusion((void*)stream, segments, connections, num_connections, num_segments, dt,
                              extra_delta, intra_delta, intra_nt_delta, nt_delta,
                              atp_delta, temp_delta, txn_delta, i_np_accum);
        }

        // ── Шаг 3-3.5: синапсы + пластичность (NT-сброс в шаге 4) ──
        if (num_synapses > 0) {
            // Сброс нейромедиаторов перед синаптическим шагом
            for (uint32_t i = 0; i < num_segments; i++) {
                for (int n = 0; n < NT_COUNT; n++) segments[i].neurotransmitter[n] = 0.0f;
            }

            // Protein→parameter mapping (BION v7): CPU-side loop перед GPU-ядром.
            // Managed-память доступна с CPU — копирование не требуется.
            if (bion_data.nuclei && bion_data.num_nuclei > 0) {
                for (uint32_t i = 0; i < num_synapses; i++) {
                    Synapse* syn = &synapses[i];
                    if (syn->id == 0) continue;
                    uint32_t pre_idx = syn->pre_segment;
                    if (pre_idx < num_segments) {
                        uint32_t nid = segments[pre_idx].neuron_id;
                        if (nid < bion_data.num_nuclei) {
                            rrp_apply_protein_mapping(syn, bion_data.nuclei[nid].protein_level);
                        }
                    }
                }
            }

            // ── Глиальные индексы (сегменты с MOD_GLAST) для посегментного захвата NT ──
            glial_count = 0;
            for (uint32_t g = 0; g < num_segments && glial_count < glial_capacity; g++)
                for (uint8_t j = 0; j < segments[g].modifier_count; j++)
                    if (segments[g].modifiers[j].type == MOD_GLAST) { glial_indices[glial_count++] = g; break; }

            compute_synaptic_currents((void*)stream, segments, synapses,
                                       num_synapses, num_segments, mode, tick, dt, delay_buffer,
                                       glial_indices, glial_count);
            compute_plasticity((void*)stream, segments, synapses, num_synapses, dt);
        }

        // ── Шаг 4: V_m + сброс I_buffer (GPU) ──
        if (num_segments > 0) {
            update_membrane((void*)stream, segments, num_segments, dt, tick, stim_data);
            compute_clamp_segments((void*)stream, segments, num_segments);
        }
        cudaStreamSynchronize(stream);  // нужно: CPU читает V_m после run_tick
    }

    const BionData* get_bion_data() const { return &bion_data; }
};

// ── extern "C" C-обёртка (API совместим с gpu_backend.cpp) ──────────

extern "C" {

void* gpu_backend_create_on_device(int device_index) {
    int ndev = 0;
    cudaGetDeviceCount(&ndev);
    if (ndev == 0) return nullptr;
    if (device_index < 0 || device_index >= ndev) device_index = 0;
    try {
        return new Simulation(device_index);
    } catch (...) {
        return nullptr;
    }
}

void* gpu_backend_create() {
    return gpu_backend_create_on_device(0);
}

void gpu_backend_destroy(void* sim) {
    if (sim) delete static_cast<Simulation*>(sim);
}

int gpu_backend_load_bion(void* sim, const char* filename) {
    if (!sim || !filename) return -1;
    return static_cast<Simulation*>(sim)->load_bion(filename);
}

void gpu_backend_run_tick(void* sim, float dt) {
    if (!sim) return;
    static_cast<Simulation*>(sim)->run_tick(dt);
}

void* gpu_backend_bion_data(void* sim) {
    if (!sim) return nullptr;
    return const_cast<BionData*>(static_cast<Simulation*>(sim)->get_bion_data());
}

void gpu_backend_upload_stimuli(void* sim, const GpuStimulusData* src) {
    if (!sim || !src) return;
    static_cast<Simulation*>(sim)->upload_stimuli(src);
}

int gpu_backend_device_count() {
    int ndev = 0;
    cudaGetDeviceCount(&ndev);
    return ndev;
}

int gpu_backend_get_device_info(int index, GpuDeviceInfo* info) {
    if (!info) return -1;
    int ndev = 0;
    cudaGetDeviceCount(&ndev);
    if (index < 0 || index >= ndev) return -1;
    memset(info, 0, sizeof(GpuDeviceInfo));
    cudaDeviceProp prop;
    if (cudaGetDeviceProperties(&prop, index) != cudaSuccess) return -1;
    strncpy(info->name, prop.name, 127);
    info->name[127] = '\0';
    info->vendor_id = 0x10DE;  // NVIDIA
    info->is_cpu = 0;
    info->global_mem_bytes = prop.totalGlobalMem;
    info->max_compute_units = prop.multiProcessorCount;
    info->device_index = index;
    strncpy(info->backend_type, "cuda", 15);
    info->backend_type[15] = '\0';
    return 0;
}

// ── Диагностические offset-функции (ABI-верификация) ──

int gpu_backend_ibuf_offset() { return (int)offsetof(Segment, I_buffer); }
int gpu_backend_vm_offset()   { return (int)offsetof(Segment, V_m); }
int gpu_backend_seg_size()    { return (int)sizeof(Segment); }

} // extern "C"
