// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_165_trpm5.cpp — TRPM5 Ca²⁺-активируемый неселективный катионный канал.
// Тип: MOD_TRPM5 (165)
// Категория: Ca²⁺-активируемый неселективный
// Био: γ=25 пС; Po = Ca^n/(EC50^n + Ca^n), n=2.4, EC50=0.7 µМ; моновалентный Na⁺/K⁺
//      (Ca²⁺ — активатор, НЕ переносчик). Фармы нет.
// Источники: PMID 14634208, PMID 15670874, DOI 10.1073/pnas.2334159100.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double trpm5_current_impl(Modifier* m, Segment* s, float dt) {
    // TRPM5 — Ca²⁺-активируемый неселективный катионный канал (Na⁺/K⁺). Единый источник
    // логики — fallback trpm5_current (src/hh_biological.cpp): GHK Na⁺/K⁺, Po = Hill(Ca²⁺).
    float V = s->V_m;
    float area = effective_area(m, s);
    float Ca_uM = s->intra_ions[ION_CA] * MM_TO_UM;
    float ca_n = powf(Ca_uM, 2.4f);
    float Po = ca_n / (powf(0.7f, 2.4f) + ca_n);  // EC50 0.7 µМ, Hill 2.4

    float gamma_ps = 25.0f;
    double P_Na, P_K, P_Ca;
    ghk_partial_P(25.0f, 1.0f, 1.0f, 0.0f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double flux_K = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    float I_Na = (float)(P_Na * flux_Na * N * Po * A_TO_PA);
    float I_K  = (float)(P_K * flux_K * N * Po * A_TO_PA);

    float vol = segment_volume(s);
    float cpv = charge_per_mole(vol);
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpv; s->extra_ions[ION_NA] += (I_Na * dt) / (cpv * EXTRA_VOLUME_RATIO);
    s->intra_ions[ION_K]  -= (I_K  * dt) / cpv; s->extra_ions[ION_K]  += (I_K  * dt) / (cpv * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);

    return -(I_Na + I_K);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_TRPM5, trpm5_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"TRPM5 Ca2+-activated nonselective cation","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":165,"name":"TRPM5 Ca2+-activated nonselective cation","category":"Ca2+-activated nonselective","description":"TRPM5: Ca2+-activated nonselective cation channel (Na+/K+, not Ca2+-conducting). gamma=25 pS, Ca2+-dependent Po (EC50 0.7 uM, Hill 2.4). No pharmacology. GHK Na/K (1:1).","release":false}]})BIOCORE";
}
