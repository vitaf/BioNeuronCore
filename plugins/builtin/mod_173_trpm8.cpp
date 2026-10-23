// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_173_trpm8.cpp — TRPM8 холод/ментол канал
// Тип: MOD_TRPM8 (173). Категория: TRP channel
// Био: TRPM8 — холодовой/ментоловый сенсор (McKemy 2002), γ≈63 пС.
// Лиганд state[4]=ментол µM (EC50 ~30 µM), Ca²⁺-десенситизация (Kd 0.5 µM).
// GHK: Ica=P·0.5 (z=2), Ina=P·0.5 (z=1). См. notes/modifier_refs/TRPM8_173.md.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double trpm8_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    // Лиганд state[4]=ментол µM, r=lig/(lig+EC50), EC50 ~30 µM
    double lig = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;
    float r = lig / (lig + 30.0f);

    // Ca²⁺-десенситизация: d_inf = Ca/(Ca+Kd), Kd=0.5 µM
    float Ca = s->intra_ions[ION_CA] * MM_TO_UM;
    float d_inf = Ca / (Ca + 0.5f);

    m->state[0] += (r - m->state[0]) * (dt / 20.0f);   // τ_акт 20 мс
    m->state[1] += (d_inf - m->state[1]) * (dt / 200.0f); // τ_десенс 200 мс
    if (m->state[0] < 0.0f) m->state[0] = 0.0f; if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f; if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    double open = m->state[0] * (1.0f - m->state[1]);

    // P_Ca:P_Na = 1:1 (γ=63 пС)
    double P_Na, P_K, P_Ca;
    ghk_partial_P(63.0f, 0.5f, 0.0f, 0.5f, &P_Na, &P_K, &P_Ca);
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
    register_modifier_current(MOD_TRPM8, trpm8_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"TRPM8 холод/ментол канал","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":173,"name":"TRPM8 холод/ментол канал","category":"TRP channel","description":"TRPM8 холодовой/ментоловый катионный канал. Лиганд state[4]=ментол µM (EC50 ~30 µM), Ca²⁺-десенситизация (Kd 0.5 µM, τ 200 мс). γ=63 пС. Ica=P·0.5 (z=2) + Ina=P·0.5 (z=1). GHK.","release":false}]})BIOCORE";
}
