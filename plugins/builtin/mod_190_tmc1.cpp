// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_190_tmc1.cpp — TMC1/TMC2 MET канал стереоцилий волосковых клеток.
// Тип: MOD_TMC1 (190). Категория: Ионные каналы (механо).
// Био: MET (mechanoelectrical transduction), γ=80 пС, P_Ca/P_Na≈3.8.
// Базальная Po ~10%, механо-активация (сдвиг пучка state[4]), Ca²⁺-адаптация.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double tmc1_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m; float area = effective_area(m, s);
    double stim = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;
    float po_inf = 0.1f + 0.9f * (stim / (stim + 0.3f));
    m->state[0] += (po_inf - m->state[0]) * (dt / 1.0f);
    float Ca = s->intra_ions[ION_CA] * MM_TO_UM;
    float adapt = Ca / (Ca + 5.0f);
    m->state[1] += (adapt - m->state[1]) * (dt / 2.0f);        // fast (τ~2 мс)
    m->state[2] += (adapt - m->state[2]) * (dt / 45.0f);       // slow (myosin-1c, τ~45 мс — МЫШЬ утрикула PMID 12826658)
    if (m->state[0] < 0) m->state[0] = 0; if (m->state[0] > 1) m->state[0] = 1;
    if (m->state[1] < 0) m->state[1] = 0; if (m->state[1] > 1) m->state[1] = 1;
    if (m->state[2] < 0) m->state[2] = 0; if (m->state[2] > 1) m->state[2] = 1;
    double open = m->state[0] * (1.0f - m->state[1]) * (1.0f - m->state[2]);
    double P_Na, P_K, P_Ca;
    ghk_partial_P(80.0f, 0.21f, 0.0f, 0.79f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Ca = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    float Ica = (float)(P_Ca * flux_Ca * N * open * A_TO_PA);
    float Ina = (float)(P_Na * flux_Na * N * open * A_TO_PA);
    float cpm = FARADAY_CONST * segment_volume(s) / 1000.0f;
    s->intra_ions[ION_CA] -= (Ica*dt)/(2.0f*cpm);
    s->intra_ions[ION_NA] -= (Ina*dt)/cpm;
    s->extra_ions[ION_CA] += (Ica*dt)/(2.0f*cpm*EXTRA_VOLUME_RATIO);
    s->extra_ions[ION_NA] += (Ina*dt)/(cpm*EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT); clamp_ions(s->extra_ions, ION_COUNT);
    return -(Ica + Ina);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_TMC1, tmc1_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"TMC1/TMC2 MET channel","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":190,"name":"TMC1/TMC2 (MET канал стереоцилий)","category":"Ионные каналы","description":"TMC1/TMC2 mechanoelectrical transduction канал волосковых клеток. γ 80 пС, P_Ca/P_Na≈3.8, базальная Po 10%, Ca²⁺-адаптация.","release":false}]})BIOCORE";
}
