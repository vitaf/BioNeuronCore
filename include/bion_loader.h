// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#pragma once
// bion_loader.h — platform-agnostic declarations for .bion file I/O.
// For SYCL USM allocation, the USM variants are enabled when
// compiled with the Intel DPC++ compiler (__INTEL_LLVM_COMPILER defined).
#include <cstdio>
#include "types.h"
#include "bion_format.h"

// Структура, возвращаемая загрузчиком .bion.
// Указатели могут быть host-памятью (new[]) или USM (sycl::malloc_shared).
// Владение передаётся вызывающему — нужно вызвать соответствующий unload.
struct BionData {
    NetworkHeader     header;
    Neuron*           neurons;
    Segment*          segments;
    IntraConnection*  connections;
    Synapse*          synapses;
    Nucleus*          nuclei;
    uint32_t          num_neurons;
    uint32_t          num_segments;
    uint32_t          num_connections;
    uint32_t          num_synapses;
    uint32_t          num_nuclei;
};

// ── Host-память (new[]/delete[], не требует SYCL) ───────────────────────

int  bion_load_host(const char* filename, BionData* data);
void bion_unload_host(BionData* data);

// ── Сохранение (работает и с host, и с USM-памятью) ─────────────────────

int bion_save(const char* filename, const BionData* data);

// ── USM-память (sycl::malloc_shared, только с SYCL-компилятором) ───────

#if defined(__INTEL_LLVM_COMPILER) || defined(__ADAPTIVECPP__) || defined(__HIPSYCL__)
#include <sycl/sycl.hpp>
int  bion_load_usm(const char* filename, BionData* data, sycl::queue& q);
void bion_unload_usm(BionData* data, sycl::queue& q);
// Загрузка в ПРЕДВЫДЕЛЕННЫЕ буферы (data->segments/connections/synapses
// уже указывают на готовую память). Без аллокаций → без i915 GEM-утечки.
int  bion_load_into_buffers(const char* filename, BionData* data);
#endif

// ── CUDA unified memory (cudaMallocManaged, только с nvcc) ─────────────

#if defined(__CUDACC__)
int  bion_load_cuda(const char* filename, BionData* data);
void bion_unload_cuda(BionData* data);
#endif
