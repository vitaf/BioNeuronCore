// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// kv31.cpp — Kv3.1 fast-spiking K⁺ канал
// Тип: MOD_KV31 (150)
// Категория: Voltage-gated K

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double kv31_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float Vhalf = 10.0f;    // Kv3.1 высокопороговая активация (Rudy & McBain 2001)
    float k     = 10.0f;    // slope
    float gamma_ps = 27.0f; // γ (пС, Grissmer 1992)

    // Активация m³ (fast-spiking)
    float m_inf = 1.0f / (1.0f + expf(-(V - Vhalf) / (k + 1e-6f)));
    float tau_m = 1.0f;
    m->state[0] += (m_inf - m->state[0]) * dt / tau_m;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    // C-type медленная инактивация (τ~2 с, V½≈−30 мВ) — Klemic 1998 J Gen Physiol
    float h_inf = 1.0f / (1.0f + expf((V + 30.0f) / 6.0f));
    float tau_h = 2000.0f;
    m->state[1] += (h_inf - m->state[1]) * dt / tau_h;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    float K_i = s->intra_ions[ION_K];
    float K_o = s->extra_ions[ION_K];
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double open_prob = m->state[0] * m->state[0] * m->state[0] * m->state[1]; // m³·h_slow
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
    register_modifier_current(MOD_KV31, kv31_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Kv3.1 fast-spiking K+","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":150,"name":"Kv3.1 fast-spiking K+","category":"Voltage-gated K","description":"Fast-spiking delayed rectifier K+ (Kv3.1/KCNC1). Activation m3 (V1/2 ~ +10 mV), no inactivation, fast deactivation. gamma=27 pS. GHK flux K+ (z=1).","release":false}]})BIOCORE";
}
