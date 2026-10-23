// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_163_trpc5.cpp — TRPC5 неселективный катионный канал (Gq + Ca²⁺-зависимый)
// Тип: MOD_TRPC5 (163)
// Категория: Nonselective cation (Gq/Ca²⁺-activated)

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double trpc5_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    // Gq/PLC-сопряжение (state[4], 0–1)
    double gq = m->state[4];
    if (gq < 0.0f) gq = 0.0f;
    if (gq > 1.0f) gq = 1.0f;

    // Ca²⁺-зависимая активация: ca_act = [Ca²⁺]ᵢ / ([Ca²⁺]ᵢ + EC50), EC50 = 0.5 µМ
    float Ca_uM = s->intra_ions[ION_CA] * 1000.0f;
    float ca_act = Ca_uM / (Ca_uM + 0.5f);
    float Po = gq * ca_act;

    // Фармакология: HC-070 (state[5], нМ), IC50 = 9.3 нМ
    double hc = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    Po /= (1.0f + hc / HC070_IC50_TRPC5);

    // γ = 50 пС; смешанный Na⁺/Ca²⁺ (PCa/PNa ≈ 5)
    float gamma_ps = 50.0f;
    double P_Na, P_K, P_Ca;
    ghk_partial_P(50.0f, 1.0f, 0.0f, 5.0f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double flux_Ca = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    float I_Na = (float)(P_Na * flux_Na * N * Po * A_TO_PA);
    float I_Ca = (float)(P_Ca * flux_Ca * N * Po * A_TO_PA);

    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpm;
    s->extra_ions[ION_NA] += (I_Na * dt) / (cpm * EXTRA_VOLUME_RATIO);
    s->intra_ions[ION_CA] -= (I_Ca * dt) / cpm;
    s->extra_ions[ION_CA] += (I_Ca * dt) / (cpm * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);

    return -(I_Na + I_Ca);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_TRPC5, trpc5_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"TRPC5 nonselective cation (Gq+Ca2+ dependent)","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":163,"name":"TRPC5 nonselective cation","category":"Nonselective cation (Gq+Ca2+)","description":"TRPC5 nonselective cation channel. gamma=50 pS (41-66). Gq-dependent (state[4]) + Ca2+-dependent (EC50 0.5 uM). Na+/Ca2+ (PCa/PNa~5). HC-070 IC50 9.3 nM (state[5], nM). Mixed GHK Na (z=1) + Ca (z=2).","release":false}]})BIOCORE";
}
