// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_141_axon_nav16.cpp — аксональный Na⁺ (Nav1.6/SCN8A), transient m³h.
// Тип: MOD_AXON_NAV16 (141). Категория: Базовые модификаторы (ионные каналы).
// Био: Nav1.6 — аксональный Na⁺ (repetitive firing, НЕ глубокая инактивация dIN).
// GHK-парадигма (γ=20.7 пС). Кинетика PNAS 2018. См. notes/modifier_refs/NAV16_141.md.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double nav16_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = plugin_segment_area(s);

    if (s->V_m < -200.0f) s->V_m = -200.0f;
    if (s->V_m > 200.0f)  s->V_m = 200.0f;
    V = s->V_m;

    // m³h гейтинг (PNAS 2018 HH), экспоненциальный Эйлер
    double m_val = nav16_m_inf(V) + (m->state[0] - nav16_m_inf(V)) * expf(-dt / nav16_m_tau(V));
    double h_val = nav16_h_inf(V) + (m->state[1] - nav16_h_inf(V)) * expf(-dt / nav16_h_tau(V));
    if (m_val < 0.0f) m_val = 0.0f; if (m_val > 1.0f) m_val = 1.0f;
    if (h_val < 0.0f) h_val = 0.0f; if (h_val > 1.0f) h_val = 1.0f;
    m->state[0] = m_val;
    m->state[1] = h_val;

    // GHK: P = γ·density, I_Na = P·flux·area·m³h
    float open_prob = m_val * m_val * m_val * h_val;
    // Ингибиция: TTX (state[5], нМ) + PKA/PKC фосфорилирование (слабое — Nav1.6 без Ser573)
    double ttx = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    open_prob /= (1.0f + ttx / TTX_IC50_NAV16);
    float pkc = segment_pkc_act(s);
    float pka = segment_pka_act(s);
    open_prob *= (1.0f - PKA_PKC_INHIBIT_NAV16 * (pkc + pka) * 0.5f);
    if (open_prob < 0.0f) open_prob = 0.0f;

    double P_single = ghk_single_channel_P(NAV16_GAMMA_PS, 1.0f, GHK_CBAR_NA_MM);
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
    register_modifier_current(MOD_AXON_NAV16, nav16_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Аксональный Na⁺ (Nav1.6/SCN8A) — transient m³h","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":141,"name":"Аксональный Na⁺ (Nav1.6)","category":"Базовые модификаторы","description":"Nav1.6 transient Na⁺ (m³h, PNAS 2018), repetitive firing. GHK (γ=20.7 пС).","release":false}]})BIOCORE";
}
