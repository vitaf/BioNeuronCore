// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_193_nav12dev.cpp — тадпольный аксональный Na⁺ (Nav1.2 developmental/Scn2a), transient m³h.
// Тип: MOD_NAV12DEV (193). Категория: Voltage-gated Na.
// Био: Nav1.2 developmental — низкий порог активации (V½=−23, как Nav1.2) + деполяризованная
// инактивация (V½=−35, Ribera & Spitzer) → доступен на покое головастика −51 мВ.
// Нужен для проведения ПД по немиелинизированному аксону (задача #8).
// GHK-парадигма (γ=20 пС), m³h. См. notes/modifier_refs/NAV12DEV_193.md.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double nav12dev_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = plugin_segment_area(s);

    if (s->V_m < -200.0f) s->V_m = -200.0f;
    if (s->V_m > 200.0f)  s->V_m = 200.0f;
    V = s->V_m;

    // m³h гейтинг (m — как Nav1.2, h — деполяризованная инактивация), экспоненциальный Эйлер
    double m_val = nav12_m_inf(V) + (m->state[0] - nav12_m_inf(V)) * expf(-dt / nav12_m_tau(V));
    double h_val = nav12dev_h_inf(V) + (m->state[1] - nav12dev_h_inf(V)) * expf(-dt / nav12_h_tau(V));
    if (m_val < 0.0f) m_val = 0.0f; if (m_val > 1.0f) m_val = 1.0f;
    if (h_val < 0.0f) h_val = 0.0f; if (h_val > 1.0f) h_val = 1.0f;
    m->state[0] = m_val;
    m->state[1] = h_val;

    float open_prob = m_val * m_val * m_val * h_val;
    // Ингибиция: TTX (state[5], нМ) + PKA/PKC фосфорилирование (сильное — Ser573, как Nav1.2)
    double ttx = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    open_prob /= (1.0f + ttx / TTX_IC50_NAV12);
    float pkc = segment_pkc_act(s);
    float pka = segment_pka_act(s);
    open_prob *= (1.0f - PKA_PKC_INHIBIT_NAV12 * (pkc + pka) * 0.5f);
    if (open_prob < 0.0f) open_prob = 0.0f;

    double P_single = ghk_single_channel_P(NAV12DEV_GAMMA_PS, 1.0f, GHK_CBAR_NA_MM);
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
    register_modifier_current(MOD_NAV12DEV, nav12dev_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Тадпольный аксональный Na⁺ (Nav1.2 developmental/Scn2A)","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":193,"name":"Nav1.2 developmental (тадпольный аксональный Na⁺)","category":"Voltage-gated Na","description":"Nav1.2 developmental (Scn2a) — тадпольный аксональный Na⁺. m³h: активация V½=−23 мВ k=6 (как Nav1.2), инактивация V½=−35 мВ k=6.2 (деполяризованная, Ribera & Spitzer) → доступен на покое головастика −51 мВ (h_inf=0.94). Для проведения ПД по немиелинизированному аксону (задача #8). TTX-S (IC50 10 нМ, state[5]), PKA/PKC (0.7, Ser573). γ=20 пС. GHK Na⁺ (z=1).","release":false}]})BIOCORE";
}
