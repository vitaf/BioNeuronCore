// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// kv13.cpp — Kv1.3 delayed-rectifier K⁺ канал
// Тип: MOD_KV13 (154)
// Категория: Voltage-gated K

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double kv13_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float Vhalf = -30.0f;   // ⚠️ Kv1.1 −30 / Kv1.2 −27; точная Kv1.3 V½ в откр. лит-ре не найдена — взято типовое
    float k     = 6.0f;     // slope (Kv1.1)
    float gamma_ps = 19.0f; // γ 14–24 пС (среднее, Fadool 2011)

    // Активация m⁴ (delayed rectifier), τ_m=5 мс (⚠️ из Kv1.1 — Kv1.3 точная не найдена)
    float m_inf = 1.0f / (1.0f + expf(-(V - Vhalf) / (k + 1e-6f)));
    float tau_m = 5.0f;
    m->state[0] += (m_inf - m->state[0]) * dt / tau_m;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    // C-type медленная инактивация (τ~3 с, V½≈−40) — Kv1.x slow inactivation (⚠️ Kv1.3 точная не найдена)
    float h_inf = 1.0f / (1.0f + expf((V + 40.0f) / 6.0f));
    float tau_h = 3000.0f;
    m->state[1] += (h_inf - m->state[1]) * dt / tau_h;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    float K_i = s->intra_ions[ION_K];
    float K_o = s->extra_ions[ION_K];
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double open_prob = m->state[0] * m->state[0] * m->state[0] * m->state[0] * m->state[1]; // m⁴·h_slow

    // Фармакология: ShK-186 (state[5], пМ) — селективный Kv1.3 блокатор
    double shk = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    open_prob /= (1.0f + shk / SHK_IC50_KV13);

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
    register_modifier_current(MOD_KV13, kv13_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Kv1.3 delayed-rectifier K+","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":154,"name":"Kv1.3 delayed-rectifier K+","category":"Voltage-gated K","description":"Kv1.3 (KCNA3) delayed-rectifier K+. Activation m4 (V1/2 ~ -30 mV, typical Kv1.x). gamma=14-24 pS. GHK flux K+ (z=1).","release":false}]})BIOCORE";
}
