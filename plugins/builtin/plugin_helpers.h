// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// plugin_helpers.h — хелперы для встроенных .so плагинов.
// Включает modifier_helpers.h (единый источник формул) + plugin-специфичные шаблоны.
// Концепция: [[concept-single-source]] — нет дублирования формул.
#pragma once
#include "types.h"
#include "modifier_helpers.h"
#include <cmath>

// ═══════════════════════════════════════════════════════════════════════════════
// BACKWARD-COMPATIBLE ALIASES для существующих плагинов (127 .so)
// ═══════════════════════════════════════════════════════════════════════════════

static inline float plugin_nernst(float intra, float extra, float valence) {
    return nernst(intra, extra, valence);
}
static inline float plugin_segment_area(const Segment* s) {
    return segment_area(s);
}
static inline void plugin_clamp_ions(double* arr, int count) {
    clamp_ions(arr, count);
}

// effective_area_mod — plugin-специфичный (без сегмента, только из modifier state[7])
static inline float effective_area_mod(const Modifier* m) {
    if (m->state[7] > 0.0f) return m->state[7];
    return 1.0f;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ПРЕСИНАПТИЧЕСКИЕ МОДУЛЯТОРЫ
// ═══════════════════════════════════════════════════════════════════════════════

static inline float modulate_release_gi(Modifier* m, float* cleft_nt, float dt, int nt_index) {
    float nt = cleft_nt[nt_index];
    float activation = nt / (nt + 0.5f);
    m->state[0] += (activation - m->state[0]) * dt / 50.0f;
    double scale = m->density * effective_area_mod(m);
    return 1.0f - 0.7f * m->state[0] * scale;
}

static inline float modulate_release_gs(Modifier* m, float* cleft_nt, float dt, int nt_index) {
    float nt = cleft_nt[nt_index];
    float activation = nt / (nt + 0.5f);
    m->state[0] += (activation - m->state[0]) * dt / 30.0f;
    double scale = m->density * effective_area_mod(m);
    return 1.0f + 0.5f * m->state[0] * scale;
}

static inline float modulate_release_ionotropic(Modifier* m, float* cleft_nt, float dt, int nt_index) {
    float nt = cleft_nt[nt_index];
    float activation = nt / (nt + 0.5f);
    (void)dt;
    double scale = m->density * effective_area_mod(m);
    return 1.0f + 0.3f * activation * scale;
}

// ═══════════════════════════════════════════════════════════════════════════════
// СЕНСОРНАЯ АДАПТАЦИЯ
// ═══════════════════════════════════════════════════════════════════════════════

static inline void sensor_habituation_step(Modifier* m, int state_idx_H, float activation, float dt,
                                            float tau_hab_ms) {
    if (tau_hab_ms < 1.0f) return;
    m->state[state_idx_H] += (activation - m->state[state_idx_H]) * dt / tau_hab_ms;
    if (m->state[state_idx_H] < 0.0f) m->state[state_idx_H] = 0.0f;
    if (m->state[state_idx_H] > 1.0f) m->state[state_idx_H] = 1.0f;
}

static inline void sensor_sensitization_step(Modifier* m, int state_idx_S, float activation, float dt,
                                              float tau_sens_ms) {
    if (tau_sens_ms < 1.0f) return;
    m->state[state_idx_S] += (activation - m->state[state_idx_S]) * dt / tau_sens_ms;
    if (m->state[state_idx_S] < 0.0f) m->state[state_idx_S] = 0.0f;
    if (m->state[state_idx_S] > 1.0f) m->state[state_idx_S] = 1.0f;
}

static inline float sensor_adaptation_gain(float H, float S, float k_hab, float k_sens) {
    return (1.0f + k_sens * S) * (1.0f - k_hab * H);
}
