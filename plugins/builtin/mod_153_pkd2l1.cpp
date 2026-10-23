// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// pkd2l1.cpp — PKD2L1 (TRPP3) Ca²⁺-предпочитающий механосенсор CSF
// Тип: MOD_PKD2L1 (153)
// Категория: Mechanosensitive Ca-preferring

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double pkd2l1_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float gamma_ps = 150.0f;  // γ 120–200 пС (PKD2L1)

    // Механо-активация: stim = state[4] (CSF-поток), по умолчанию 0
    double stim = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;
    // Механо-адаптация (инактивация при длительном стимуле, τ~1 с)
    float adapt_inf = stim;
    float tau_a = 1000.0f;
    m->state[1] += (adapt_inf - m->state[1]) * dt / tau_a;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;
    double Po = stim * (1.0f - m->state[1]);  // stim·(1−adapt)
    if (Po <= 0.0f) return 0.0f;

    // Ca²⁺-предпочитающий (P_Ca:P_Na ≈ 5:1)
    double P_Na, P_K, P_Ca;
    ghk_partial_P(150.0f, 0.17f, 0.0f, 0.83f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Ca = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    float Ica = (float)(P_Ca * flux_Ca * N * Po * A_TO_PA);
    float Ina = (float)(P_Na * flux_Na * N * Po * A_TO_PA);

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
    register_modifier_current(MOD_PKD2L1, pkd2l1_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"PKD2L1 (TRPP3) mechanosensitive Ca-preferring","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":153,"name":"PKD2L1 (TRPP3)","category":"Mechanosensitive Ca-preferring","description":"Ca2+-preferring mechanosensitive cation channel (PKD2L1/TRPP3). CSF-flow sensor. PCa/PNa~4-6, gamma=120-200 pS. Po=state[4] (mechanical stimulus). Mixed GHK Ca (z=2) + Na (z=1).","release":false}]})BIOCORE";
}
