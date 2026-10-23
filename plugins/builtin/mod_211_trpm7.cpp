// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_211_trpm7.cpp — TRPM7 chanzyme (Mg²⁺/Ca²⁺-проницаемый канал+киназа).
// Тип: MOD_TRPM7 (211). γ=40 пС, P_Ca/P_Na=0.34, Mg²⁺(внутр) IC50=0.7 мМ, NS8593 1.6 µM.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double trpm7_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    // Mg²⁺-ингибиция (внутр. Mg²⁺, IC50 0.7 мМ, Hill 2)
    float Mg_i = s->intra_ions[ION_MG];
    float r = Mg_i / TRPM7_MG_IC50_MM;
    float Po = 1.0f / (1.0f + r * r);
    double ns8593 = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;  // NS8593 (µM)
    Po /= (1.0f + ns8593 / NS8593_IC50_TRPM7);
    double P_Na, P_K, P_Ca;
    ghk_partial_P(TRPM7_GAMMA_PS, 1.0f, 0.0f, TRPM7_PCA_PNA, &P_Na, &P_K, &P_Ca);
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
    register_modifier_current(MOD_TRPM7, trpm7_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"TRPM7 chanzyme","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":211,"name":"TRPM7","category":"Ионные каналы","description":"TRPM7 chanzyme (γ=40 пС, P_Ca/P_Na=0.34, Mg²⁺-ингибируемый). GHK Na+Ca, NS8593 1.6 µM.","release":false}]})BIOCORE";
}
