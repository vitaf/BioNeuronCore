// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// gpu_backend.cpp — C-обёртка вокруг Simulation класса (SYCL/GPU).
// Компилируется DPC++ (Intel) или nvcc (NVIDIA) в libbiocore_gpu_backend_*.so.
// Studio/Daemon загружают через dlopen для переключения CPU → GPU.
#include "simulation.h"
#include "daemon_log.h"
#include <sycl/sycl.hpp>
#include <vector>
#include <cstring>
#include <cstdlib>

// ── Native CUDA: без маскировки бекендов ──
// NVIDIA GPU компилируется нативным nvcc — JIT не нужен.

// ── Кэш устройств (заполняется при первом вызове device_count) ──
static std::vector<sycl::device> g_devices;
static bool g_devices_enumerated = false;

static void enumerate_devices() {
    if (g_devices_enumerated) return;
    try {
        auto platforms = sycl::platform::get_platforms();
        for (auto& p : platforms) {
            auto devs = p.get_devices();
            for (auto& d : devs) {
                // Пропускаем CPU-устройства: CPU-путь есть в Studio (GCC)
                if (d.is_cpu()) continue;
                g_devices.push_back(d);
            }
        }
    } catch (...) {
        // Если SYCL runtime не доступен — оставляем пустой список
    }
    g_devices_enumerated = true;
}

extern "C" {

void* gpu_backend_create_on_device(int device_index);

void* gpu_backend_create() {
    return gpu_backend_create_on_device(0);
}

void* gpu_backend_create_on_device(int device_index) {
    enumerate_devices();
    if (device_index < 0 || device_index >= (int)g_devices.size()) {
        // Fallback: default_selector
        try {
            sycl::device dev(sycl::default_selector_v);
            return new Simulation(dev);
        } catch (...) {
            return nullptr;
        }
    }
    try {
        return new Simulation(g_devices[device_index]);
    } catch (...) {
        return nullptr;
    }
}

int gpu_backend_device_count() {
    enumerate_devices();
    return (int)g_devices.size();
}

int gpu_backend_get_device_info(int index, GpuDeviceInfo* info) {
    enumerate_devices();
    if (index < 0 || index >= (int)g_devices.size() || !info) return -1;
    auto& d = g_devices[index];
    memset(info, 0, sizeof(GpuDeviceInfo));
    try {
        std::string name = d.get_info<sycl::info::device::name>();
        strncpy(info->name, name.c_str(), 127);
        info->name[127] = '\0';
        info->vendor_id = d.get_info<sycl::info::device::vendor_id>();
        info->is_cpu = d.is_cpu() ? 1 : 0;
        info->global_mem_bytes = d.get_info<sycl::info::device::global_mem_size>();
        info->max_compute_units = d.get_info<sycl::info::device::max_compute_units>();
        info->device_index = index;
#if defined(__ADAPTIVECPP__) || defined(__HIPSYCL__)
        strncpy(info->backend_type, "cuda", 15);
#else
        strncpy(info->backend_type, "intel", 15);
#endif
        info->backend_type[15] = '\0';
    } catch (...) {
        // устройство недоступно при детальном опросе
        info->name[0] = '?';
        info->name[1] = '\0';
    }
    return 0;
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
    try {
        static_cast<Simulation*>(sim)->run_tick(dt);
    } catch (const std::exception& e) {
        DLOG_ERROR("GPU_BACKEND: run_tick exception: %s", e.what());
    } catch (...) {
        DLOG_ERROR("GPU_BACKEND: run_tick unknown exception");
    }
}

void* gpu_backend_bion_data(void* sim) {
    if (!sim) return nullptr;
    return const_cast<BionData*>(static_cast<Simulation*>(sim)->get_bion_data());
}

void gpu_backend_upload_stimuli(void* sim, const GpuStimulusData* src) {
    if (!sim || !src) return;
    static_cast<Simulation*>(sim)->upload_stimuli(src);
}

// Diagnostic: returns offsetof(Segment, I_buffer) as computed by SYCL compiler
int gpu_backend_ibuf_offset() {
    // Compute offset manually: sum of all fields before I_buffer
    // This MUST match what GCC sees. If it doesn't, we have a compiler mismatch.
    return (int)offsetof(Segment, I_buffer);
}

int gpu_backend_vm_offset() {
    return (int)offsetof(Segment, V_m);
}

int gpu_backend_seg_size() {
    return (int)sizeof(Segment);
}

} // extern "C"
