// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// kv11.cpp — Kv1.1 delayed-rectifier K⁺ канал
// Тип: MOD_KV11 (156)

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double kv11_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float Vhalf = -30.0f;
    float k     = 6.0f;
    float gamma_ps = 10.0f;  // γ 9–14 пС

    float m_inf = 1.0f / (1.0f + expf(-(V - Vhalf) / (k + 1e-6f)));
    float tau_m = 5.0f;
    m->state[0] += (m_inf - m->state[0]) * dt / tau_m;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    float h_inf = 1.0f / (1.0f + expf((V + 40.0f) / 6.0f));
    float tau_h = 3000.0f;
    m->state[1] += (h_inf - m->state[1]) * dt / tau_h;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    double open_prob = m->state[0] * m->state[0] * m->state[0] * m->state[0] * m->state[1];

    double dtx = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    double tea = (m->state[6] > 0.0f) ? m->state[6] : 0.0f;
    open_prob /= (1.0f + dtx / DTX_IC50_KV11) * (1.0f + tea / TEA_IC50_KV1X);

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
    register_modifier_current(MOD_KV11, kv11_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Kv1.1 delayed-rectifier K+","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":156,"name":"Kv1.1 delayed-rectifier K+","category":"Voltage-gated K","description":"Kv1.1 (KCNA1) delayed-rectifier K+. m4 (V1/2 -30 mV) + C-type slow inactivation (tau 3 s). gamma=9-14 pS. alpha-DTX/TEA pharmacology. GHK K+ (z=1).","release":false}]})BIOCORE";
}
