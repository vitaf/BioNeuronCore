// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_105_nap_channel.cpp — Персистентный Na⁺ канал (Nav1.6)
// Тип: MOD_NAP_CHANNEL (105)
//
// NaP (Persistent Na⁺, Nav1.6) — непогашаемый натриевый ток.
// Активируется при подпороговых потенциалах (~-55 mV).
// БЕЗ инактивации — остаётся открытым, создаёт устойчивый
// деполяризующий драйв. Критичен для подпороговых осцилляций
// и ритмогенеза в CPG.
//
// Архитектура: прямой GHK-поток Na⁺ + одни m-ворота (без h).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double nap_channel_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    // state[0] = m_val (activation, dynamic)
    // state[1] = V_half_m (mV, default −58.7 — persistent Nav1.6)
    // state[2] = k_m (mV, default 6)
    // state[3] = gamma (пС, single-channel, default 20.7)
    // state[4] = tau_m (ms, default 5)
    // state[5] = Po (persistent-фракция, default 0.016)
    double m_val = m->state[0];
    double V_half = (m->state[1] != 0.0f) ? m->state[1] : NAP_VHALF_M;
    double k_m = (m->state[2] > 0.0f) ? m->state[2] : NAP_K_M;
    double gamma_ps = (m->state[3] > 0.0f) ? m->state[3] : NAP_GAMMA_PS;
    double tau_m = (m->state[4] > 0.0f) ? m->state[4] : NAP_TAU_M_MS;
    double Po = (m->state[5] > 0.0f) ? m->state[5] : NAP_PERSIST_FRAC;
    // ✅ ДОСТОВЕРНО — менять ТОЛЬКО с согласования с пользователем (алгоритм аудита модификаторов).

    // Активация (Больцман, без инактивации)
    float m_inf = boltzmann_act(V, V_half, k_m);
    m_val += (m_inf - m_val) * dt / tau_m;
    if (m_val < 0.0f) m_val = 0.0f;
    if (m_val > 1.0f) m_val = 1.0f;
    m->state[0] = m_val;

    // GHK-поток Na⁺ (z=1), P в см/с
    float Na_i = s->intra_ions[ION_NA];  // моль/м³
    float Na_o = s->extra_ions[ION_NA];
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_NA_MM);
    double flux = ghk_monovalent(V, Na_i, Na_o);
    double I_A = P_single * flux * (m->density * area) * (m_val * Po);
    float I_Na = (float)(I_A * A_TO_PA);

    float volume = segment_volume(s);
    float charge_per_mole_vol = FARADAY_CONST * volume / 1000.0f;
    s->intra_ions[ION_NA] -= (I_Na * dt) / charge_per_mole_vol;

    plugin_clamp_ions(s->intra_ions, ION_COUNT);

    return -I_Na;  // Na⁺ входит = деполяризующий
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NAP_CHANNEL, nap_channel_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"NaP канал (Nav1.6)","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":105,"name":"NaP канал — персистентный Na⁺ (Nav1.6)","category":"Ионные каналы ритмогенеза","description":"Персистентный Na⁺ канал (Nav1.6). Без инактивации (только m-ворота). Подпороговая активация (V_half=-52mV). Малая проводимость (0.08nS). Обеспечивает устойчивый деполяризующий драйв для подпороговых осцилляций и ритмогенеза CPG. GHK-поток Na⁺.","release":false}]})BIOCORE";
}
