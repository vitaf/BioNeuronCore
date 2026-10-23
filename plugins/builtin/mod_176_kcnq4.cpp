// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_176_kcnq4.cpp — KCNQ4 (Kv7.4) покойный K⁺ волосковых клеток
// Тип: MOD_KCNQ4 (176). Категория: Voltage-gated K
// Био: KCNQ4 (Kv7.4) — медленный потенциал-зависимый K⁺ БЕЗ инактивации (Kharkovets 2000).
// GHK-парадигма (γ=2.31 пС), m (Boltzmann V½=−78 k=14, τ_акт 212 мс).
// См. notes/modifier_refs/KCNQ4_176.md.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double kcnq4_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    // Активация m (нативный V½=−78 мВ, k=14), τ_акт 212 мс, без инактивации
    float m_inf = 1.0f / (1.0f + expf(-(V - (-78.0f)) / 14.0f));
    m->state[0] += (m_inf - m->state[0]) * (dt / 212.0f);
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    double open = m->state[0];  // Kv7: без инактивации

    double P_single = ghk_single_channel_P(2.31f, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double I_A = P_single * flux * (m->density * area) * open;
    float I = (float)(I_A * A_TO_PA);

    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);
    s->intra_ions[ION_K] -= (I * dt) / cpm;
    s->extra_ions[ION_K] += (I * dt) / (cpm * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);

    return -I;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_KCNQ4, kcnq4_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"KCNQ4 (Kv7.4) покойный K⁺ канал","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":176,"name":"KCNQ4 (Kv7.4) покойный K⁺","category":"Voltage-gated K","description":"KCNQ4 (Kv7.4) медленный потенциал-зависимый K⁺ волосковых клеток, БЕЗ инактивации. m: Boltzmann V½=−78 мВ k=14, τ_акт 212 мс. γ=2.31 пС. GHK-флюкс K⁺ (z=1).","release":false}]})BIOCORE";
}
