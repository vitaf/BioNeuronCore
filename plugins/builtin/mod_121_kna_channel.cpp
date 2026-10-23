// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_121_kna_channel.cpp — Slack/Slick KNa канал (KCNT1/KCNT2)
// Na⁺-активируемый K⁺ канал. Slow AHP при burst firing.
// Активация: Hill n≈3 от [Na⁺]in, EC50≈40mM.
// Тип: MOD_KNA_CHANNEL (121). GHK-версия.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double kna_channel_current_impl(Modifier* m, Segment* s, float dt) {
    float area = effective_area(m, s);

    // state[0] = open_prob (dynamic)
    // state[1] = Na⁺ EC50 (мМ, default 41 — Slack Yuan 2003)
    // state[3] = γ (пС, single-channel, default 180 — Slack KCNT1)
    // state[4] = τ (мс, default 800 — медленный slow AHP)
    // Hill (3.4) — константа в modifier_helpers.h (KNA_HILL).
    double open_prob = m->state[0];
    double EC50 = (m->state[1] > 0.0f) ? m->state[1] : KNA_NA_EC50_MM;
    double gamma_ps = (m->state[3] > 0.0f) ? m->state[3] : KNA_GAMMA_PS;
    double tau = (m->state[4] > 0.0f) ? m->state[4] : KNA_TAU_MS;
    // ✅ ДОСТОВЕРНО — Slack (KCNT1); менять ТОЛЬКО с согласования (алгоритм аудита модификаторов).

    // Na⁺-зависимость: Hill по [Na⁺]in (Slack, чисто Na⁺-гейт, нет V).
    float Na_in = s->intra_ions[ION_NA];  // моль/м³ (= мМ)
    float Na_n = powf(Na_in / EC50, KNA_HILL);
    float m_inf = Na_n / (1.0f + Na_n);

    float alpha = dt / tau;
    if (alpha > 1.0f) alpha = 1.0f;
    open_prob += (m_inf - open_prob) * alpha;
    if (open_prob < 0.0f) open_prob = 0.0f;
    if (open_prob > 1.0f) open_prob = 1.0f;
    m->state[0] = open_prob;

    // GHK-поток K⁺ (z=1), P в см/с — честная P-шкала γ→P.
    float K_in  = s->intra_ions[ION_K];  // моль/м³
    float K_out = s->extra_ions[ION_K];
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(s->V_m, K_in, K_out);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    float I_K = (float)(I_A * A_TO_PA);

    // Прямое движение K⁺ (единый объём с fallback: segment_volume).
    float volume = segment_volume(s);
    float cpv = FARADAY_CONST * volume / 1000.0f;
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);

    return -I_K;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_KNA_CHANNEL, kna_channel_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"KNa канал (Slack/Slick, KCNT1/KCNT2)","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":121,"name":"KNa канал — Na⁺-активируемый K⁺ (Slack/Slick, KCNT1/2)","category":"Ионные каналы ритмогенеза","description":"Na⁺-активируемый K⁺ канал. Slow AHP при burst firing. Hill n≈3 от [Na⁺]in, EC50≈40mM, tau≈800ms. Обеспечивает Ca²⁺-независимую терминацию длинных burst-ов. Экспрессируется в спинальных интернейронах (dIN, cIN).","release":false}]})BIOCORE";
}
