// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// synaptic_release.cpp — Ca²⁺-активируемый K⁺ ток (SK-канал, K_Ca2.x)
// Тип: MOD_SYNAPTIC_RELEASE (5)
// Категория: Базовые модификаторы
// Автогенерация: generate_builtin_plugins.py

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double synaptic_release_current_impl(Modifier* m, Segment* s, float dt) {
    float area = plugin_segment_area(s);

    // Ca²⁺-сенсор (Hill n=2, K_d ≈ 0.5 μM)
    float ca = s->intra_ions[ION_CA];
    float ca_sensor = (ca * ca) / (ca * ca + 0.0005f * 0.0005f);

    // Параметрические переопределения
    double p_fac_tau = (m->state[4] > 0.0f) ? m->state[4] : 100.0f;
    double p_dep_tau = (m->state[5] > 0.0f) ? m->state[5] : 500.0f;

    // Гейтинг: Ca_sensor → facilitation → depression
    m->state[0] += (ca_sensor - m->state[0]) * dt / 1.0f;
    m->state[1] += (m->state[0] - m->state[1]) * dt / p_fac_tau;
    m->state[2] += (m->state[0] - m->state[2]) * dt / p_dep_tau;

    double open_prob = m->state[1] / (1.0f + m->state[2]);

    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);
    double P_single = ghk_single_channel_P(SK_GAMMA_PS, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(s->V_m, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double N = m->density * area;
    float I_K = (float)(P_single * flux * N * open_prob * A_TO_PA);
    s->intra_ions[ION_K] -= (I_K * dt) / cpm;
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    return -I_K;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_SYNAPTIC_RELEASE, synaptic_release_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"SK Ca²⁺-активируемый K⁺","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":5,"name":"SK Ca²⁺-активируемый K⁺","category":"Базовые модификаторы","description":"Ca²⁺-активируемый K⁺ канал (SK/K_Ca2.x). Hill=2 сенсор Ca²⁺ (K_d=0.5μM). AHP-обратная связь. Моделирует пресинаптический Ca²⁺-сенсор.","release":false}]})BIOCORE";
}
