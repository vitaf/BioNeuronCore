// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// delay_buffer.h — буфер задержанных синаптических выбросов
// Каждый синапс имеет до MAX_DELAY_SLOTS отложенных выбросов.
// Использует поле Synapse::delay (мс) для рострально-каудальной волны.
// Память: MAX_DELAY_SLOTS=4 × 16 байт × N синапсов ≈ 384 KB на 6000 синапсов.

#pragma once
#include "types.h"
#include "kernel_dispatch.h"
#include <cstdint>

#define MAX_DELAY_SLOTS 4

#pragma pack(push, 1)
typedef struct {
    uint64_t arrival_tick;   // тик, на котором применить выброс
    float    amount;         // количество нейромедиатора или I_gap (nA)
    uint8_t  nt_type;        // тип нейромедиатора (NT_GLUTAMATE, ...)
    uint8_t  _pad[3];
} DelayedRelease;
#pragma pack(pop)

static_assert(sizeof(DelayedRelease) == 16, "DelayedRelease must be 16 bytes");

// Обработать очередь задержанных выбросов для одного синапса.
// Вызывается В НАЧАЛЕ шага 3, до проверки pre-spike.
// Возвращает true если был применён хотя бы один выброс.
static inline BIOCORE_HOST_DEVICE bool process_delayed_releases(DelayedRelease* slots, int max_slots,
                                             uint64_t current_tick, float* cleft_nt) {
    bool applied = false;
    for (int s = 0; s < max_slots; s++) {
        if (slots[s].arrival_tick == 0) continue;
        if (slots[s].arrival_tick <= current_tick) {
            cleft_nt[slots[s].nt_type] += slots[s].amount;
            slots[s].arrival_tick = 0; // освободить слот
            applied = true;
        }
    }
    return applied;
}

// Поставить выброс в очередь с задержкой.
// Возвращает true если слот найден.
static inline BIOCORE_HOST_DEVICE bool enqueue_delayed_release(DelayedRelease* slots, int max_slots,
                                            uint64_t arrival_tick, float amount, uint8_t nt_type) {
    for (int s = 0; s < max_slots; s++) {
        if (slots[s].arrival_tick == 0) {
            slots[s].arrival_tick = arrival_tick;
            slots[s].amount = amount;
            slots[s].nt_type = nt_type;
            return true;
        }
    }
    return false; // все слоты заняты — выброс теряется
}

// Вычислить задержку в тиках из миллисекунд
static inline BIOCORE_HOST_DEVICE uint64_t delay_ms_to_ticks(float delay_ms, float dt_ms) {
    if (delay_ms <= 0.0f || dt_ms <= 0.0f) return 0;
    return (uint64_t)((delay_ms / dt_ms) + 0.5f); // round to nearest tick
}
