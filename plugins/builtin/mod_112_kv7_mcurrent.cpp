// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_112_kv7_mcurrent.cpp — Kv7 (KCNQ2/3) M-ток
// Тип: MOD_KV7_MCURRENT (112)
//
// Kv7 (KCNQ2/3) — медленный потенциал-зависимый K⁺ канал БЕЗ инактивации.
// Формирует M-ток (muscarinic-sensitive K⁺ current):
// - Медленная активация ~100ms (самый медленный Kv)
// - Нет инактивации (в отличие от Kv4.2 A-type)
// - PIP2-зависимый: Gq/11 → PLCβ → PIP2 depletion → закрытие
// - Ключевая роль: регуляция возбудимости, spike frequency adaptation
//
// Архитектура: GHK-поток K⁺ + voltage-gated m-ворота + PIP2-модуляция.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double kv7_mcurrent_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    // state[0] = m_val (dynamic activation gate)
    // state[1] = V_half (mV, default -38)
    // state[2] = k_slope (mV, default 10)
    // state[3] = g_max (nS, default 0.5)
    // state[4] = tau_act (ms, default 100)
    // state[5] = tau_deact (ms, default 60)
    // state[6] = PIP2_factor (0-1, default 1.0)
    double m_val = m->state[0];
    double V_half = (m->state[1] != 0.0f) ? m->state[1] : -38.0f;
    double k_slope = (m->state[2] > 0.0f)  ? m->state[2] : 10.0f;
    double tau_act = (m->state[4] > 0.0f)  ? m->state[4] : 100.0f;
    double tau_deact = (m->state[5] > 0.0f)  ? m->state[5] : 60.0f;
    double pip2 = (m->state[6] >= 0.0f) ? m->state[6] : 1.0f;

    // Активация (Больцман, без инактивации)
    float m_inf = 1.0f / (1.0f + expf(-(V - V_half) / k_slope));
    float tau_m = tau_deact + (tau_act - tau_deact) * m_inf;
    if (tau_m < 1.0f) tau_m = 1.0f;

    m_val += (m_inf - m_val) * dt / tau_m;
    if (m_val < 0.0f) m_val = 0.0f;
    if (m_val > 1.0f) m_val = 1.0f;
    m->state[0] = m_val;

    // PIP2-зависимость: при PIP2=0 канал закрыт
    if (pip2 < 0.01f) return 0.0f;

    // GHK-поток K⁺, P в см/с
    float K_i = s->intra_ions[ION_K];  // моль/м³
    float K_o = s->extra_ions[ION_K];

    double P_single = ghk_single_channel_P(7.0f, 1.0f, GHK_CBAR_K_MM);  // γ KCNQ2/3 native ≈ 7 пС (Wang 1998)
    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * (m_val*pip2);
    float I_K = (float)(I_A * A_TO_PA);

    float volume = segment_volume(s);
    float charge_per_mole_vol = FARADAY_CONST * volume / 1000.0f;
    s->intra_ions[ION_K] -= (I_K * dt) / charge_per_mole_vol;
    s->extra_ions[ION_K] += (I_K * dt) / (charge_per_mole_vol * EXTRA_VOLUME_RATIO);

    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);

    return -I_K;  // K⁺ выходит = гиперполяризующий (GHK-масштаб, без 0.01f)
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_KV7_MCURRENT, kv7_mcurrent_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Kv7 M-ток (KCNQ2/3)","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":112,"name":"Kv7 (KCNQ2/3) M-ток — медленный K⁺","category":"Ионные каналы","description":"Kv7 (KCNQ2/3) — медленный потенциал-зависимый K⁺ канал БЕЗ инактивации. Формирует M-ток (muscarinic-sensitive). Медленная активация ~100ms (самый медленный Kv). PIP2-зависимость (state[6]=0→закрыт). Ключевая роль: регуляция возбудимости, spike frequency adaptation, подпороговый резонанс. State[0]=m_val, [1]=V_half(-38mV), [2]=k(10mV), [3]=g_max(0.5nS), [4]=tau_act(100ms), [5]=tau_deact(60ms), [6]=PIP2(1.0). GHK-поток K⁺.","release":false}]})BIOCORE";
}
