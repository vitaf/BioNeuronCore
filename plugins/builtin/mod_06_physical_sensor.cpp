// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// physical_sensor.cpp — Мультимодальный физический сенсор
// Тип: MOD_PHYSICAL_SENSOR (6)
// Категория: Базовые модификаторы
// Автогенерация: generate_builtin_plugins.py

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double physical_sensor_current_impl(Modifier* m, Segment* s, float dt) {
    float area = plugin_segment_area(s);

    // BIOREF: g≈0.1-1 mS/cm² (mechanosensitive Piezo1/TRP)
    double p_sens = (m->state[1] > 0.0f) ? m->state[1] : 1.0f;

    // Температурная чувствительность (TRPV1 >43°C, TRPM8 <25°C)
    float T = s->temperature;
    float dT_hot  = (T > 37.0f) ? (T - 37.0f) / 6.0f : 0.0f;
    float dT_cold = (T < 33.0f) ? (33.0f - T) / 8.0f : 0.0f;
    float temp_act = dT_hot + dT_cold;

    // Механочувствительность (растяжение, Piezo1/2)
    float stretch = (s->length - 50.0f) / 50.0f;
    if (stretch < 0.0f) stretch = 0.0f;
    float mech_act = stretch * 0.5f;

    // pH не моделируется (H⁺ отсутствует в ионной модели) — убран Na⁺-суррогат.
    float stimulus = p_sens * (temp_act + mech_act);

    m->state[0] += (stimulus - m->state[0]) * dt / 10.0f;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    // Адаптация: контр-направленная (habituation, state[2]) + со-направленная (sensitization, state[3])
    // Habituation: GRK/arrestin-десенситизация GPCR + Ca²⁺/CaM-инактивация канала (Piezo1/TRPV1)
    double tau_hab = (m->state[6] > 0.0f) ? m->state[6] : 2000.0f;  // по умолчанию SA (медленная), 2с
    sensor_habituation_step(m, 2, m->state[0], dt, tau_hab);
    // Sensitization: cAMP/PKA priming (очень медленная), tau=30s
    sensor_sensitization_step(m, 3, m->state[0], dt, 30000.0f);
    double k_hab = (m->state[4] > 0.0f) ? m->state[4] : 0.6f;
    double k_sens = (m->state[5] > 0.0f) ? m->state[5] : 0.2f;
    double gain = sensor_adaptation_gain(m->state[2], m->state[3], k_hab, k_sens);

    // GHK: неселективный катионный канал (Na⁺ 40% / K⁺ 30% / Ca²⁺ 30%)
    float vol = segment_volume(s);
    float cpm = FARADAY_CONST * vol / 1000.0f;
    double P_Na, P_K, P_Ca;
    ghk_partial_P(TRP_SENSOR_GAMMA_PS, 0.4f, 0.3f, 0.3f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area * m->state[0] * gain;
    float I_Na = (float)(P_Na * ghk_monovalent(s->V_m, s->intra_ions[ION_NA], s->extra_ions[ION_NA]) * N * A_TO_PA);
    float I_K  = (float)(P_K  * ghk_monovalent(s->V_m, s->intra_ions[ION_K],  s->extra_ions[ION_K])  * N * A_TO_PA);
    float I_Ca = (float)(P_Ca * ghk_divalent(s->V_m, s->intra_ions[ION_CA], s->extra_ions[ION_CA]) * N * A_TO_PA);
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpm;
    s->intra_ions[ION_K]  -= (I_K  * dt) / cpm;
    s->intra_ions[ION_CA] -= (I_Ca * dt) / (2.0f * cpm);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    return -(I_Na - I_K + I_Ca);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_PHYSICAL_SENSOR, physical_sensor_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Физический сенсор","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":6,"name":"Физический сенсор","category":"Базовые модификаторы","description":"Мультимодальный сенсор-трансдьюсер. Температура (TRPV1/TRPM8), механорастяжение (Piezo1/2), pH (ASIC). Неселективный катионный ток (E_rev≈0mV).","release":false}]})BIOCORE";
}
