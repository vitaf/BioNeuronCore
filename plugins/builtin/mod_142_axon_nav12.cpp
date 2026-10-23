// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_142_axon_nav12.cpp — аксональный Na⁺ (Nav1.2/SCN2A), transient m³h.
// Тип: MOD_AXON_NAV12 (142). Категория: Базовые модификаторы (ионные каналы).
// Био: Nav1.2 (SCN2A) — тадпольный спинальный Na-изоформ (ранние нейроны Xenopus
// экспрессируют Nav1.2, НЕ Nav1.6). GHK-парадигма (γ≈20 пС). Кинетика JGP 2022.
// См. notes/modifier_refs/NAV12_142.md.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double nav12_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = plugin_segment_area(s);

    if (s->V_m < -200.0f) s->V_m = -200.0f;
    if (s->V_m > 200.0f)  s->V_m = 200.0f;
    V = s->V_m;

    // m³h гейтинг (JGP 2022), экспоненциальный Эйлер
    double m_val = nav12_m_inf(V) + (m->state[0] - nav12_m_inf(V)) * expf(-dt / nav12_m_tau(V));
    double h_val = nav12_h_inf(V) + (m->state[1] - nav12_h_inf(V)) * expf(-dt / nav12_h_tau(V));
    if (m_val < 0.0f) m_val = 0.0f; if (m_val > 1.0f) m_val = 1.0f;
    if (h_val < 0.0f) h_val = 0.0f; if (h_val > 1.0f) h_val = 1.0f;
    m->state[0] = m_val;
    m->state[1] = h_val;

    // GHK: P = γ·density, I_Na = P·flux·area·m³h
    float open_prob = m_val * m_val * m_val * h_val;
    // Ингибиция: TTX (state[5], нМ) + PKA/PKC фосфорилирование (сильное — Nav1.2 Ser573)
    double ttx = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    open_prob /= (1.0f + ttx / TTX_IC50_NAV12);
    float pkc = segment_pkc_act(s);
    float pka = segment_pka_act(s);
    open_prob *= (1.0f - PKA_PKC_INHIBIT_NAV12 * (pkc + pka) * 0.5f);
    if (open_prob < 0.0f) open_prob = 0.0f;

    double P_single = ghk_single_channel_P(NAV12_GAMMA_PS, 1.0f, GHK_CBAR_NA_MM);
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
    register_modifier_current(MOD_AXON_NAV12, nav12_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Аксональный Na⁺ (Nav1.2/SCN2A) — transient m³h","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":142,"name":"Аксональный Na⁺ (Nav1.2)","category":"Базовые модификаторы","description":"Nav1.2 transient Na⁺ (m³h, JGP 2022), repetitive firing — тадпольный спинальный Na. GHK (γ≈20 пС).","release":false}]})BIOCORE";
}
