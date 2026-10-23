// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_179_trpc67.cpp — TRPC6/7 DAG-гейт
// Тип: MOD_TRPC67 (179). Категория: TRP channel
// Био: TRPC6/7 — канал фотопроводимости ipRGC, DAG-гейт (Perez-Leighton 2011).
// GHK-парадигма (γ≈35 пС), P_Ca/P_Na≈5 (0.83 Ca²⁺ / 0.17 Na⁺).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double trpc67_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    // DAG (state[4], Gq/PLC) — активация r = lig/(lig+10), τ=50 мс
    double lig = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;
    float r = lig / (lig + 10.0f);
    m->state[0] += (r - m->state[0]) * (dt / 50.0f);
    if (m->state[0] < 0.0f) m->state[0] = 0.0f; if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    double open = m->state[0];

    double P_Na, P_K, P_Ca;
    ghk_partial_P(TRPC67_GAMMA_PS, 0.17f, 0.0f, 0.83f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Ca = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    float Ica = (float)(P_Ca * flux_Ca * N * open * A_TO_PA);
    float Ina = (float)(P_Na * flux_Na * N * open * A_TO_PA);

    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);
    s->intra_ions[ION_CA] -= (Ica * dt) / (2.0f * cpm);
    s->intra_ions[ION_NA] -= (Ina * dt) / cpm;
    s->extra_ions[ION_CA] += (Ica * dt) / (2.0f * cpm * EXTRA_VOLUME_RATIO);
    s->extra_ions[ION_NA] += (Ina * dt) / (cpm * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);

    return -(Ica + Ina);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_TRPC67, trpc67_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"TRPC6/7 DAG-гейт канал","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":179,"name":"TRPC6/7 DAG-гейт","category":"TRP channel","description":"TRPC6/7 — канал фотопроводимости ipRGC, DAG-гейт (state[4], τ=50 мс). γ≈35 пС, P_Ca/P_Na≈5 (0.83 Ca²⁺ z=2 / 0.17 Na⁺ z=1). GHK. Perez-Leighton 2011.","release":false}]})BIOCORE";
}
