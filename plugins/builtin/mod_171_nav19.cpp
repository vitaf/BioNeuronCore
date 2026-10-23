// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_171_nav19.cpp — Nav1.9 (SCN11A) персистентный TTX-R Na⁺
// Тип: MOD_NAV19 (171). Категория: Voltage-gated Na
// Био: Nav1.9 — персистентный (окно-ток) TTX-R Na⁺ (Cummins 1999).
// GHK-парадигма (γ=16 пС), m³h (m_inf V½=−47 k=5.5, h_inf V½=−44 k=6, почти без инактивации).
// TTX-R (Ki 39 µM). См. notes/modifier_refs/NAV19_171.md.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double nav19_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    // m³h гейтинг: m_inf V½=−47 k=5.5 (τ_m=10 мс), h_inf V½=−44 k=6 (τ_h=43 мс)
    float m_inf = 1.0f / (1.0f + expf(-(V - (-47.0f)) / 5.5f));
    float h_inf = 1.0f / (1.0f + expf((V - (-44.0f)) / 6.0f));
    m->state[0] += (m_inf - m->state[0]) * (dt / 10.0f);
    m->state[1] += (h_inf - m->state[1]) * (dt / 43.0f);
    if (m->state[0] < 0.0f) m->state[0] = 0.0f; if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f; if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    double open = m->state[0] * m->state[0] * m->state[0] * m->state[1];

    // TTX-R ингибиция (state[5], нМ): Ki 39 µM
    double ttx = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    open /= (1.0f + ttx / 39000.0f);

    double P_single = ghk_single_channel_P(NAV19_GAMMA_PS, 1.0f, GHK_CBAR_NA_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double I_A = P_single * flux * (m->density * area) * open;
    float I = (float)(I_A * A_TO_PA);

    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);
    s->intra_ions[ION_NA] -= (I * dt) / cpm;
    s->extra_ions[ION_NA] += (I * dt) / (cpm * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);

    return -I;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NAV19, nav19_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Nav1.9 (SCN11A) персистентный Na⁺ канал","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":171,"name":"Nav1.9 (SCN11A) персистентный TTX-R Na⁺","category":"Voltage-gated Na","description":"Nav1.9 (SCN11A) персистентный TTX-R ноцицепторный Na⁺ (окно-ток, почти без инактивации). m³h: m_inf V½=−47 k=5.5 (τ_m=10 мс), h_inf V½=−44 k=6 (τ_h=43 мс). TTX-R (state[5] нМ, Ki 39 µM). γ=16 пС. GHK Na⁺ (z=1).","release":false}]})BIOCORE";
}
