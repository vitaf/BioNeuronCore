// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// olfactory_receptor.cpp — Обонятельный рецептор (GPCR Gs)
// Тип: MOD_OLFACTORY_RECEPTOR (108). GHK-версия.
// Связывание лиганда (глутамат) → Gs-каскад → cAMP↑ → закрытие K⁺-каналов → деполяризация.
// Активация пропорциональна концентрации лиганда в среде.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double olfactory_receptor_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);

    // Параметры через state[] (с оверрайдом через active_vars)
    double EC50 = (m->active_vars >= 1 && m->state[m->active_vars] > 0.0f)
                 ? m->state[m->active_vars] : 0.01f;
    double sensitivity = (m->active_vars >= 2 && m->state[m->active_vars + 1] > 0.0f)
                        ? m->state[m->active_vars + 1] : 2.0f;

    // Лиганд = глутамат (state[1] выставляется сенсором)
    double ligand = m->state[1];
    if (ligand < 0.0f) ligand = 0.0f;
    if (ligand > 1.0f) ligand = 1.0f;

    // Сигмоидная активация: state[0] = open probability
    float activation = 1.0f / (1.0f + expf(-sensitivity * (ligand - EC50) * 100.0f));
    m->state[0] += (activation - m->state[0]) * dt / 50.0f;  // tau=50ms
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    // Habituation (state[2]): GRK3/β-arrestin2 + Ca²⁺/CaM→PDE→cAMP↓, tau=2s
    sensor_habituation_step(m, 2, m->state[0], dt, 2000.0f);
    if (m->state[2] < 0.0f) m->state[2] = 0.0f;
    if (m->state[2] > 1.0f) m->state[2] = 1.0f;
    // Sensitization (state[3]): cAMP/PKA priming при слабой активации, tau=10s
    sensor_sensitization_step(m, 3, m->state[0], dt, 10000.0f);
    double k_sens = (m->state[4] > 0.0f) ? m->state[4] : 0.3f;
    double gain = sensor_adaptation_gain(m->state[2], m->state[3], 0.7f, k_sens);
    double effective_act = m->state[0] * gain;

    // Gs-механизм: закрытие K⁺-каналов → снижение K⁺ выхода → деполяризация
    // Базовая K⁺ проводимость модулируется: g_K_eff = g_K_basal * (1 - effective_act)
    float K_i = s->intra_ions[ION_K];  // моль/м³
    float K_o = s->extra_ions[ION_K];

    double P_single = ghk_single_channel_P(GIRK_GAMMA_PS, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, K_i, K_o);
    double N = m->density * area;
    float I_K_basal = (float)(P_single * flux * N * A_TO_PA);
    float I_K_modulated = (float)(P_single * (1.0f - effective_act * 0.8f) * flux * N * A_TO_PA);

    // Разница = снижение K⁺ выхода = эффективный деполяризующий ток
    float I_effect = I_K_basal - I_K_modulated;  // positive = less K+ efflux = depolarizing

    s->intra_ions[ION_K] -= (I_effect * dt) / cpm;
    plugin_clamp_ions(s->intra_ions, ION_COUNT);

    return -I_effect;  // convention: positive return = depolarizing
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_OLFACTORY_RECEPTOR, olfactory_receptor_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Обонятельный рецептор (Gs)","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":108,"name":"Обонятельный рецептор (GPCR Gs)","category":"Сенсорные рецепторы","description":"Обонятельный рецептор (GPCR Gs-тип). Лиганд = глутамат (аминокислоты = запах еды). Связывание → Gs-каскад → cAMP↑ → закрытие K⁺-каналов → деполяризация. Сигмоидная активация с адаптацией (tau=2s). GHK-поток K⁺.","release":false}]})BIOCORE";
}
