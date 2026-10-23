// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_164_trpm4.cpp — TRPM4 Ca²⁺-активируемый неселективный катионный канал.
// Тип: MOD_TRPM4 (164)
// Категория: Ca²⁺-активируемый неселективный (Na⁺/K⁺)
// Био: γ=25 пС; Ca²⁺-зависимый Po (EC50 0.4 µM); моновалентный Na⁺/K⁺ (не Ca).
// Фарма: 9-phenanthrol (state[5], µM), IC50=18 µM.
// Источники: PMID 12015988, 15670874, 23416167.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double trpm4_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    // Ca²⁺-зависимый Po: Po = Ca_uM / (Ca_uM + EC50), EC50 = 0.4 µM.
    float Ca_uM = s->intra_ions[ION_CA] * 1000.0f;
    float Po = Ca_uM / (Ca_uM + 0.4f);

    // 9-phenanthrol (state[5], µM) — блокада по IC50.
    double phen = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    Po /= (1.0f + phen / PHENANTHROL_IC50_TRPM4);

    float gamma_ps = 25.0f;  // γ = 25 пС
    double P_Na, P_K, P_Ca;
    ghk_partial_P(25.0f, 1.0f, 1.0f, 0.0f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double flux_K = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    float I_Na = (float)(P_Na * flux_Na * N * Po * A_TO_PA);
    float I_K  = (float)(P_K * flux_K * N * Po * A_TO_PA);

    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpm;
    s->intra_ions[ION_K]  -= (I_K  * dt) / cpm;
    s->extra_ions[ION_NA] += (I_Na * dt) / (cpm * EXTRA_VOLUME_RATIO);
    s->extra_ions[ION_K]  += (I_K  * dt) / (cpm * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);

    return -(I_Na + I_K);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_TRPM4, trpm4_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"TRPM4 Ca2+-activated nonselective cation","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":164,"name":"TRPM4 Ca2+-activated nonselective cation","category":"Ca2+-activated nonselective","description":"TRPM4 Ca2+-activated nonselective cation channel. Monovalent Na+/K+ (not Ca2+-conducting). Ca2+-dependent Po (EC50 0.4 uM). gamma=25 pS. 9-phenanthrol block (IC50 18 uM). GHK Na/K (z=1, equal P_base). PMID 12015988, 15670874, 23416167.","release":false}]})BIOCORE";
}
