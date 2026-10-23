// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_180_calhm13.cpp — CALHM1/3 АТФ-пора
// Тип: MOD_CALHM13 (180). Категория: Other
// Био: CALHM1/3 — пора высвобождения АТФ (вкус тип II), V-зависимая (Taruno 2013; Ma 2018).
// GHK-парадигма (γ=20 пС — placeholder), m_inf V½=+30.5 k=8, τ=20 мс.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double calhm13_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    // m-активация: V½=+30.5 k=8, τ=20 мс
    float m_inf = 1.0f / (1.0f + expf(-(V - 30.5f) / 8.0f));
    m->state[0] += (m_inf - m->state[0]) * (dt / 20.0f);
    if (m->state[0] < 0.0f) m->state[0] = 0.0f; if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    double open = m->state[0];

    // γ=25 пС (Ma 2012 PNAS), слабо селективна: P_Na:P_K=1:1.17, P_Ca/P_Na≈10 (Cl 0.56 — анион, малый вклад пропущен)
    double P_Na, P_K, P_Ca;
    ghk_partial_P(CALHM13_GAMMA_PS, 1.0f, 1.17f, 10.0f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double flux_K = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double flux_Ca = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    float Ina = (float)(P_Na * flux_Na * N * open * A_TO_PA);
    float Ik  = (float)(P_K * flux_K * N * open * A_TO_PA);
    float Ica = (float)(P_Ca * flux_Ca * N * open * A_TO_PA);

    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);
    s->intra_ions[ION_NA] -= (Ina * dt) / cpm;
    s->intra_ions[ION_K]  -= (Ik * dt) / cpm;
    s->intra_ions[ION_CA] -= (Ica * dt) / (2.0f * cpm);
    s->extra_ions[ION_NA] += (Ina * dt) / (cpm * EXTRA_VOLUME_RATIO);
    s->extra_ions[ION_K]  += (Ik * dt) / (cpm * EXTRA_VOLUME_RATIO);
    s->extra_ions[ION_CA] += (Ica * dt) / (2.0f * cpm * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);

    return -(Ina + Ik + Ica);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_CALHM13, calhm13_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"CALHM1/3 АТФ-пора","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":180,"name":"CALHM1/3 АТФ-пора","category":"Other","description":"CALHM1/3 — пора высвобождения АТФ (вкус тип II), V-зависимая. m_inf V½=+30.5 мВ k=8, τ=20 мс. γ=20 пС (placeholder). GHK Na⁺ (z=1). Taruno 2013; Ma 2018.","release":false}]})BIOCORE";
}
