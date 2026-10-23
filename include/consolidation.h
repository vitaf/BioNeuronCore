// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.
//
// Консолидация/реконсолидация (двухстадийная память) — фоновый поток структурной пластичности.
// Запускается при старте симуляции, работает параллельно с шагами 1-4.
// Читает segments/synapses/nuclei без блокировок, пишет ConsolidationEvent в очередь.
// Симуляционный поток применяет события на шаге 5.

#pragma once
#include "types.h"
#include <atomic>
#include <mutex>
#include <thread>

// ── Структура для кэширования V_m-паттерна (детекция reactivation) ──────
#define REACTIVATION_WINDOW 1000  // тиков истории V_m для сравнения паттернов
#define REACTIVATION_CORR_THRESHOLD 0.7f

typedef struct {
    float    v_m_history[REACTIVATION_WINDOW];  // кольцевой буфер V_m сомы
    uint32_t head;                              // текущая позиция записи
    uint32_t count;                             // количество записанных отсчётов
} ReactivationBuffer;

class ConsolidationThread {
public:
    ConsolidationThread();
    ~ConsolidationThread();

    // Вызывается при старте симуляции
    void start(Segment* segments, uint32_t num_segments,
               Synapse* synapses, uint32_t* num_synapses_ptr, uint32_t max_synapses,
               Neuron* neurons, uint32_t num_neurons,
               Nucleus* nuclei, uint32_t num_nuclei,
               uint64_t* tick_counter);

    void stop();
    bool isRunning() const { return running_.load(); }

    // Потокобезопасное применение ожидающих событий к данным симуляции
    // Вызывается из simulation_host.cpp на шаге 5.
    // Возвращает количество применённых событий.
    uint32_t applyPendingEvents(Segment* segments, uint32_t num_segments,
                                 Synapse* synapses, uint32_t* num_synapses,
                                 uint32_t max_synapses);

    // Конфигурация
    void setGlobalInterval(uint32_t ticks);  // 0 = OFF
    uint32_t getGlobalInterval() const { return global_interval_.load(); }
    uint32_t getPendingEventCount() const { return event_count_.load(); }

    // Для Studio: снимок ядра нейрона
    bool getNucleusSnapshot(uint32_t neuron_idx, Nucleus* out) const;
    bool setNucleus(uint32_t neuron_idx, const Nucleus* data);  // запись ядра из Studio

    // Сома→Nucleus связь (вызывается каждый тик из simulation_host.cpp шаг 2.5b)
    void updateNucleusFromSoma(Segment* segments, Neuron* neurons,
                                Nucleus* nuclei, uint32_t num_neurons);

private:
    void threadLoop();

    // Один цикл консолидации: ген→белок→структура
    void runConsolidationCycle(uint64_t current_tick);

    // ── Читаемые указатели (устанавливаются один раз при start) ──────
    Segment*  segments_ = nullptr;
    Synapse*  synapses_ = nullptr;
    Neuron*   neurons_ = nullptr;
    Nucleus*  nuclei_ = nullptr;
    uint32_t  num_segments_ = 0;
    uint32_t* num_synapses_ptr_ = nullptr;
    uint32_t  max_synapses_ = 0;
    uint32_t  num_neurons_ = 0;
    uint32_t  num_nuclei_ = 0;
    uint64_t* tick_counter_ = nullptr;

    // ── Поток ────────────────────────────────────────────────────────
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<uint32_t> global_interval_{10000};

    // ── Очередь событий (producer: consolidation, consumer: simulation) ──
    std::mutex event_mutex_;
    ConsolidationEvent pending_events_[MAX_CONSOL_EVENTS];
    std::atomic<uint32_t> event_count_{0};

    // ── Снимки ядер для Studio (double-buffered, mutex) ──────────────
    mutable std::mutex nucleus_mutex_;
    Nucleus* nucleus_snapshot_ = nullptr;

    // ── Буферы реактивации для реконсолидации ────────────────────────
    ReactivationBuffer* reactivation_buffers_ = nullptr;
};
