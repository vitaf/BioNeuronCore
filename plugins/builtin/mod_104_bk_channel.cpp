// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_104_bk_channel.cpp — BK канал (KCa1.1) — быстрая Ca²⁺+V-активируемая реполяризация
// Тип: MOD_BK_CHANNEL (104)
//
// BK (Big-conductance Ca²⁺+voltage-activated K⁺) — быстрый канал реполяризации (fAHP).
// Полный аллостерический механизм Horrigan–Aldrich (2002): 4 вольт-сенсора + 4 Ca²⁺-сайта
// + воротный переход C⇌O, частичная связь D/C/E. K⁺ выходит через GHK (z=1).
//
// Архитектура: прямой GHK-поток K⁺ × open-вероятность HA-модели.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double bk_channel_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    // state[0] = open_prob (dynamic)
    // state[1] = γ (пС, single-channel, default 250 — KCa1.1)
    // state[2] = τ (мс, default 1 — быстрый fAHP)
    double open_prob = m->state[0];
    double gamma_ps = (m->state[1] > 0.0f) ? m->state[1] : BK_GAMMA_PS;
    double tau_ch = (m->state[2] > 0.0f) ? m->state[2] : BK_TAU_MS;
    // ✅ ДОСТОВЕРНО — Horrigan–Aldrich; менять ТОЛЬКО с согласования (алгоритм аудита модификаторов).

    // Мгновенная open-вероятность по Horrigan–Aldrich (4 сенсора + 4 Ca²⁺-сайта).
    float Ca = s->intra_ions[ION_CA];  // моль/м³ (= мМ)
    float po_inf = bk_ha_po(V, Ca);
    open_prob += (po_inf - open_prob) * dt / tau_ch;
    if (open_prob < 0.0f) open_prob = 0.0f;
    if (open_prob > 1.0f) open_prob = 1.0f;
    m->state[0] = open_prob;

    // GHK-поток K⁺ (z=1), P в см/с
    float K_i = s->intra_ions[ION_K];  // моль/м³
    float K_o = s->extra_ions[ION_K];
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    float I_K = (float)(I_A * A_TO_PA);

    float volume = segment_volume(s);
    float charge_per_mole_vol = FARADAY_CONST * volume / 1000.0f;
    s->intra_ions[ION_K] -= (I_K * dt) / charge_per_mole_vol;

    plugin_clamp_ions(s->intra_ions, ION_COUNT);

    return -I_K;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_BK_CHANNEL, bk_channel_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"BK канал (KCa1.1)","version":"1.1.0","author":"BioNeuronCore","modifiers":[{"id":104,"name":"BK канал — Ca²⁺+V-активируемый K⁺ (fAHP)","category":"Ионные каналы ритмогенеза","description":"Высокопроводящий Ca²⁺+V-активируемый K⁺ канал (KCa1.1). Полный аллостерический механизм Horrigan–Aldrich (2002): 4 вольт-сенсора + 4 Ca²⁺-сайта, частичная связь D/C/E. Быстрая кинетика (τ≈1ms). Обеспечивает быструю реполяризацию спайка (fAHP). GHK-поток K⁺.","release":false}]})BIOCORE";
}
