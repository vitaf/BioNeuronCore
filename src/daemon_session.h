// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#pragma once
// daemon_session.h — изолированный контекст симуляции для одной сессии
#include "types.h"
#include "modifier_interface.h"
#include "daemon_state.h"
#include "consolidation.h"
#include "delay_buffer.h"
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <string>

class DaemonServer; // forward

struct SessionContext {
    uint32_t session_id = 0;

    // ── Владелец (DaemonServer для доступа к GPU-функциям) ──
    DaemonServer* server = nullptr;

    // ── Данные сети ──
    Segment*         segments = nullptr;
    Synapse*         synapses = nullptr;
    IntraConnection* connections = nullptr;
    Neuron*          neurons = nullptr;
    Nucleus*         nuclei = nullptr;
    uint32_t num_segments = 0;
    uint32_t num_synapses = 0;
    uint32_t num_connections = 0;
    uint32_t num_neurons = 0;
    uint32_t num_nuclei = 0;
    uint32_t loaded_mode = 0;   // режим симуляции из заголовка .bion (MODE_BIOLOGICAL=0, MODE_ARBITRARY=1)

    // ── GPU на сессию ──
    int   gpu_device_index = -1;  // -1 = CPU/default, >=0 = конкретное устройство
    void* gpu_sim = nullptr;      // Simulation* для этой сессии
    bool  simDataFromGpu = false;
    void* gpu_backend_handle = nullptr;  // указатель на GpuBackendHandle в DaemonServer

    // ── Стимулы ──
    static constexpr size_t MAX_STIM = 256;
    CurrentStimulus stimuli[MAX_STIM];
    uint32_t stimulus_count = 0;
    VoltageClamp    vclamps[MAX_STIM];
    uint32_t vclamp_count = 0;
    RampStimulus    ramps[MAX_STIM];
    uint32_t ramp_count = 0;
    IonInjection    ion_inj[MAX_STIM];
    uint32_t ion_inj_count = 0;
    LigandInjection lig_inj[MAX_STIM];
    uint32_t lig_inj_count = 0;
    std::mutex stim_mutex;

    // ── Состояние симуляции ──
    std::thread sim_thread;
    std::atomic<bool> sim_running{false};
    std::atomic<bool> sim_paused{false};
    std::atomic<bool> sim_step_requested{false};
    uint32_t sim_step_count = 1;
    bool sim_step_only = false;
    std::atomic<int> sim_backend{0};   // 0=CPU, 1=GPU
    float sim_multiplier = 1.0f;
    float sim_target_hz = 0.0f;
    uint64_t sim_tick_counter = 0;
    std::mutex sim_mutex;
    bool shutdown = false;

    // ── State streamer ──
    StateStreamer streamer;

    // ── Консолидация (per-session) ──
    bool consolidation_started = false;
    ConsolidationThread consolidation_thread;
    Neuron*  cons_neurons = nullptr;
    uint32_t cons_num_neurons = 0;
    Nucleus* cons_nuclei = nullptr;
    uint32_t cons_num_nuclei = 0;
    std::vector<uint8_t> initial_nuclei;

    // ── Delay buffer (per-session, был static в core_run_tick) ──
    std::vector<DelayedRelease> delay_buffer;
    uint32_t delay_syn_count = 0;

    // ── Таблицы функций модификаторов (per-session, были глобальными) ──
    ModifierFunc* active_library = nullptr;
    ReleaseModulatorFunc* active_modulators = nullptr;

    // ── Долгие задачи (Stage 5) ──
    uint64_t task_total_ticks = 0;    // всего тиков в задаче (для прогресса)
    uint64_t task_start_tick = 0;     // счётчик на старте задачи
    uint32_t checkpoint_interval = 0;  // 0=выключен
    std::string checkpoint_path;       // путь для авто-чекпоинтов
    uint64_t next_checkpoint_tick = 0;

    // ── Tiered execution (Stage 2: HOT/WARM/COLD) ──
    bool tiered_execution = false;
    std::vector<uint8_t>  segment_tiers;       // 0=HOT, 1=WARM, 2=COLD на сегмент
    std::vector<uint64_t> segment_last_spike;  // tick последнего спайка
    uint64_t tier_classify_tick = 0;
    uint32_t tier_classify_interval = 50;      // тиков между переклассификацией
    float    tier_hot_threshold = 5.0f;        // |V_m - V_rest| > 5mV → HOT
    float    tier_cold_threshold = 0.5f;       // |V_m - V_rest| < 0.5mV → COLD
    uint64_t tier_warm_ttl = 500;              // тиков в WARM после спайка
    float    tier_reclassify_tolerance = 0.1f; // допустимое отклонение при сверке

    // ── Освобождение памяти сети ──
    void freeNetwork() {
        if (segments)    { delete[] segments;    segments    = nullptr; }
        if (synapses)    { delete[] synapses;    synapses    = nullptr; }
        if (connections) { delete[] connections; connections = nullptr; }
        if (neurons)     { delete[] neurons;     neurons     = nullptr; }
        if (nuclei)      { delete[] nuclei;      nuclei      = nullptr; }
        num_segments = 0;
        num_synapses = 0;
        num_connections = 0;
        num_neurons = 0;
        num_nuclei = 0;
        simDataFromGpu = false;
    }

    ~SessionContext() {
        shutdown = true;
        if (sim_thread.joinable()) sim_thread.join();
        freeNetwork();
    }
};
