// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_209_kca31.cpp — KCa3.1 (KCNN4/SK4/IK) Ca²⁺-активируемый K⁺ канал.
// Тип: MOD_KCA31 (209). γ=25 пС, Ca²⁺ EC50=188 нМ, voltage-independent, TRAM-34 20 нМ.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double kca31_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double open_prob = m->state[0];
    double gamma_ps = (m->state[3] > 0.0f) ? m->state[3] : KCA31_GAMMA_PS;
    double tau_open = (m->state[4] > 0.0f) ? m->state[4] : KCA31_TAU_OPEN_MS;
    double tau_close = (m->state[5] > 0.0f) ? m->state[5] : KCA31_TAU_CLOSE_MS;
    float ca = s->intra_ions[ION_CA];
    float p_inf = kca31_p_inf(ca);
    float tau = (p_inf > open_prob) ? tau_open : tau_close;
    open_prob += (p_inf - open_prob) * dt / tau;
    if (open_prob < 0.0f) open_prob = 0.0f;
    if (open_prob > 1.0f) open_prob = 1.0f;
    m->state[0] = open_prob;
    // TRAM-34 блок (state[6], нМ)
    double tram = (m->state[6] > 0.0f) ? m->state[6] : 0.0f;
    open_prob /= (1.0f + tram / KCA31_TRAM34_IC50_NM);
    float K_i = s->intra_ions[ION_K], K_o = s->extra_ions[ION_K];
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    float I_K = (float)(I_A * A_TO_PA);
    float vol = segment_volume(s);
    float cpv = FARADAY_CONST * vol / 1000.0f;
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);
    return -I_K;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_KCA31, kca31_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"KCa3.1 Ca-activated K","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":209,"name":"KCa3.1 (KCNN4)","category":"Ионные каналы","description":"KCa3.1 Ca²⁺-активируемый K⁺ (γ=25 пС, EC50=188 нМ, voltage-independent). GHK K⁺, TRAM-34 20 нМ.","release":false}]})BIOCORE";
}
