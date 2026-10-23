// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_210_trpc3.cpp — TRPC3 неселективный катионный канал (DAG-активируемый).
// Тип: MOD_TRPC3 (210). γ=60 пС, P_Ca:P_Na=1.62, Pyr3 0.7 µM.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double trpc3_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double dag = m->state[4];  // DAG (0–1)
    if (dag < 0.0f) dag = 0.0f; if (dag > 1.0f) dag = 1.0f;
    float Po = dag;
    double pyr3 = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;  // Pyr3 (µM)
    Po /= (1.0f + pyr3 / PYR3_IC50_TRPC3);
    double P_Na, P_K, P_Ca;
    ghk_partial_P(TRPC3_GAMMA_PS, 1.0f, 0.0f, TRPC3_PCA_PNA, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double flux_Ca = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    double I_Na = P_Na * flux_Na * N * Po * A_TO_PA;
    double I_Ca = P_Ca * flux_Ca * N * Po * A_TO_PA;
    float vol = segment_volume(s);
    float cpv = FARADAY_CONST * vol / 1000.0f;
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpv; s->extra_ions[ION_NA] += (I_Na * dt) / (cpv * EXTRA_VOLUME_RATIO);
    s->intra_ions[ION_CA] -= (I_Ca * dt) / cpv; s->extra_ions[ION_CA] += (I_Ca * dt) / (cpv * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->extra_ions, ION_COUNT); plugin_clamp_ions(s->intra_ions, ION_COUNT);
    return -(I_Na + I_Ca);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_TRPC3, trpc3_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"TRPC3 nonselective cation","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":210,"name":"TRPC3","category":"Ионные каналы","description":"TRPC3 неселективный катионный (γ=60 пС, P_Ca:P_Na=1.62, DAG-активируемый). GHK Na+Ca, Pyr3 0.7 µM.","release":false}]})BIOCORE";
}
