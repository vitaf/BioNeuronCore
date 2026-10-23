// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_204_kv33.cpp — Kv3.3 (KCNC3) high-threshold K⁺ канал.
// Тип: MOD_KV33 (204). γ=35 пС, V½_m=+1.5, V½_h=−27.9, A-type (медленная инакт ~200 мс).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double kv33_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : KV33_GAMMA_PS;
    double m_val = m->state[0], h_val = m->state[1];
    float m_inf = boltzmann_act(V, KV33_VHALF_M, KV33_K_M);
    float h_inf = boltzmann_inact(V, KV33_VHALF_H, KV33_K_H);
    float tau_m = KV33_TAU_M_MS;
    float tau_h = KV33_TAU_H_MS;
    m_val += (m_inf - m_val) * dt / tau_m;
    h_val += (h_inf - h_val) * dt / tau_h;
    if (m_val < 0.0f) m_val = 0.0f; if (m_val > 1.0f) m_val = 1.0f;
    if (h_val < 0.0f) h_val = 0.0f; if (h_val > 1.0f) h_val = 1.0f;
    m->state[0] = m_val; m->state[1] = h_val;
    float open_prob = m_val * m_val * m_val * h_val;
    // Ингибиция (фармакология): TEA (state[3], µM) + 4-AP (state[4], µM)
    double tea = (m->state[3] > 0.0f) ? m->state[3] : 0.0f;
    double ap4 = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;
    open_prob /= (1.0f + tea / KV33_TEA_IC50_UM) * (1.0f + ap4 / KV33_4AP_IC50_UM);
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
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
    register_modifier_current(MOD_KV33, kv33_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Kv3.3 high-threshold K","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":204,"name":"Kv3.3 (KCNC3)","category":"Ионные каналы","description":"Kv3.3 high-threshold K⁺ (γ=35 пС, V½_m=+1.5, V½_h=−27.9, A-type ~200 мс). GHK K⁺, m³h-гейтинг, TEA/4-AP.","release":false}]})BIOCORE";
}
