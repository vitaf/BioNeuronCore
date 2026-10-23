// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// magnetite_mechanoreceptor.cpp — Магнетитовый механорецептор (компасная ориентация)
// Тип: MOD_MAGNETITE_MECHANORECEPTOR (119). GHK-версия.
// Цепочки биогенного магнетита (Fe₃O₄) → крутящий момент τ = m × B →
// механическое открытие MET-каналов → деполяризация.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double magnetite_mechanoreceptor_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);

    double sensitivity = (m->active_vars >= 1 && m->state[m->active_vars] > 0.0f)
                        ? m->state[m->active_vars] : 3.0f;

    // state[0] = P_open
    // state[1] = B_norm — норма магнитного поля (0..1)
    double B_norm = m->state[1];
    if (B_norm < 0.0f) B_norm = 0.0f;
    if (B_norm > 1.0f) B_norm = 1.0f;

    // Крутящий момент: |τ| = |m × B| ∝ sin(θ_B) × |B|
    double sin_theta = sqrtf(1.0f - m->state[3] * m->state[3]);
    if (sin_theta < 0.0f) sin_theta = 0.0f;
    float torque_factor = B_norm * sin_theta * sin_theta;

    // Механотрансдукция: torque → MET P_open
    float target_open = 1.0f / (1.0f + expf(-sensitivity * (torque_factor - 0.05f) * 20.0f));
    m->state[0] += (target_open - m->state[0]) * dt / 5.0f;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    // Ca²⁺-адаптация (τ=8s)
    m->state[2] += (m->state[0] - m->state[2]) * dt / 8000.0f;
    if (m->state[2] < 0.0f) m->state[2] = 0.0f;
    if (m->state[2] > 1.0f) m->state[2] = 1.0f;
    double eff_open = m->state[0] * (1.0f - 0.5f * m->state[2]);

    // Неселективный катионный канал: Na⁺ 45%, K⁺ 30%, Ca²⁺ 25%
    double P_Na, P_K, P_Ca;
    ghk_partial_P(MET_GAMMA_PS, 0.45f, 0.30f, 0.25f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area * eff_open;
    float I_Na = (float)(P_Na * ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]) * N * A_TO_PA);
    float I_K  = (float)(P_K  * ghk_monovalent(V, s->intra_ions[ION_K],  s->extra_ions[ION_K])  * N * A_TO_PA);
    float I_Ca = (float)(P_Ca * ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]) * N * A_TO_PA);
    float I_total = I_Na - I_K + I_Ca;

    s->intra_ions[ION_NA] -= (I_Na * dt) / cpm;
    s->intra_ions[ION_K]  -= (I_K  * dt) / cpm;
    s->intra_ions[ION_CA] -= (I_Ca * dt) / (2.0f * cpm);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);

    return -I_total;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_MAGNETITE_MECHANORECEPTOR, magnetite_mechanoreceptor_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Магнетитовый механорецептор","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":119,"name":"Магнетитовый механорецептор — Fe3O4/компас","category":"Сенсорные рецепторы","description":"Магниторецепция через биогенный магнетит (Fe3O4). Цепочки кристаллов (~10^6 шт) создают магнитный момент m. Внешнее поле B создаёт крутящий момент (tau=m*B), который открывает механочувствительные каналы (TRP-подобные). Чувствительность ~10-100 nT. Каналы: Na+ 45%, K+ 30%, Ca2+ 25%. Ca2+-адаптация (tau=8s). GHK-поток.","release":false}]})BIOCORE";
}
