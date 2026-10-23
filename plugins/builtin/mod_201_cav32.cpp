// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_201_cav32.cpp — Cav3.2 (CACNA1H/α1H) T-type Ca²⁺ канал.
// Тип: MOD_CAV32 (201). γ=9 пС, V½_m=−46, V½_h=−72, Ni²⁺ 13 µM. Самая Ni-чувствительная из T-type.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double cav32_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : CAV32_GAMMA_PS;
    double m_val = m->state[0], h_val = m->state[1];
    float m_inf = boltzmann_act(V, CAV32_VHALF_M, CAV32_K_M);
    float h_inf = boltzmann_inact(V, CAV32_VHALF_H, CAV32_K_H);
    float tau_m = cav32_m_tau(V);
    float tau_h = cav32_h_tau(V);
    float sub_dt = dt / 10.0f;
    for (int step = 0; step < 10; step++) {
        m_val += (m_inf - m_val) * sub_dt / tau_m;
        h_val += (h_inf - h_val) * sub_dt / tau_h;
    }
    if (m_val < 0.0f) m_val = 0.0f; if (m_val > 1.0f) m_val = 1.0f;
    if (h_val < 0.0f) h_val = 0.0f; if (h_val > 1.0f) h_val = 1.0f;
    m->state[0] = m_val; m->state[1] = h_val;
    float open_prob = m_val * m_val * m_val * h_val;
    // Ингибиция (фармакология): Ni²⁺/мибефрадил/TTA-P2/Z944 (state[3..6], мультипликативный блок)
    double ni = (m->state[3] > 0.0f) ? m->state[3] : 0.0f;   // Ni²⁺ (µМ)
    double mib = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;   // мибефрадил (µМ)
    double tta = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;   // TTA-P2 (нМ)
    double z944 = (m->state[6] > 0.0f) ? m->state[6] : 0.0f;   // Z944 (нМ)
    open_prob /= (1.0f + ni / CAV32_NI_IC50_UM) * (1.0f + mib / CAV32_MIBEFRADIL_IC50_UM)
               * (1.0f + tta / CAV32_TTAP2_IC50_NM) * (1.0f + z944 / CAV32_Z944_IC50_NM);
    double P_single = ghk_single_channel_P(gamma_ps, 2.0f, GHK_CBAR_CA_MM);
    double flux = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    float I_CaT = (float)(I_A * A_TO_PA);
    float volume = segment_volume(s);
    float charge_per_mole = FARADAY_CONST * volume / 1000.0f;
    s->intra_ions[ION_CA] -= (I_CaT * dt) / (2.0f * charge_per_mole);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    return -I_CaT;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_CAV32, cav32_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Cav3.2 T-type Ca","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":201,"name":"Cav3.2 (CACNA1H)","category":"Ионные каналы","description":"Cav3.2 T-type Ca²⁺ (γ=9 пС, V½_m=−46, V½_h=−72, Ni²⁺ 13 µM — самая Ni-чувствительная из T-type). GHK Ca²⁺ z=2, m³h-гейтинг, window-current для low-threshold burst.","release":false}]})BIOCORE";
}
