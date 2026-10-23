// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_202_kv43.cpp — Kv4.3 (KCND3) A-type K⁺ канал.
// Тип: MOD_KV43 (202). γ=4.4 пС, V½_m=−31.7, V½_h=−47.4, быстрая N-type инактивация.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double kv43_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : KV43_GAMMA_PS;
    double m_val = m->state[0], h_val = m->state[1];
    float m_inf = boltzmann_act(V, KV43_VHALF_M, KV43_K_M);
    float h_inf = boltzmann_inact(V, KV43_VHALF_H, KV43_K_H);
    float tau_m = KV43_TAU_M_MS;
    float tau_h = kv43_h_tau(V);
    float sub_dt = dt / 10.0f;
    for (int step = 0; step < 10; step++) {
        m_val += (m_inf - m_val) * sub_dt / tau_m;
        h_val += (h_inf - h_val) * sub_dt / tau_h;
    }
    if (m_val < 0.0f) m_val = 0.0f; if (m_val > 1.0f) m_val = 1.0f;
    if (h_val < 0.0f) h_val = 0.0f; if (h_val > 1.0f) h_val = 1.0f;
    m->state[0] = m_val; m->state[1] = h_val;
    float open_prob = m_val * m_val * m_val * h_val;
    // Ингибиция (фармакология): 4-AP (state[3], µM) + HpTx3 (state[4], нМ)
    double ap4 = (m->state[3] > 0.0f) ? m->state[3] : 0.0f;
    double hptx = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;
    open_prob /= (1.0f + ap4 / KV43_4AP_IC50_UM) * (1.0f + hptx / KV43_HPTX3_IC50_NM);
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
    register_modifier_current(MOD_KV43, kv43_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Kv4.3 A-type K","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":202,"name":"Kv4.3 (KCND3)","category":"Ионные каналы","description":"Kv4.3 A-type K⁺ (γ=4.4 пС, V½_m=−31.7, V½_h=−47.4, N-type инактивация). GHK K⁺, m³h-гейтинг, 4-AP/HpTx3.","release":false}]})BIOCORE";
}
