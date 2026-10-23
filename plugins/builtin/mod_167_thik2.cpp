// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_167_thik2.cpp — THIK-2 (K2P12.1/KCNK12), фоновая K⁺-утечка
// Тип: MOD_THIK2 (167)
//
// ⚠️ THIK-2 гомомерно НЕфункционален: задержан в эндоплазматическом ретикулуме
// (ЭПР) и не выходит на плазматическую мембрану (Rajan 2001, PMID 11060316).
// Единичная проводимость γ НЕ измерена — используется placeholder γ=5 пС, Po=0.5.
// GHK-поток K⁺ (z=1), return -I_K (K⁺-эффлюкс → гиперполяризация).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include <cmath>

static double thik2_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    // ⚠️ Гомомерно нефункционален — placeholder (γ не измерена, Rajan 2001).
    float Po = 0.5f;        // placeholder (нефункционален in vivo)
    float gamma_ps = 5.0f;  // placeholder ~5 пС

    float K_i = s->intra_ions[ION_K];
    float K_o = s->extra_ions[ION_K];
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * Po;
    float I_K = (float)(I_A * A_TO_PA);

    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);
    s->intra_ions[ION_K] -= (I_K * dt) / cpm;
    s->extra_ions[ION_K] += (I_K * dt) / (cpm * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);

    return -I_K;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_THIK2, thik2_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"THIK-2 фоновая K⁺-утечка","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":167,"name":"THIK-2 фоновая K⁺-утечка (K2P12.1/KCNK12)","category":"Leak K","description":"THIK-2 (K2P12.1/KCNK12) — двухпоровый K⁺-канал фоновой утечки. ⚠️ Гомомерно НЕфункционален (задержан в ЭПР, Rajan 2001) — γ не измерена, placeholder γ=5 пС, Po=0.5. GHK-флюкс K⁺ (z=1).","release":false}]})BIOCORE";
}
