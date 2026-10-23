// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// metabotropic.cpp — Метаботропный GPCR Gi/o каскад (общего назначения)
// Тип: MOD_METABOTROPIC (3)
// Категория: Базовые модификаторы
// Автогенерация: generate_builtin_plugins.py

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double metabotropic_current_impl(Modifier* m, Segment* s, float dt) {
    float area = plugin_segment_area(s);

    // Суммарная активация всеми нейромедиаторами
    float total_nt = 0.0f;
    for (int n = 0; n < NT_COUNT; n++) total_nt += s->neurotransmitter[n];

    // Параметрические переопределения
    double p_coupling = (m->state[2] > 0.0f) ? m->state[2] : 1.0f;
    double p_tau = (m->state[3] > 0.0f) ? m->state[3] : 100.0f;

    // Активация G-белка (state[0]) → effector (state[1])
    float activation = total_nt / (total_nt + 1.0f);
    m->state[0] += (activation - m->state[0]) * dt / p_tau;
    m->state[1] = p_coupling * m->state[0];

    // GIRK K⁺ ток — GHK (как fallback)
    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);
    double P_single = ghk_single_channel_P(GIRK_GAMMA_PS, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(s->V_m, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double N = m->density * area;
    double I_K = (float)(P_single * flux * N * m->state[1] * A_TO_PA);
    s->intra_ions[ION_K] -= (I_K * dt) / cpm;
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    return -I_K;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_METABOTROPIC, metabotropic_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Метаботропный Gi/o","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":3,"name":"Метаботропный Gi/o","category":"Базовые модификаторы","description":"Универсальный Gi/o-связанный GPCR. Суммарная активация нейромедиаторами → G-белок → GIRK K⁺ каналы → гиперполяризация. Параметры: coupling, tau.","release":false}]})BIOCORE";
}
