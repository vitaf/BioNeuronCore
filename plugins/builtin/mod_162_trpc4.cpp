// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// trpc4.cpp — TRPC4 неселективный катионный канал
// Тип: MOD_TRPC4 (162)
// Категория: TRP Cation

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

// TRPC4: Gq/PLC-зависимый неселективный катионный (Na⁺/Ca²⁺, PCa/PNa≈4).
// γ=40 пС; Po=state[4] (Gq 0–1); ML204 (state[5], µМ) блокирует по IC50=2 µМ.
// GHK Na⁺ (z=1) + Ca²⁺ (z=2, P_base×4); return -(I_Na + I_Ca).
// Источники: PMID 21795696, 29385160.
static double trpc4_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double gq = m->state[4];  // Gq/PLC (0–1)
    if (gq < 0.0f) gq = 0.0f; if (gq > 1.0f) gq = 1.0f;
    float Po = gq;
    double ml = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;  // ML204 (µМ)
    Po /= (1.0f + ml / ML204_IC50_TRPC4);

    float gamma_ps = 40.0f;
    double P_Na, P_K, P_Ca;
    ghk_partial_P(40.0f, 1.0f, 0.0f, 4.0f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double flux_Ca = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    float I_Na = (float)(P_Na * flux_Na * N * Po * A_TO_PA);
    float I_Ca = (float)(P_Ca * flux_Ca * N * Po * A_TO_PA);

    float vol = segment_volume(s);
    float cpv = charge_per_mole(vol);
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpv; s->extra_ions[ION_NA] += (I_Na * dt) / (cpv * EXTRA_VOLUME_RATIO);
    s->intra_ions[ION_CA] -= (I_Ca * dt) / cpv; s->extra_ions[ION_CA] += (I_Ca * dt) / (cpv * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);

    return -(I_Na + I_Ca);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_TRPC4, trpc4_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"TRPC4 non-selective cation","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":162,"name":"TRPC4 non-selective cation","category":"TRP Cation","description":"TRPC4 non-selective cation (Na+/Ca2+, PCa/PNa~4). Gq/PLC-dependent (Po=state[4] 0-1). ML204 block (IC50 2 uM). gamma=40 pS. GHK Na+ (z=1) + Ca2+ (z=2, x4).","release":false}]})BIOCORE";
}
