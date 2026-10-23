// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// pir_ttype_calcium.cpp — T-type Ca²⁺ канал (Cav3.x) — ФАЗИЧЕСКИЙ ОТСКОК
// Тип: MOD_PIR_TTYPE_CALCIUM (96)
//
// Моделирует низкопороговые Ca²⁺ каналы (Cav3.1/3.2/3.3).
// Ключевой механизм постингибиторного отскока:
//   Гиперполяризация → деинактивация h-ворот → выход из торможения →
//   m-ворота открываются быстрее чем h закрываются → окно проводимости →
//   мощный Ca²⁺-поток внутрь через GHK → деполяризация → rebound spike.
//
// Архитектура MindCopy: прямой перенос Ca²⁺ через GHK-уравнение,
// без абстрактного I = g*(V-E). Ионные потоки → изменение концентраций →
// V_m обновляется из баланса зарядов (шаг 4 симуляции).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

double pir_ttype_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    // ── Параметры из state[] ─────────────────────────────────────────
    // state[0] = m_val (активация m³, динамический)
    // state[1] = h_val (деинактивация, динамический)
    // state[2] = gamma (single-channel γ, пС, default 7.2 — Cav3.2)
    // state[3] = V_half_m (V полуактивации m, default −48.4 mV)
    // state[4] = V_half_h (V полудеинактивации h, default −75.6 mV)
    // state[5] = (зарезервирован — τ_h теперь V-зависимый через tca_h_tau)
    // state[6] = EC50_5HTDA (порог 5-HT/DA-подавления, default 0.01)
    // state[7] = effective_area (выставляется в simulation_host)
    // ✅ ДОСТОВЕРНО — менять ТОЛЬКО с согласования с пользователем (алгоритм аудита модификаторов).

    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : TCA_GAMMA_PS;
    double V_half_m = (m->state[3] != 0.0f) ? m->state[3] : TCA_VHALF_M;
    double V_half_h = (m->state[4] != 0.0f) ? m->state[4] : TCA_VHALF_H;
    double EC50_5HTDA = (m->state[6] > 0.0f) ? m->state[6] : 0.01f;

    // ── Гейтинг (m³h-кинетика, 10 подшагов) ────────────────────────
    double m_val = m->state[0];
    double h_val = m->state[1];

    float m_inf = boltzmann_act(V, V_half_m, TCA_K_M);
    float h_inf = boltzmann_inact(V, V_half_h, TCA_K_H);
    float tau_m = 3.0f;   // m-ворота быстрые
    float tau_h = tca_h_tau(V);  // V-зависимая инактивация/де-инактивация (recovery)

    float sub_dt = dt / 10.0f;
    for (int step = 0; step < 10; step++) {
        m_val += (m_inf - m_val) * sub_dt / tau_m;
        h_val += (h_inf - h_val) * sub_dt / tau_h;
    }
    if (m_val < 0.0f) m_val = 0.0f; if (m_val > 1.0f) m_val = 1.0f;
    if (h_val < 0.0f) h_val = 0.0f; if (h_val > 1.0f) h_val = 1.0f;

    m->state[0] = m_val;
    m->state[1] = h_val;

    // ── GHK-поток Ca²⁺ (z=2), P в см/с ────────────────────────────────
    float ca_in  = s->intra_ions[ION_CA];  // моль/м³
    float ca_out = s->extra_ions[ION_CA];
    double P_single = ghk_single_channel_P(gamma_ps, 2.0f, GHK_CBAR_CA_MM);

    // ── 5-HT/DA-подавление ──────────────────────────────────────────
    float ht = s->neurotransmitter[NT_SEROTONIN];
    float da = s->neurotransmitter[NT_DOPAMINE];
    float suppression = 1.0f / (1.0f + (ht + da) / EC50_5HTDA);
    double mod = (1.0 - 0.7 * suppression);  // до 70% подавления

    // Ток через GHK
    float open_prob = m_val * m_val * m_val * h_val;
    double flux = ghk_divalent(V, ca_in, ca_out);
    double I_A = P_single * flux * (m->density * area) * mod * open_prob;
    float I_CaT = (float)(I_A * A_TO_PA);

    // ── Прямое перемещение Ca²⁺ ────────────────────────────────────
    float volume = segment_volume(s);
    float charge_per_mole = FARADAY_CONST * volume / 1000.0f;
    s->intra_ions[ION_CA] -= (I_CaT * dt) / (2.0f * charge_per_mole);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);

    return -I_CaT;  // входящий Ca²⁺ = деполяризующий ток
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_PIR_TTYPE_CALCIUM, pir_ttype_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"T-type Ca²⁺ канал (Cav3)","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":96,"name":"T-type Ca²⁺ (Cav3) — фазический отскок","category":"PIR","description":"Низкопороговый T-type Ca²⁺ канал (Cav3.1/3.2/3.3). GHK-поток Ca²⁺ (z=2), m³h-гейтинг: m-активация деполяризацией (V_half=-38mV), h-деинактивация гиперполяризацией (V_half=-68mV). Окно проводимости при -70..-30mV создаёт фазический rebound spike. 5-HT/DA-подавление через снижение проницаемости. Ключевой механизм CPG головастика (Roberts et al. 2014).","release":false}]})BIOCORE";
}
