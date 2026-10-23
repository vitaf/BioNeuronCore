// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_170_nav18.cpp — Nav1.8 (SCN10A) TTX-R ноцицепторный Na⁺
// Тип: MOD_NAV18 (170). Категория: Voltage-gated Na
// Био: Nav1.8 — TTX-резистентный Na⁺-канал ноцицепторов (Akopian 1996).
// GHK-парадигма (γ=20 пС), m³h (m_inf V½=−20 k=7, h_inf V½=−30 k=5, медленный).
// TTX-R (IC50 ~60 µM). См. notes/modifier_refs/NAV18_170.md.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double nav18_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    // m³h гейтинг: m_inf V½=−20 k=7 (τ_m=1.5 мс), h_inf V½=−30 k=5 (τ_h=8 мс)
    float m_inf = 1.0f / (1.0f + expf(-(V - (-20.0f)) / 7.0f));
    float h_inf = 1.0f / (1.0f + expf((V - (-30.0f)) / 5.0f));
    m->state[0] += (m_inf - m->state[0]) * (dt / 1.5f);
    m->state[1] += (h_inf - m->state[1]) * (dt / 8.0f);
    if (m->state[0] < 0.0f) m->state[0] = 0.0f; if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f; if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    double open = m->state[0] * m->state[0] * m->state[0] * m->state[1];

    // TTX-R ингибиция (state[5], нМ): IC50 ~60 µM
    double ttx = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    open /= (1.0f + ttx / 60000.0f);

    double P_single = ghk_single_channel_P(NAV18_GAMMA_PS, 1.0f, GHK_CBAR_NA_MM);
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
    register_modifier_current(MOD_NAV18, nav18_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Nav1.8 (SCN10A) TTX-R Na⁺ канал","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":170,"name":"Nav1.8 (SCN10A) TTX-R Na⁺","category":"Voltage-gated Na","description":"Nav1.8 (SCN10A) TTX-резистентный ноцицепторный Na⁺. m³h: m_inf V½=−20 k=7 (τ_m=1.5 мс), h_inf V½=−30 k=5 (τ_h=8 мс, медленный). TTX-R (state[5] нМ, IC50 ~60 µM). γ=20 пС. GHK Na⁺ (z=1).","release":false}]})BIOCORE";
}
