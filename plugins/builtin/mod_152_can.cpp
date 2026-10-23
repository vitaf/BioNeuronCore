// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// can.cpp — CAN (TRPM4/5) Ca²⁺-активируемый неселективный катионный канал
// Тип: MOD_CAN (152)
// Категория: Ca²⁺-activated nonselective

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double can_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float gamma_ps = 30.0f;  // γ ~25–35 пС (TRPM4/5)

    // Ca²⁺-зависимый Po (Hill n=2, Kd=0.5 µM = 5e-4 мМ)
    float ca = s->intra_ions[ION_CA];
    float Kd = 5e-4f;
    float Po = (ca * ca) / (ca * ca + Kd * Kd);
    // Ca²⁺-зависимая десенситизация (инактивация при длительном Ca²⁺, τ~2 с)
    float d_inf = ca / (ca + 0.01f);
    float tau_d = 2000.0f;
    m->state[1] += (d_inf - m->state[1]) * dt / tau_d;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;
    Po *= (1.0f - m->state[1]);  // Po·(1−d)

    // Неселективный Na⁺/K⁺ (P_Na:P_K = 1:1)
    double P_Na, P_K, P_Ca;
    ghk_partial_P(30.0f, 1.0f, 1.0f, 0.0f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double flux_K = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    float Ina = (float)(P_Na * flux_Na * N * Po * A_TO_PA);
    float Ik  = (float)(P_K * flux_K * N * Po * A_TO_PA);

    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);
    s->intra_ions[ION_NA] -= (Ina * dt) / cpm;
    s->intra_ions[ION_K]  -= (Ik  * dt) / cpm;
    s->extra_ions[ION_NA] += (Ina * dt) / (cpm * EXTRA_VOLUME_RATIO);
    s->extra_ions[ION_K]  += (Ik  * dt) / (cpm * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);

    return -(Ina - Ik);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_CAN, can_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"CAN (TRPM4/5) Ca2+-activated nonselective cation","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":152,"name":"CAN (TRPM4/5)","category":"Ca2+-activated nonselective","description":"Ca2+-activated nonselective cation channel (TRPM4/5). Monovalent Na+/K+ (not Ca2+-conducting), Ca2+-dependent Po (Hill n=2, Kd=0.5 uM). gamma=25-35 pS. Mixed GHK Na/K (1:1).","release":false}]})BIOCORE";
}
