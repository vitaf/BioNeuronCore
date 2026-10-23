// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// kv34.cpp — Kv3.4 A-type K⁺ канал
// Тип: MOD_KV34 (159)

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double kv34_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float gamma_ps = 20.0f;

    float m_inf = 1.0f / (1.0f + expf(-(V - 19.0f) / 11.3f));
    float tau_m = kv34_m_tau(V);
    m->state[0] += (m_inf - m->state[0]) * dt / tau_m;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    float h_inf = 1.0f / (1.0f + expf((V + 15.0f) / 7.4f));
    float tau_h = kv34_h_tau(V);
    m->state[1] += (h_inf - m->state[1]) * dt / tau_h;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    float pkc = segment_pkc_act(s);
    double h_eff = m->state[1] + (1.0f - m->state[1]) * pkc * PKC_KV34_DISINHIBIT;

    // C-type медленная инактивация (общая для Kv3.x, τ~2 с, V½≈−30 мВ) — Klemic 1998
    float hs_inf = 1.0f / (1.0f + expf((V + 30.0f) / 6.0f));
    float tau_hs = 2000.0f;
    m->state[2] += (hs_inf - m->state[2]) * dt / tau_hs;
    if (m->state[2] < 0.0f) m->state[2] = 0.0f;
    if (m->state[2] > 1.0f) m->state[2] = 1.0f;

    double open_prob = m->state[0] * h_eff * m->state[2];  // m·h_fast(N-ball)·h_slow(C-type)

    double ap4 = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    double tea = (m->state[6] > 0.0f) ? m->state[6] : 0.0f;
    open_prob /= (1.0f + ap4 / AP4_IC50_KV34) * (1.0f + tea / TEA_IC50_KV34);

    float K_i = s->intra_ions[ION_K];
    float K_o = s->extra_ions[ION_K];
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    float I_K = (float)(I_A * A_TO_PA);

    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);
    s->intra_ions[ION_K] -= (I_K * dt) / cpm;
    s->extra_ions[ION_K] += (I_K * dt) / (cpm * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);

    return -I_K;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_KV34, kv34_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Kv3.4 A-type K+","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":159,"name":"Kv3.4 A-type K+","category":"Voltage-gated K","description":"Kv3.4 (KCNC4) A-type K+. m (V1/2 +19 mV) + N-ball fast inactivation (V1/2 -15 mV). PKC dis-inhibition (removes N-ball). 4-AP/TEA. GHK K+ (z=1).","release":false}]})BIOCORE";
}
