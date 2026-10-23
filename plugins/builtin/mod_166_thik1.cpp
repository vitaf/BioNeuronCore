// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// thik1.cpp — THIK-1 фоновая K⁺-утечка
// Тип: MOD_THIK1 (166)
// Категория: Leak

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double thik1_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float Po = 0.5f;  // ⚠️ Po не опубликована — константа
    double hal = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;  // галотан (мМ)
    Po /= (1.0f + hal / HALOTHANE_IC50_THIK);

    float gamma_ps = 5.0f;  // THIK-1 γ≈5 пС
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
    register_modifier_current(MOD_THIK1, thik1_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"THIK-1 background K+ leak","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":166,"name":"THIK-1 background K+ leak","category":"Leak","description":"Background K+ leak channel (THIK-1, K2P13.1/KCNK13). Voltage/time-independent. Po=0.5 (const, not published). gamma~5 pS. Halothane block (IC50 2.8 mM). GHK flux K+ (z=1).","release":false}]})BIOCORE";
}
