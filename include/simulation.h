// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#pragma once
#include <sycl/sycl.hpp>
#include "types.h"
#include "bion_loader.h"
#include "delay_buffer.h"

class Simulation {
public:
    sycl::queue q;
    Segment* segments;
    IntraConnection* connections;
    Synapse* synapses;
    Neuron* neurons;
    NetworkHeader header;

    uint32_t num_segments;
    uint32_t num_connections;
    uint32_t num_synapses;
    uint32_t num_neurons;
    uint64_t tick_count;  // счётчик тиков для RNG в синаптическом выбросе

    // Capacity pool: буферы переиспользуются при загрузке новой сети.
    // Аллокация только при росте. Без alloc/free → нет i915 GEM-утечки.
    uint32_t seg_capacity = 0;
    uint32_t conn_capacity = 0;
    uint32_t syn_capacity = 0;
    size_t   delay_capacity = 0;

    BionData bion_data;   // данные, загруженные из .bion (USM)
    bool bion_loaded;     // true если данные загружены через load_network/load_bion

    DelayedRelease* delay_buffer;  // USM-буфер задержанных выбросов (N_syn × MAX_DELAY_SLOTS)
    GpuStimulusData* stim_data;    // USM-буфер стимулов (загружается через upload_stimuli)
    uint32_t* glial_indices;       // USM-буфер индексов глиальных сегментов (с MOD_GLAST) для шага 3
    uint32_t glial_count;          // число глиальных сегментов (обновляется каждый тик)
    uint32_t glial_capacity;       // ёмкость буфера glial_indices (= num_segments)

    Simulation(sycl::device dev);
    ~Simulation();

    int load_network(const char* filename);   // обратная совместимость
    int load_bion(const char* filename);      // загрузка из .bion (основной метод)
    int save_checkpoint(const char* filename);
    int export_bion(const char* filename);    // синоним save_checkpoint
    void run_tick(float dt);
    void upload_stimuli(const GpuStimulusData* src);  // копирует стимулы в USM
    const BionData* get_bion_data() const { return &bion_data; }

private:
    int validate_bion_data();  // валидация загруженных данных
};
