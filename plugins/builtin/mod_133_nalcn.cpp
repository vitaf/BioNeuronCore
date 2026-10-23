// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_133_nalcn.cpp — NALCN: Na⁺-leak канал (non-selective cation)
// Тип: MOD_NALCN (133)
//
// NALCN (Sodium Leak Channel, Non-selective) — НЕЗАВИСИМЫЙ от напряжения
// катионный канал утечки. Преимущественно Na⁺ (P_Na:P_K:P_Ca ≈ 1.3:1.3:0.5).
// Задаёт покоящуюся Na⁺-проводимость и потенциал покоя вместе с K2P (K⁺).
// Реверс ≈ 0..-20 мВ (non-selective). Нет гейтинга — всегда открыт.
// Мутации NALCN → тяжёлые неврологические синдромы (критичен для возбудимости покоя).
//
// Архитектура: раздельные GHK-потоки Na⁺ (внутрь), K⁺ (наружу), Ca²⁺ (внутрь, z=2).
// Покоящийся Na⁺-вход уравновешивает Na⁺/K⁺-АТФазу → стабильный Na_i.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double nalcn_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    // state[0] = gamma (single-channel проводимость, пС, default 27 — Lu/2022)
    double gamma_ps = (m->state[0] > 0.0f) ? m->state[0] : 27.0f;

    // ✅VERIFIED (Lu 2007): P_Na:P_K:P_Ca = 1.3:1.2:0.5 — nonselective (WT), P в см/с
    // ✅ ДОСТОВЕРНО — менять ТОЛЬКО с согласования с пользователем (алгоритм аудита модификаторов).
    // Парциальные проницаемости [м³/с] (chord-conductance разложение γ по 1.3:1.2:0.5)
    double P_Na, P_K, P_Ca;
    nalcn_partial_P(gamma_ps, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    float Ina = (float)(P_Na * ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]) * N * A_TO_PA);
    float Ik  = (float)(P_K  * ghk_monovalent(V, s->intra_ions[ION_K],  s->extra_ions[ION_K])  * N * A_TO_PA);
    float Ica = (float)(P_Ca * ghk_divalent(V,  s->intra_ions[ION_CA], s->extra_ions[ION_CA]) * N * A_TO_PA);

    float vol = segment_volume(s);
    float cpm = FARADAY_CONST * vol / 1000.0f;
    s->intra_ions[ION_NA] -= (Ina * dt) / cpm;   // Na⁺ внутрь (ghk отриц. → -= даёт рост)
    s->intra_ions[ION_K]  -= (Ik  * dt) / cpm;   // K⁺ наружу
    s->intra_ions[ION_CA] -= (Ica * dt) / (2.0f * cpm);  // Ca²⁺ внутрь (z=2)
    // Консервация ионов: внеклетка — резервуар ×10 (как HCN/K2P).
    s->extra_ions[ION_NA] += (Ina * dt) / (cpm * EXTRA_VOLUME_RATIO);
    s->extra_ions[ION_K]  += (Ik  * dt) / (cpm * EXTRA_VOLUME_RATIO);
    s->extra_ions[ION_CA] += (Ica * dt) / (2.0f * cpm * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);

    return -(Ina - Ik + Ica);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NALCN, nalcn_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"NALCN (Na+ leak)","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":133,"name":"NALCN","category":"Leak","description":"Na+ leak channel (non-selective cation). Voltage-independent, P_Na:P_K:P_Ca=1.3:1.3:0.5. Sets resting Na+ permeability and resting potential with K2P. Reversal ~0..-20mV.","release":false}]})BIOCORE";
}
