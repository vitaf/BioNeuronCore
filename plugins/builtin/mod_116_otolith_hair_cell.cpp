// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// otolith_hair_cell.cpp — Волосковая клетка отолитового органа (вестибулярный аппарат)
// Тип: MOD_OTOLITH_HAIR_CELL (116). GHK-версия.
// Механотрансдукция отолитов (утрикулюс/саккулюс): сдвиг отолитовой мембраны →
// изгиб стереоцилий → открытие MET-каналов → деполяризация.
// Направленное (cosine) тюнингование + Ca²⁺-адаптация миозиновых моторов.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double otolith_hair_cell_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);

    double sensitivity = (m->active_vars >= 1 && m->state[m->active_vars] > 0.0f)
                        ? m->state[m->active_vars] : 5.0f;   // крутизна MET-активации

    // state[0] = MET P_open (тонический ~0.15 — направление гравитации)
    // state[1] = shear_force (линейное ускорение/гравитация, 0..1, из среды)
    double shear = m->state[1];
    if (shear < 0.0f) shear = 0.0f;
    if (shear > 1.0f) shear = 1.0f;

    // Направленное (cosine) тюнингование: hair cell отвечает только на сдвиг в preferred direction
    double cos_theta = (m->state[5] > -99.0f) ? m->state[5] : 1.0f; // default: aligned
    if (cos_theta < -1.0f) cos_theta = -1.0f;
    if (cos_theta > 1.0f) cos_theta = 1.0f;
    float eff_shear = shear * cos_theta;
    if (eff_shear < 0.0f) eff_shear = 0.0f; // hair cell деполяризуется только в preferred direction

    // Ca²⁺-зависимая медленная адаптация (миозин, τ≈30ms) снижает эффективный сдвиг
    double adaptation = m->state[2];
    float adapted = eff_shear * (1.0f - 0.7f * adaptation);

    // Тонический P_open ~0.15 в покое (потенциал покоя волосковой клетки ≈ -60 mV)
    float target_open = 0.15f + 0.85f / (1.0f + expf(-sensitivity * (adapted - 0.15f) * 10.0f));
    m->state[0] += (target_open - m->state[0]) * dt / 1.0f; // τ≈1ms — быстрая механотрансдукция
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    // Ca²⁺-зависимая адаптация миозиновых моторов: dCa_adapt/dt ∝ P_open
    // Больше MET-каналов открыто → больше Ca²⁺ входит → больше slippage → снижение чувствительности
    m->state[2] += (m->state[0] - m->state[2]) * dt / 30.0f;
    if (m->state[2] < 0.0f) m->state[2] = 0.0f;
    if (m->state[2] > 1.0f) m->state[2] = 1.0f;

    // MET-канал: неселективный катионный, TRP-подобный
    // В эндолимфе [K⁺]_ext ≈ 150mM → K⁺-вход = деполяризация
    // Проницаемости (нормированные): P_Na=0.60, P_K=0.30, P_Ca=0.10
    double P_Na, P_K, P_Ca;
    ghk_partial_P(MET_GAMMA_PS, 0.60f, 0.30f, 0.10f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area * m->state[0];
    float I_Na = (float)(P_Na * ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]) * N * A_TO_PA);
    float I_K  = (float)(P_K  * ghk_monovalent(V, s->intra_ions[ION_K],  s->extra_ions[ION_K])  * N * A_TO_PA);
    float I_Ca = (float)(P_Ca * ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]) * N * A_TO_PA);
    float I_total = I_Na - I_K + I_Ca;  // Na⁺/Ca²⁺ вход, K⁺ выход

    s->intra_ions[ION_NA] -= (I_Na * dt) / cpm;
    s->intra_ions[ION_K]  -= (I_K  * dt) / cpm;
    s->intra_ions[ION_CA] -= (I_Ca * dt) / (2.0f * cpm); // Ca²⁺ = divalent
    plugin_clamp_ions(s->intra_ions, ION_COUNT);

    return -I_total;  // катионный вход = деполяризующий
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_OTOLITH_HAIR_CELL, otolith_hair_cell_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Волосковая клетка отолитового органа","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":116,"name":"Волосковая клетка отолита — MET/гравитация","category":"Сенсорные рецепторы","description":"Механотрансдукция отолитов (утрикулюс/саккулюс). Сдвиг отолитовой мембраны → изгиб стереоцилий → открытие MET-каналов (TRP-подобный: Na⁺ 60%, K⁺ 30%, Ca²⁺ 10%). Направленное (cosine) тюнингование. Ca²⁺-адаптация миозиновых моторов (τ=30ms). Тоническая активность ~15% (направление гравитации). GHK-поток.","release":false}]})BIOCORE";
}
