// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_199_nav11.cpp — Nav1.1 (SCN1A) transient Na⁺, repetitive (PV⁺-интернейроны).
// Тип: MOD_NAV11 (199). Категория: Voltage-gated Na.
// Био: γ=17 пС (Vanoye 2006). V½_акт=−30 / инакт=−65 (нейроны, PMC4220023).
// Быстрый однофазный recovery (repetitive) — НЕ slow 500 мс (NAV12DEV single-spike).
// Для fast-spiking PV⁺-интернейронов, проприоцепторов (статика), тригеминального ганглия, таламуса.
// GHK-парадигма, m³h. См. notes/modifier_refs/NAV11_199.md.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double nav11_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = plugin_segment_area(s);

    if (s->V_m < -200.0f) s->V_m = -200.0f;
    if (s->V_m > 200.0f)  s->V_m = 200.0f;
    V = s->V_m;

    // m³h гейтинг, экспоненциальный Эйлер. Однофазный быстрый recovery (repetitive).
    double m_val = nav11_m_inf(V) + (m->state[0] - nav11_m_inf(V)) * expf(-dt / nav11_m_tau(V));
    double h_val = nav11_h_inf(V) + (m->state[1] - nav11_h_inf(V)) * expf(-dt / nav11_h_tau(V));
    if (m_val < 0.0f) m_val = 0.0f; if (m_val > 1.0f) m_val = 1.0f;
    if (h_val < 0.0f) h_val = 0.0f; if (h_val > 1.0f) h_val = 1.0f;
    m->state[0] = m_val;
    m->state[1] = h_val;

    float open_prob = m_val * m_val * m_val * h_val;
    // Ингибиция: TTX (state[5], нМ). Nav1.1 TTX-S (IC50 ~10 нМ, как Nav1.2).
    double ttx = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    open_prob /= (1.0f + ttx / TTX_IC50_NAV12);

    double P_single = ghk_single_channel_P(NAV11_GAMMA_PS, 1.0f, GHK_CBAR_NA_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    float I_Na = (float)(I_A * A_TO_PA);

    float vol = segment_volume(s);
    float cpv = FARADAY_CONST * vol / 1000.0f;
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpv;
    s->extra_ions[ION_NA] += (I_Na * dt) / (cpv * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);

    return -I_Na;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NAV11, nav11_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Nav1.1 (SCN1A) transient Na⁺","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":199,"name":"Nav1.1 (SCN1A) transient Na⁺","category":"Voltage-gated Na","description":"Nav1.1 (SCN1A) — млекопитающий transient Na⁺, repetitive (fast-spiking PV⁺-интернейроны, проприоцепторы-статика, тригеминальный ганглий, таламус). m³h: активация V½=−30 мВ k=6.5, инактивация V½=−65 мВ k=5.0. Однофазный быстрый recovery τ_rec≈7 мс (repetitive) — НЕ slow 500 мс (NAV12DEV single-spike). γ=17 пС (Vanoye 2006). TTX-S (IC50 10 нМ, state[5]). GHK Na⁺ (z=1).","release":false}]})BIOCORE";
}
