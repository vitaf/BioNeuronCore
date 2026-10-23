// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// vibration_sensor.cpp — Вибрационный/акустический сенсор (аудиотаксис)
// Тип: MOD_VIBRATION_SENSOR (115). GHK-версия.
// Детекция колебаний воды/звука через механотрансдукцию.
// Активация пропорциональна амплитуде вибрации в среде.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double vibration_sensor_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);

    // Параметры через state[]
    double sensitivity = (m->active_vars >= 1 && m->state[m->active_vars] > 0.0f)
                        ? m->state[m->active_vars] : 3.0f;   // крутизна активации

    // state[0] = open probability (активация)
    // state[1] = vibration intensity (выставляется средой/сенсорным слоем, 0..1)
    double intensity = m->state[1];
    if (intensity < 0.0f) intensity = 0.0f;
    if (intensity > 1.0f) intensity = 1.0f;

    // Сигмоидная активация: P_open = sigmoid(intensity, mid=0.3, slope=sensitivity)
    float midpoint = 0.3f;
    float target_open = 1.0f / (1.0f + expf(-sensitivity * (intensity - midpoint) * 10.0f));
    m->state[0] += (target_open - m->state[0]) * dt / 30.0f;  // tau=30ms — быстрый ответ
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    // Habituation (state[2]): адаптация к постоянной вибрации, tau=3s
    sensor_habituation_step(m, 2, m->state[0], dt, 3000.0f);
    if (m->state[2] < 0.0f) m->state[2] = 0.0f;
    if (m->state[2] > 1.0f) m->state[2] = 1.0f;
    // Sensitization (state[3]): усиление при слабом но повторяющемся сигнале, tau=10s
    sensor_sensitization_step(m, 3, m->state[0], dt, 10000.0f);
    double k_sens = (m->state[4] > 0.0f) ? m->state[4] : 0.25f;
    double gain = sensor_adaptation_gain(m->state[2], m->state[3], 0.6f, k_sens);
    double effective_open = m->state[0] * gain;

    // Механотрансдукция: неселективный катионный канал (Na⁺/K⁺/Ca²⁺)
    // Пропускает смесь Na⁺ (60%) и K⁺ (40%) — деполяризующий ток
    double P_Na, P_K, P_Ca;
    ghk_partial_P(MET_GAMMA_PS, 0.6f, 0.4f, 0.0f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area * effective_open;
    float I_Na = (float)(P_Na * ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]) * N * A_TO_PA);
    float I_K  = (float)(P_K  * ghk_monovalent(V, s->intra_ions[ION_K],  s->extra_ions[ION_K])  * N * A_TO_PA);

    s->intra_ions[ION_NA] -= (I_Na * dt) / cpm;
    s->intra_ions[ION_K]  -= (I_K  * dt) / cpm;
    plugin_clamp_ions(s->intra_ions, ION_COUNT);

    return -(I_Na + I_K);  // positive = depolarizing
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_VIBRATION_SENSOR, vibration_sensor_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Вибрационный сенсор (аудиотаксис)","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":115,"name":"Вибрационный/акустический сенсор","category":"Сенсорные рецепторы","description":"Механотрансдуктор для аудиотаксиса. Детекция колебаний воды через неселективный катионный канал (TRP-подобный, Na⁺/K⁺). Сигмоидная активация с адаптацией (tau=3s). GHK-поток.","release":false}]})BIOCORE";
}
