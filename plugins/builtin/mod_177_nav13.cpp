// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_177_nav13.cpp — Nav1.3 (SCN3A) TTX-S Na⁺
// Тип: MOD_NAV13 (177). Категория: Voltage-gated Na
// Био: Nav1.3 — TTX-чувствительный Na⁺-канал (Cummins 2001).
// GHK-парадигма (γ=18 пС), m³h (m_inf V½=−15.8 k=5.2, h_inf V½=−51.8 k=5.5).
// TTX-ингибиция (state[5], нМ; IC50 ~4 нМ).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double nav13_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    // m³h гейтинг: m_inf V½=−22 k=6.5 (τ_m=0.5 мс), h_inf V½=−64 k=6.5 (τ_h=0.65 мс) — Cummins 2001
    float m_inf = 1.0f / (1.0f + expf(-(V - (-22.0f)) / 6.5f));
    float h_inf = 1.0f / (1.0f + expf((V - (-64.0f)) / 6.5f));
    m->state[0] += (m_inf - m->state[0]) * (dt / 0.5f);
    m->state[1] += (h_inf - m->state[1]) * (dt / 0.65f);
    if (m->state[0] < 0.0f) m->state[0] = 0.0f; if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f; if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    double open = m->state[0] * m->state[0] * m->state[0] * m->state[1];

    // TTX-ингибиция (state[5], нМ): TTX-S IC50 ~4 нМ
    double ttx = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    open /= (1.0f + ttx / 4.0f);

    double P_single = ghk_single_channel_P(NAV13_GAMMA_PS, 1.0f, GHK_CBAR_NA_MM);
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
    register_modifier_current(MOD_NAV13, nav13_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Nav1.3 (SCN3A) TTX-S Na⁺ канал","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":177,"name":"Nav1.3 (SCN3A) TTX-S Na⁺","category":"Voltage-gated Na","description":"Nav1.3 (SCN3A) TTX-чувствительный Na⁺-канал. m³h: m_inf V½=−15.8 мВ k=5.2 (τ_m=0.5 мс), h_inf V½=−51.8 мВ k=5.5 (τ_h=0.65 мс). TTX-ингибиция (state[5] нМ, IC50 ~4 нМ). γ=18 пС. GHK Na⁺ (z=1). Cummins 2001.","release":false}]})BIOCORE";
}
