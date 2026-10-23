// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// cav22.cpp — Cav2.2 N-type Ca²⁺ канал (CACNA1B)
// Тип: MOD_CAV22 (168)

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double cav22_current_impl(Modifier* m, Segment* s, float dt) {
    // Cav2.2 (CACNA1B) — N-type Ca²⁺. γ=2.76 пС (физиол. Ca²⁺); HVA m² V½ −20; h β-зависимая; ω-GVIA.
    float V = s->V_m;
    float area = effective_area(m, s);
    float Vhalf = -20.0f;   // HVA
    float k     = 6.0f;
    float gamma_ps = 2.76f; // γ (пС, физиол. Ca²⁺; 16.4 в Ba²⁺)

    // Активация m² (HVA)
    float m_inf = 1.0f / (1.0f + expf(-(V - Vhalf) / (k + 1e-6f)));
    float tau_m = 1.7f;
    m->state[0] += (m_inf - m->state[0]) * dt / tau_m;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    // Инактивация h (β-зависимая, ~116–174 мс)
    float h_inf = 1.0f / (1.0f + expf((V + 60.0f) / 6.0f));
    float tau_h = 150.0f;
    m->state[1] += (h_inf - m->state[1]) * dt / tau_h;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    double open_prob = m->state[0] * m->state[0] * m->state[1]; // m²·h

    // Фармакология: ω-conotoxin GVIA (state[5], нМ) — неконкурентный блок поры
    double gvia = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    open_prob /= (1.0f + gvia / GVIA_IC50_CAV22);

    // GHK Ca²⁺ (z=2)
    float Ca_i = s->intra_ions[ION_CA];
    float Ca_o = s->extra_ions[ION_CA];
    double P_single = ghk_single_channel_P(gamma_ps, 2.0f, GHK_CBAR_CA_MM);
    double flux = ghk_divalent(V, Ca_i, Ca_o);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    float I_Ca = (float)(I_A * A_TO_PA);

    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);
    s->intra_ions[ION_CA] -= (I_Ca * dt) / cpm;
    s->extra_ions[ION_CA] += (I_Ca * dt) / (cpm * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);

    return -I_Ca;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_CAV22, cav22_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Cav2.2 N-type Ca2+","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":168,"name":"Cav2.2 N-type Ca2+","category":"Voltage-gated Ca","description":"Cav2.2 (CACNA1B) N-type Ca2+. m2 HVA (V1/2 -20 mV) + slow beta-dependent h inactivation. omega-conotoxin GVIA block (IC50 0.5 nM). gamma=2.76 pS. GHK Ca2+ (z=2).","release":false}]})BIOCORE";
}
