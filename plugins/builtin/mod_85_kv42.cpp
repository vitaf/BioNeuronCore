// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// kv42.cpp — Kv4.2 A-type K⁺ канал с CaMKII-фосфорилированием
// Тип: MOD_KV42 (85)
// Категория: STDP Cascade

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double kv42_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double Vhalf = (m->state[4] != 0.0f) ? -fabs(m->state[4]) : -10.0f; // Kv4.2 (HVA для K⁺)
    double k = (m->state[5] > 0.0f) ? m->state[5] : 13.5f;  // slope (Kv4.2)
    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : 5.0f;  // γ (пС, Kv4.2)

    // Активация m³ (A-type — быстрая, три гейта)
    float m_inf = 1.0f / (1.0f + expf(-(V - Vhalf) / (k + 1e-6f)));
    float tau_m = 1.5f;
    m->state[0] += (m_inf - m->state[0]) * dt / tau_m;

    // Инактивация (быстрая N-type при деполяр.) + де-инактивация (медленная recovery при гиперполяр.)
    float h_inf = 1.0f / (1.0f + expf((V + 65.0f) / 8.0f));
    float tau_h = kv42_h_tau(V);
    m->state[1] += (h_inf - m->state[1]) * dt / tau_h;

    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    // Чтение CaMKII -> фосфорилирование Kv4.2
    float camkii = 0.0f;
    for (uint8_t j = 0; j < s->modifier_count; j++)
        if (s->modifiers[j].type == MOD_CAMKII)
            camkii = s->modifiers[j].state[0] + s->modifiers[j].state[1];
    m->state[3] += (camkii * 0.5f - m->state[3]) * dt / 5000.0f;
    if (m->state[3] < 0.0f) m->state[3] = 0.0f;
    if (m->state[3] > 1.0f) m->state[3] = 1.0f;

    // Фосфорилирование снижает γ (0-70%)
    double gamma_eff = gamma_ps * (1.0f - m->state[3] * 0.7f);
    float K_i = s->intra_ions[ION_K];  // моль/м³
    float K_o = s->extra_ions[ION_K];
    double P_single = ghk_single_channel_P(gamma_eff, 1.0f, GHK_CBAR_K_MM);
    double open_prob = m->state[0] * m->state[0] * m->state[0] * m->state[1]; // m³·h
    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    float I_K = (float)(I_A * A_TO_PA);

    // Прямое движение K+
    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);
    s->intra_ions[ION_K] -= (I_K * dt) / cpm;
    s->extra_ions[ION_K] += (I_K * dt) / (cpm * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);

    return -I_K;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_KV42, kv42_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Kv4.2 A-type K+","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":85,"name":"Kv4.2 A-type K+","category":"STDP Cascade","description":"A-type K+ channel (Kv4.2). Activation m³ (V½=−10 мВ), fast N-type inactivation (V½=−65 мВ). γ=5 пС. CaMKII phosphorylation reduces conductance (0-70%). GHK flux for K+ (z=1).","release":false}]})BIOCORE";
}
