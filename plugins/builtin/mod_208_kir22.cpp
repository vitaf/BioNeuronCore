// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_208_kir22.cpp — Kir2.2 (KCNJ12) inward rectifier K⁺ канал.
// Тип: MOD_KIR22 (208). γ=36 пС, конститутивный, Ba²⁺ 2.8 µM.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double kir22_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double gamma_ps = (m->state[3] > 0.0f) ? m->state[3] : KIR22_GAMMA_PS;
    float K_i = s->intra_ions[ION_K], K_o = s->extra_ions[ION_K];
    float f_open = kir_f_open(V);  // Woodhull Mg²⁺/spermine
    // Ba²⁺ блок (state[4], µM) — чувствительнее Kir2.1
    double ba = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;
    f_open /= (1.0f + ba / KIR22_BA_IC50_UM);
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * f_open;
    float I_K = (float)(I_A * A_TO_PA);
    float vol = segment_volume(s);
    float cpv = FARADAY_CONST * vol / 1000.0f;
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);
    return -I_K;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_KIR22, kir22_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Kir2.2 inward rectifier K","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":208,"name":"Kir2.2 (KCNJ12)","category":"Ионные каналы","description":"Kir2.2 inward rectifier K⁺ (γ=36 пС, конститутивный, Ba²⁺ 2.8 µM). GHK K⁺ + Woodhull rectification.","release":false}]})BIOCORE";
}
