// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// hunger_sensor.cpp — Сенсор голода (ATP-монитор)
// Тип: MOD_HUNGER_SENSOR (94). GHK-версия: Na⁺/K⁺-неселективный катионный канал.
// Активируется при падении ATP → деполяризация через прямой Na⁺ вход.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double hunger_sensor_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);

    double atp_threshold = (m->active_vars >= 1 && m->state[m->active_vars] != 0.0f)
                          ? m->state[m->active_vars] : 0.3f;
    double sensitivity = (m->active_vars >= 2 && m->state[m->active_vars + 1] > 0.0f)
                        ? m->state[m->active_vars + 1] : 10.0f;

    float atp_norm = s->ATP_level / 5.0f;  // мМ (0..5) → нормировка (как mTORC1)
    if (atp_norm > 1.0f) atp_norm = 1.0f;
    if (atp_norm < 0.0f) atp_norm = 0.0f;

    float hunger = 1.0f / (1.0f + expf(sensitivity * (atp_norm - atp_threshold)));
    m->state[0] += (hunger - m->state[0]) * dt / 500.0f;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    // Habituation (state[1]): медленная контр-адаптация, tau=300s — AMPK/mTOR-десенситизация
    sensor_habituation_step(m, 1, m->state[0], dt, 300000.0f);
    // Sensitization (state[2]): очень медленная со-адаптация, tau=600s — грелин/лептин priming
    sensor_sensitization_step(m, 2, m->state[0], dt, 600000.0f);
    double k_hab = (m->state[3] > 0.0f) ? m->state[3] : 0.5f;
    double k_sens = (m->state[4] > 0.0f) ? m->state[4] : 0.4f;
    double gain = sensor_adaptation_gain(m->state[1], m->state[2], k_hab, k_sens);

    // GHK: неселективный катионный канал (Na⁺ 60% / K⁺ 30% / Ca²⁺ 10%)
    double P_Na, P_K, P_Ca;
    ghk_partial_P(TRP_SENSOR_GAMMA_PS, 0.6f, 0.3f, 0.0f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area * m->state[0] * gain;

    float I_Na = (float)(P_Na * ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]) * N * A_TO_PA);
    float I_K  = (float)(P_K  * ghk_monovalent(V, s->intra_ions[ION_K],  s->extra_ions[ION_K])  * N * A_TO_PA);

    s->intra_ions[ION_NA] -= (I_Na * dt) / cpm;
    s->intra_ions[ION_K]  -= (I_K  * dt) / cpm;
    plugin_clamp_ions(s->intra_ions, ION_COUNT);

    return -(I_Na - I_K);  // Na⁺ вход доминирует → деполяризация
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_HUNGER_SENSOR, hunger_sensor_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Сенсор голода","version":"2.0.0","author":"BioNeuronCore","modifiers":[{"id":94,"name":"Сенсор голода (ATP)","category":"Сенсоры","description":"Интероцептивный сенсор голода (GHK-версия). Неселективный катионный канал (Na⁺ 60%/K⁺ 30%/Ca²⁺ 10%). Активируется при падении ATP ниже порога. Сигмоида: голод=1/(1+exp(s*(ATP-thr))). Прямое движение Na⁺ внутрь и K⁺ наружу через GHK.","release":false}]})BIOCORE";
}
