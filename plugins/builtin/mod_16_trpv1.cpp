// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// trpv1.cpp — TRPV1-рецептор
// Тип: MOD_NT_RECEPTOR_TRPV1 (16)
// Категория: Ионотропные рецепторы
// Автогенерация: generate_builtin_plugins.py

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double trpv1_current_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — TRPV1 (Caterina 1997): температурный (>43°C) + капсаициновый сенсор.
    float area = effective_area(m, s);
    float V = s->V_m;
    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);

    // Температурная активация (Boltzmann вокруг 43°C).
    float f_T = 1.0f / (1.0f + expf((TRPV1_T_THRESHOLD_C - s->temperature) / TRPV1_T_SLOPE_C));

    // Капсаиновая активация.
    float cap = s->neurotransmitter[NT_CAPSAICIN];
    if (cap < 0.0f) cap = 0.0f;
    float f_cap = cap / (cap + TRPV1_EC50_CAP_REL);

    // Медленная активация r + десенситизация d.
    double r = m->state[0];
    float p_inf = f_T + f_cap - f_T * f_cap; // ИЛИ: тепло или капсаицин
    float r_inf = p_inf;
    r += (r_inf - r) * dt / TRPV1_TAU_ACT_MS;
    if (r < 0.0f) r = 0.0f; if (r > 1.0f) r = 1.0f;
    m->state[0] = r;

    double d = m->state[1];
    float d_inf = p_inf;
    d += (d_inf - d) * dt / TRPV1_TAU_DESENS_MS;
    if (d < 0.0f) d = 0.0f; if (d > 1.0f) d = 1.0f;
    m->state[1] = d;

    float open_prob = r * (1.0f - d);

    // GHK-ток (Na/K/Ca).
    double P_Na, P_K, P_Ca;
    ghk_partial_P(TRPV1_GAMMA_PS, TRPV1_P_NA_RATIO, TRPV1_P_K_RATIO, TRPV1_P_CA_RATIO, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    float I_Na = (float)(P_Na * ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]) * N * open_prob * A_TO_PA);
    float I_K  = (float)(P_K  * ghk_monovalent(V, s->intra_ions[ION_K],  s->extra_ions[ION_K])  * N * open_prob * A_TO_PA);
    float I_Ca = (float)(P_Ca * ghk_divalent(V,  s->intra_ions[ION_CA], s->extra_ions[ION_CA]) * N * open_prob * A_TO_PA);

    s->intra_ions[ION_NA] -= (I_Na * dt) / cpm;
    s->intra_ions[ION_K]  -= (I_K  * dt) / cpm;
    s->intra_ions[ION_CA] -= (I_Ca * dt) / (2.0f * cpm);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);

    return -(I_Na + I_K + I_Ca);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NT_RECEPTOR_TRPV1, trpv1_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"TRPV1-рецептор","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":16,"name":"TRPV1-рецептор","category":"Ионотропные рецепторы","description":"TRPV1 — ванилоидный рецептор, активируемый температурой и лигандами","release":false}]})BIOCORE";
}
