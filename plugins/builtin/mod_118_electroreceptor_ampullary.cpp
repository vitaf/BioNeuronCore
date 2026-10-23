// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// electroreceptor_ampullary.cpp — Ампулярный электрорецептор (ампулы Лоренцини)
// Тип: MOD_ELECTRORECEPTOR_AMPULLARY (118). GHK-версия.
// Детекция слабых электрических полей (DC — 10 Hz) через гель-заполненный канал.
// Направленное тюнингование + low-pass фильтрация + Ca²⁺-адаптация.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double electroreceptor_ampullary_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);

    double sensitivity = (m->active_vars >= 1 && m->state[m->active_vars] > 0.0f)
                        ? m->state[m->active_vars] : 4.0f;

    // state[0] = P_open MET-каналов
    // state[1] = V_ext — внешнее напряжение (нормированное, -1..1)
    double V_ext = m->state[1];
    if (V_ext < -1.0f) V_ext = -1.0f;
    if (V_ext > 1.0f) V_ext = 1.0f;

    // Направленное тюнингование: эффективное напряжение
    double cos_pref = (m->state[5] > -99.0f) ? m->state[5] : 1.0f;
    if (cos_pref < -1.0f) cos_pref = -1.0f;
    if (cos_pref > 1.0f) cos_pref = 1.0f;
    float V_eff = V_ext * cos_pref;

    // Low-pass фильтр канала (RC-цепь: τ≈100ms)
    m->state[3] += (V_eff - m->state[3]) * dt / 100.0f;

    // Ca²⁺-зависимая адаптация (τ=5s)
    double adapt_target = fabs(m->state[3]);
    m->state[2] += (adapt_target - m->state[2]) * dt / 5000.0f;
    if (m->state[2] < 0.0f) m->state[2] = 0.0f;
    if (m->state[2] > 1.0f) m->state[2] = 1.0f;
    double adapted_V = m->state[3] * (1.0f - 0.8f * m->state[2]);

    // Сигмоидная активация MET-каналов
    float target_open = 0.15f + 0.85f / (1.0f + expf(-sensitivity * (adapted_V - 0.1f) * 15.0f));
    m->state[0] += (target_open - m->state[0]) * dt / 2.0f;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    // Неселективный катионный канал: Na⁺ 50%, K⁺ 35%, Ca²⁺ 15%
    double P_Na, P_K, P_Ca;
    ghk_partial_P(MET_GAMMA_PS, 0.50f, 0.35f, 0.15f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area * m->state[0];
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
    register_modifier_current(MOD_ELECTRORECEPTOR_AMPULLARY, electroreceptor_ampullary_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Ампулярный электрорецептор","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":118,"name":"Ампулярный электрорецептор — ампулы Лоренцини","category":"Сенсорные рецепторы","description":"Электрорецепция через ампулы Лоренцини. Гель-заполненный канал (low-pass фильтр, tau=100ms). Внешнее E-поле создаёт трансэпителиальный потенциал (V=E*L*cos(theta)). MET-каналы: неселективные катионные (Na+ 50%, K+ 35%, Ca2+ 15%). Ca2+-адаптация (tau=5s). Направленное тюнингование. GHK-поток.","release":false}]})BIOCORE";
}
