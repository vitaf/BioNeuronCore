// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// semicircular_cupula.cpp — Купула полукружного канала (вестибулярный аппарат)
// Тип: MOD_SEMICIRCULAR_CUPULA (117). GHK-версия.
// Торсионный маятник: d²θ/dt² + 2ζω₀·dθ/dt + ω₀²·θ = γ·α(t).
// Билатеральный push-pull: левый/правый каналы работают антагонистично.
// Фазический ответ (τ=10ms Ca²⁺-адаптация), high-pass фильтр угловой скорости.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double semicircular_cupula_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);


    // state[0] = θ — угол отклонения купулы (торсионный маятник)
    // state[1] = α(t) — угловое ускорение (нормализованное, -1..1, из среды)
    double alpha = m->state[1];
    if (alpha < -1.0f) alpha = -1.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    // Параметры торсионного маятника
    double omega0_sq = (m->state[4] > 0.0f) ? m->state[4] : 1.0f;    // ω₀² (жёсткость купулы)
    double two_zeta_omega0 = (m->state[5] > 0.0f) ? m->state[5] : 0.9f; // 2ζω₀ (демпфирование)
    float gamma = 1.0f; // gain: угловое ускорение → купула

    // Численное интегрирование: d²θ/dt² + 2ζω₀·dθ/dt + ω₀²·θ = γ·α
    double omega = m->state[3]; // dθ/dt с предыдущего шага
    double theta = m->state[0];
    float domega_dt = alpha * gamma - two_zeta_omega0 * omega - omega0_sq * theta;
    omega += domega_dt * dt / 1000.0f; // dt в ms → секунды
    theta += omega * dt / 1000.0f;

    // Упругий возврат купулы (τ≈8s — медленная релаксация)
    theta *= expf(-dt / 8000.0f);
    omega *= expf(-dt / 8000.0f);

    if (theta < -1.0f) theta = -1.0f;
    if (theta > 1.0f) theta = 1.0f;
    m->state[0] = theta;
    m->state[3] = omega;

    // Активация волосковой клетки от отклонения купулы
    float abs_deflection = fabsf(theta);
    double hc_sens = (m->active_vars >= 2 && m->state[m->active_vars + 1] > 0.0f)
                    ? m->state[m->active_vars + 1] : 2.5f;
    float hc_target = 1.0f / (1.0f + expf(-hc_sens * (abs_deflection - 0.1f) * 15.0f));

    // Ca²⁺-зависимая адаптация волосковой клетки (фазическая, τ=10ms)
    m->state[2] += (hc_target - m->state[2]) * dt / 10.0f;
    if (m->state[2] < 0.0f) m->state[2] = 0.0f;
    if (m->state[2] > 1.0f) m->state[2] = 1.0f;
    double eff_open = hc_target * (1.0f - 0.6f * m->state[2]); // фазический: адаптация гасит sustained

    // MET-канал: неселективный катионный (как отолит, но более фазический — выше K⁺ селективность)
    // Проницаемости: P_Na=0.55, P_K=0.35, P_Ca=0.10
    double P_Na, P_K, P_Ca;
    ghk_partial_P(MET_GAMMA_PS, 0.55f, 0.35f, 0.10f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area * eff_open;
    float I_Na = (float)(P_Na * ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]) * N * A_TO_PA);
    float I_K  = (float)(P_K  * ghk_monovalent(V, s->intra_ions[ION_K],  s->extra_ions[ION_K])  * N * A_TO_PA);
    float I_Ca = (float)(P_Ca * ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]) * N * A_TO_PA);
    float I_total = I_Na - I_K + I_Ca;

    s->intra_ions[ION_NA] -= (I_Na * dt) / cpm;
    s->intra_ions[ION_K]  -= (I_K  * dt) / cpm;
    s->intra_ions[ION_CA] -= (I_Ca * dt) / (2.0f * cpm);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);

    return -I_total;  // катионный вход = деполяризующий
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_SEMICIRCULAR_CUPULA, semicircular_cupula_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Купула полукружного канала","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":117,"name":"Купула полукружного канала — угловое ускорение","category":"Сенсорные рецепторы","description":"Механотрансдукция полукружного канала (горизонтальный/латеральный). Купула как торсионный маятник (d²θ/dt²+2ζω₀·dθ/dt+ω₀²·θ=γ·α). High-pass фильтр: deflection ∝ angular velocity. Билатеральный push-pull. MET-канал (Na⁺ 55%, K⁺ 35%, Ca²⁺ 10%) с фазической Ca²⁺-адаптацией (τ=10ms). GHK-поток.","release":false}]})BIOCORE";
}
