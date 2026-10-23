// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_189_piezo2.cpp — PIEZO2 механочувствительный катионный канал.
// Тип: MOD_PIEZO2 (189). Категория: Ионные каналы (механо).
// Био: PIEZO2 (проприоцепция/касание), γ=16 пС, λ50=2 мН/м, P_Ca>P_K>P_Na.
// Механо-стимул state[4]=мН/м (VirtualBody-мост), rapidly-adapting τ~6 мс.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double piezo2_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m; float area = effective_area(m, s);
    double stim = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;
    float s23 = powf(stim, 2.3f), l23 = powf(2.0f, 2.3f);
    float po_inf = (stim > 0.0f) ? s23 / (l23 + s23) : 0.0f;
    m->state[0] += (po_inf - m->state[0]) * (dt / 1.0f);
    m->state[1] += (po_inf - m->state[1]) * (dt / 6.0f);
    if (m->state[0] < 0) m->state[0] = 0; if (m->state[0] > 1) m->state[0] = 1;
    if (m->state[1] < 0) m->state[1] = 0; if (m->state[1] > 1) m->state[1] = 1;
    double open = m->state[0] * (1.0f - m->state[1]);
    double P_Na, P_K, P_Ca;
    ghk_partial_P(24.0f, 0.27f, 0.32f, 0.40f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Ca = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    double flux_K = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    float Ica = (float)(P_Ca * flux_Ca * N * open * A_TO_PA);
    float Ik  = (float)(P_K * flux_K * N * open * A_TO_PA);
    float Ina = (float)(P_Na * flux_Na * N * open * A_TO_PA);
    float cpm = FARADAY_CONST * segment_volume(s) / 1000.0f;
    s->intra_ions[ION_CA] -= (Ica*dt)/(2.0f*cpm);
    s->intra_ions[ION_K]  -= (Ik*dt)/cpm;
    s->intra_ions[ION_NA] -= (Ina*dt)/cpm;
    s->extra_ions[ION_CA] += (Ica*dt)/(2.0f*cpm*EXTRA_VOLUME_RATIO);
    s->extra_ions[ION_K]  += (Ik*dt)/(cpm*EXTRA_VOLUME_RATIO);
    s->extra_ions[ION_NA] += (Ina*dt)/(cpm*EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT); clamp_ions(s->extra_ions, ION_COUNT);
    return -(Ica + Ik + Ina);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_PIEZO2, piezo2_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"PIEZO2 mechanosensitive","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":189,"name":"PIEZO2 (механочувствительный катионный)","category":"Ионные каналы","description":"PIEZO2 механочувствительный катионный канал (проприоцепция/касание). γ 16 пС, λ50 2 мН/м, P_Ca>P_K>P_Na, rapidly-adapting τ 6 мс.","release":false}]})BIOCORE";
}
