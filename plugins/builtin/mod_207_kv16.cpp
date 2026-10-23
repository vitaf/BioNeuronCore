// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_207_kv16.cpp — Kv1.6 (KCNA6) delayed rectifier K⁺ канал.
// Тип: MOD_KV16 (207). γ=8.2 пС, V½_m=−8.1, non-inactivating (NIP-домен).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double kv16_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : KV16_GAMMA_PS;
    double m_val = m->state[0];
    float m_inf = boltzmann_act(V, KV16_VHALF_M, KV16_K_M);
    float tau_m = KV16_TAU_M_MS;
    m_val += (m_inf - m_val) * dt / tau_m;
    if (m_val < 0.0f) m_val = 0.0f; if (m_val > 1.0f) m_val = 1.0f;
    m->state[0] = m_val;
    float open_prob = m_val * m_val * m_val;
    // Ингибиция (фармакология): TEA (state[3], мМ) + 4-AP (state[4], мМ)
    double tea = (m->state[3] > 0.0f) ? m->state[3] : 0.0f;
    double ap4 = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;
    open_prob /= (1.0f + tea / KV16_TEA_IC50_MM) * (1.0f + ap4 / KV16_4AP_IC50_MM);
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
    register_modifier_current(MOD_KV16, kv16_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Kv1.6 delayed rectifier K","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":207,"name":"Kv1.6 (KCNA6)","category":"Ионные каналы","description":"Kv1.6 delayed rectifier K⁺ (γ=8.2 пС, V½_m=−8.1, non-inactivating, NIP-домен). GHK K⁺, m³-гейтинг, TEA/4-AP.","release":false}]})BIOCORE";
}
