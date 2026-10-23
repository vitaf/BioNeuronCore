// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_111_thermal_noise.cpp — Тепловой шум (Johnson-Nyquist, stochastic V_m noise)
// Тип: MOD_THERMAL_NOISE (111)
//
// Биофизически-обоснованный тепловой шум мембранного потенциала.
// Использует xorshift32 PRNG (seed = segment_id ^ tick) для стохастичности.
// Амплитуда по умолчанию: σ=0.05mV RMS (Johnson-Nyquist для мембраны ~10pF при 37°C).
// state[0] = σ_V (мВ RMS), state[1] = seed_offset (опционально)

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

// xorshift32 PRNG (идентичен GPU-версии в ядрах)
static inline float xorshift32_rnd(uint32_t seed) {
    uint32_t x = seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return (float)(x & 0x7FFFFFFFu) / (float)0x7FFFFFFFu;
}

static double thermal_noise_current_impl(Modifier* m, Segment* s, float dt) {
    double sigma = (m->state[0] > 0.0f) ? m->state[0] : 0.05f;  // mV RMS
    double seed_off = m->state[1];  // опциональный сдвиг seed

    uint32_t seed = (s->id * 7919u + seed_off * 131u);
    // Используем dt-инвариантный seed (не зависит от tick, т.к. модификатор
    // не получает tick напрямую — используем V_m как источник энтропии)
    seed ^= (uint32_t)(s->V_m * 1000.0f);

    float r = xorshift32_rnd(seed);  // [0, 1)
    // Преобразование Бокса-Мюллера для нормального распределения (приближение)
    // Используем r для двух униформ → одна нормаль (упрощённо: равномерный ±1.7σ)
    // Более точно: r → N(0,σ) через erf-аппроксимацию
    float noise = (r * 2.0f - 1.0f) * sigma * 1.732f;  // ±√3·σ для равномерного → та же дисперсия

    // Возвращаем ток эквивалентный изменению V_m (I = C_m * dV/dt)
    float area = effective_area(m, s);
    float C_m = area * 0.01f;  // C_SPECIFIC_PF_PER_UM2
    if (C_m < 1e-4f) C_m = 1e-4f;
    float I_eq = noise * C_m / dt;  // nA эквивалентный данному шуму

    return I_eq;  // положительный = деполяризует, отрицательный = гиперполяризует
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_THERMAL_NOISE, thermal_noise_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Тепловой шум мембраны","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":111,"name":"Тепловой шум (Johnson-Nyquist)","category":"Физические модификаторы","description":"Биофизически-обоснованный тепловой шум мембранного потенциала. Xorshift32 PRNG (seed=segment_id ^ V_m). sigma=0.05mV RMS (Johnson-Nyquist для мембраны ~10pF при 37C). State[0]=sigma_mV, state[1]=seed_offset.","release":false}]})BIOCORE";
}
