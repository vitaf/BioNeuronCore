// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_175_trpv3.cpp — TRPV3 тепло канал (сенситизация)
// Тип: MOD_TRPV3 (175). Категория: TRP channel
// Био: TRPV3 — тепловой сенсор с сенситизацией (Xu 2002), γ=185 пС.
// Лиганд state[4]=2-APB µM (EC50 ~50 µM). БЕЗ десенситизации (только state[0]).
// GHK: Ica=P·0.5 (z=2), Ina=P·0.5 (z=1). См. notes/modifier_refs/TRPV3_175.md.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double trpv3_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    // Лиганд state[4]=2-APB µM, r=lig/(lig+EC50), EC50 ~50 µM
    double lig = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;
    float r = lig / (lig + 50.0f);

    m->state[0] += (r - m->state[0]) * (dt / 20.0f);   // τ_акт 20 мс
    if (m->state[0] < 0.0f) m->state[0] = 0.0f; if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    double open = m->state[0];  // сенситизация (НЕ десенс.)

    // P_Ca:P_Na = 1:1 (γ=185 пС)
    double P_Na, P_K, P_Ca;
    ghk_partial_P(185.0f, 0.5f, 0.0f, 0.5f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Ca = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    float Ica = (float)(P_Ca * flux_Ca * N * open * A_TO_PA);
    float Ina = (float)(P_Na * flux_Na * N * open * A_TO_PA);

    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);
    s->intra_ions[ION_CA] -= (Ica * dt) / (2.0f * cpm);
    s->intra_ions[ION_NA] -= (Ina * dt) / cpm;
    s->extra_ions[ION_CA] += (Ica * dt) / (2.0f * cpm * EXTRA_VOLUME_RATIO);
    s->extra_ions[ION_NA] += (Ina * dt) / (cpm * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);

    return -(Ica + Ina);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_TRPV3, trpv3_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"TRPV3 тепло канал (сенситизация)","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":175,"name":"TRPV3 тепло канал (сенситизация)","category":"TRP channel","description":"TRPV3 тепловой катионный канал с сенситизацией (БЕЗ десенситизации). Лиганд state[4]=2-APB µM (EC50 ~50 µM), τ_акт 20 мс (только state[0]). γ=185 пС. Ica=P·0.5 (z=2) + Ina=P·0.5 (z=1). GHK.","release":false}]})BIOCORE";
}
