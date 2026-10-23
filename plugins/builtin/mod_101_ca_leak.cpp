// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_101_ca_leak.cpp — Пассивная Ca²⁺ утечка
// Тип: MOD_CA_LEAK (101)
// Категория: Ca²⁺/K⁺/Cl⁻ гомеостаз
//
// Пассивный входящий Ca²⁺ ток через неселективные катионные каналы утечки.
// Обеспечивает базальный вход Ca²⁺ для уравновешивания PMCA+NCX экструзии.
// Без этого канала [Ca²⁺]i падает до нуля → нарушение Ca²⁺-зависимых процессов.
// GHK-поток Ca²⁺ (z=2), без гейтинга.
//
// Архитектура: прямой GHK-поток Ca²⁺, без суррогатных формул.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double ca_leak_current_impl(Modifier* m, Segment* s, float dt) {
    float area = effective_area(m, s);

    // state[0] = gamma (single-channel γ, пС, default 50 — TRPC7)
    // ✅ ДОСТОВЕРНО — менять ТОЛЬКО с согласования с пользователем (алгоритм аудита модификаторов).
    double gamma_ps = (m->state[0] > 0.0f) ? m->state[0] : 50.0f;

    float V = s->V_m;
    float ca_in  = s->intra_ions[ION_CA];  // моль/м³
    float ca_out = s->extra_ions[ION_CA];
    float na_in  = s->intra_ions[ION_NA];
    float na_out = s->extra_ions[ION_NA];

    // nonselective TRPC7: P_Ca/P_Na ≈ 2 (Ca²⁺-предпочитающий)
    double P_Na, P_K, P_Ca;
    ghk_partial_P(50.0f, 1.0f, 0.0f, 2.0f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Na = ghk_monovalent(V, na_in, na_out);
    double flux_Ca = ghk_divalent(V, ca_in, ca_out);
    float I_Na = (float)(P_Na * flux_Na * N * A_TO_PA);
    float I_Ca = (float)(P_Ca * flux_Ca * N * A_TO_PA);

    // Прямое перемещение ионов внутрь
    float volume = segment_volume(s);
    float charge_per_mole_vol = FARADAY_CONST * volume / 1000.0f;
    s->intra_ions[ION_NA] -= (I_Na * dt) / charge_per_mole_vol;
    s->intra_ions[ION_CA] -= (I_Ca * dt) / (2.0f * charge_per_mole_vol);

    plugin_clamp_ions(s->intra_ions, ION_COUNT);

    return -(I_Na + I_Ca);  // входящий ток = деполяризующий
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_CA_LEAK, ca_leak_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Ca²⁺ утечка","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":101,"name":"Ca²⁺ утечка — пассивный вход Ca²⁺","category":"Ca²⁺/K⁺/Cl⁻ гомеостаз","description":"Пассивный входящий Ca²⁺ ток через неселективные катионные каналы. GHK-поток Ca²⁺ (z=2), без гейтинга. Обеспечивает базальный вход Ca²⁺ для уравновешивания PMCA+NCX. Поддерживает стационарный [Ca²⁺]i ≈ 50-100 nM.","release":false}]})BIOCORE";
}
