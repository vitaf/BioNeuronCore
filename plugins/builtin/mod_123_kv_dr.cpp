// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_123_kv_dr.cpp — Delayed rectifier K⁺ канал (Kv1.1, KCNA1)
// Тип: MOD_KV_DELAYED_RECTIFIER (123)
// Категория: Ионные каналы / K⁺ каналы
//
// Быстрый потенциал-зависимый K⁺ канал без инактивации.
// Основной механизм реполяризации потенциала действия.
// Изоформа Kv1.1 (KCNA1). Kv2.1 (KCNB1) — отдельный канал (MOD_KV21).
// GHK K⁺ flux с полным ионным трекингом.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    // state[0] = m (activation gate, dynamic)
    // state[1] = V½ (мВ, default −30 — Kv1.1)
    // state[2] = k (мВ, default 6 — Kv1.1)
    // state[3] = (зарезервирован — g_max убран, одномолекулярная GHK через γ)
    // state[4] = τ override (мс, 0 = V-зависимый Zerr 1998)
    double m_val = m->state[0];
    double V_half = (m->state[1] != 0.0f) ? m->state[1] : KDR_VHALF_MV;
    double k = (m->state[2] > 0.0f)  ? m->state[2] : KDR_K_MV;
    double tau = (m->state[4] > 0.0f)  ? m->state[4] : kv_dr_m_tau(V, V_half);
    // ✅ ДОСТОВЕРНО — Kv1.1; менять ТОЛЬКО с согласования (алгоритм аудита модификаторов).

    // m-гейт (Больцман, Kv1.1)
    float m_inf = 1.0f / (1.0f + expf(-(V - V_half) / (k + 1e-6f)));
    m_val += (m_inf - m_val) * dt / (tau + 1e-6f);
    if (m_val < 0.0f) m_val = 0.0f;
    if (m_val > 1.0f) m_val = 1.0f;
    m->state[0] = m_val;

    // Одномолекулярная GHK: I_K = P_single·flux·(density·area)·m (Kv1.1 delayed rectifier).
    float K_i = s->intra_ions[ION_K];  // моль/м³
    float K_o = s->extra_ions[ION_K];
    double P_single = ghk_single_channel_P(KDR_GAMMA_PS, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * m_val;
    float I_K = (float)(I_A * A_TO_PA);

    // Прямое движение K⁺ (единый объём с fallback: segment_volume).
    float volume = segment_volume(s);
    float charge_per_mole_vol = FARADAY_CONST * volume / 1000.0f;
    s->intra_ions[ION_K] -= (I_K * dt) / charge_per_mole_vol;
    // Биологическая точность: K+ из клетки → во внеклеточное пространство
    // (объём внеклетки ~10× внутриклеточного)
    s->extra_ions[ION_K] += (I_K * dt) / (charge_per_mole_vol * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);

    return -I_K;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_KV_DELAYED_RECTIFIER, impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return "{"
        "\"id\": 123,"
        "\"name\": \"Kv Delayed Rectifier\","
        "\"full_name\": \"Kv1.1 (KCNA1) Delayed Rectifier K+ Channel\","
        "\"category\": \"Ion channels / K+ channels\","
        "\"description\": \"Fast voltage-gated K+ channel without inactivation. Primary spike repolarization mechanism. Kv1.1 (KCNA1): V1/2=-30mV, k=6mV, tau=2ms, gamma=12pS. GHK K+ flux (single-molecule P_single x flux x N x m) with ion tracking.\","
        "\"electrogenic\": true,"
        "\"ions\": [\"K+\"],"
        "\"state_vars\": [\"m\", \"V_half\", \"k\", \"g_max\", \"tau\"]"
        "}";
}
