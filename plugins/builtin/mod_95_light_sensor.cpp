// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// light_sensor.cpp — Фоторецептор (pineal/eye spot)
// Тип: MOD_LIGHT_SENSOR (95). GHK-версия: CNG-канал — неселективный катионный.
// Свет → снижение cGMP → закрытие канала → гиперполяризация (как в сетчатке).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double light_sensor_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);

    double intensity = m->state[1];  // 0..1, выставляется эмуляцией

    // Habituation (state[2]): контр-адаптация — аррестин/родопсин-киназа, tau_rise=2s, tau_decay=5s
    if (intensity > 0.1f) {
        sensor_habituation_step(m, 2, 1.0f, dt, 2000.0f);
    } else {
        m->state[2] -= dt * m->state[2] / 5000.0f;
    }
    if (m->state[2] < 0.0f) m->state[2] = 0.0f;
    if (m->state[2] > 1.0f) m->state[2] = 1.0f;
    // Sensitization (state[3]): со-адаптация — Ca²⁺/рековерин/гуанилатциклаза (GCAP), tau=60s
    sensor_sensitization_step(m, 3, intensity, dt, 60000.0f);
    double k_sens = (m->state[4] > 0.0f) ? m->state[4] : 0.5f;
    double gain = sensor_adaptation_gain(m->state[2], m->state[3], 0.7f, k_sens);
    float adapted_signal = intensity * gain;
    m->state[0] += (adapted_signal - m->state[0]) * dt / 50.0f;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    // CNG-канал: открыт в темноте (высокий cGMP), закрыт на свету
    // Неселективный катионный: Na⁺ 50% / K⁺ 30% / Ca²⁺ 20%
    double open_prob = 1.0f - m->state[0];  // инвертированная логика (свет → закрытие)

    double P_Na, P_K, P_Ca;
    ghk_partial_P(CNG_GAMMA_PS, 0.5f, 0.3f, 0.2f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area * open_prob;

    float I_Na = (float)(P_Na * ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]) * N * A_TO_PA);
    float I_K  = (float)(P_K  * ghk_monovalent(V, s->intra_ions[ION_K],  s->extra_ions[ION_K])  * N * A_TO_PA);
    float I_Ca = (float)(P_Ca * ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]) * N * A_TO_PA);

    float I_total = I_Na - I_K + I_Ca;

    s->intra_ions[ION_NA] -= (I_Na * dt) / cpm;
    s->intra_ions[ION_K]  -= (I_K  * dt) / cpm;
    s->intra_ions[ION_CA] -= (I_Ca * dt) / (2.0f * cpm);

    // Тепловой эффект от света
    s->temperature += intensity * 2.0f * dt * 0.001f;
    if (s->temperature > 45.0f) s->temperature = 45.0f;

    plugin_clamp_ions(s->intra_ions, ION_COUNT);

    // В темноте каналы открыты → деполяризация; на свету закрыты → гиперполяризация
    return -I_total;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_LIGHT_SENSOR, light_sensor_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Фоторецептор","version":"2.0.0","author":"BioNeuronCore","modifiers":[{"id":95,"name":"Фоторецептор (pineal/eye spot)","category":"Сенсоры","description":"Фоторецептор головастика (GHK-версия): CNG-канал (Na⁺ 50%/K⁺ 30%/Ca²⁺ 20%). Свет закрывает канал через cGMP-каскад → меньше катионного тока → гиперполяризация. В темноте канал открыт → деполяризация. Адаптация к освещению (tau=2s).","release":false}]})BIOCORE";
}
