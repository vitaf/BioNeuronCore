// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_103_sk_channel.cpp — SK канал (KCa2.x) — Ca²⁺-активируемый K⁺, AHP
// Тип: MOD_SK_CHANNEL (103)
//
// SK (Small-conductance Ca²⁺-activated K⁺) — канал послеспайковой
// гиперполяризации (mAHP). Активируется ТОЛЬКО Ca²⁺ (не напряжением).
// Hill n=4, K_d ≈ 400 nM. Медленная кинетика (tau ~50ms закрытие).
// K⁺ выходит через GHK (z=1).
//
// Архитектура: прямой GHK-поток K⁺ + Ca²⁺-зависимый гейтинг.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double sk_channel_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    // state[0] = open_prob (dynamic)
    // state[3] = γ (пС, single-channel, default 10 — KCa2.2)
    // state[4] = τ_open (мс, default 5)
    // state[5] = τ_close (мс, default 50)
    // K_d (0.4 µM) и Hill (4) — константы в modifier_helpers.h (SK_KD_MM, SK_HILL).
    double open_prob = m->state[0];
    double gamma_ps = (m->state[3] > 0.0f) ? m->state[3] : SK_GAMMA_PS;
    double tau_open = (m->state[4] > 0.0f) ? m->state[4] : SK_TAU_OPEN_MS;
    double tau_close = (m->state[5] > 0.0f) ? m->state[5] : SK_TAU_CLOSE_MS;
    // ✅ ДОСТОВЕРНО — менять ТОЛЬКО с согласования (алгоритм аудита модификаторов).

    // Ca²⁺-зависимость: Hill 4 (кальмодулин), чисто Ca²⁺-гейт (нет V).
    float ca = s->intra_ions[ION_CA];  // моль/м³ (= мМ)
    float p_inf = sk_p_inf(ca);

    // Асимметричная кинетика
    float tau = (p_inf > open_prob) ? tau_open : tau_close;
    open_prob += (p_inf - open_prob) * dt / tau;
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

    // Прямое движение K⁺
    float volume = segment_volume(s);
    float charge_per_mole_vol = FARADAY_CONST * volume / 1000.0f;
    s->intra_ions[ION_K] -= (I_K * dt) / charge_per_mole_vol;

    plugin_clamp_ions(s->intra_ions, ION_COUNT);

    return -I_K;  // K⁺ выходит = гиперполяризующий
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_SK_CHANNEL, sk_channel_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"SK канал (KCa2.x)","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":103,"name":"SK канал — Ca²⁺-активируемый K⁺ (AHP)","category":"Ионные каналы ритмогенеза","description":"Малопроводящий Ca²⁺-активируемый K⁺ канал (KCa2.1/2.2/2.3). Чисто Ca²⁺-зависимый (нет V-гейта). Hill n=4, K_d=400nM. Медленная кинетика (τ_open=5ms, τ_close=50ms). Обеспечивает средне-послеспайковую гиперполяризацию (mAHP). GHK-поток K⁺.","release":false}]})BIOCORE";
}
