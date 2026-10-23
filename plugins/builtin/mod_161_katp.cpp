// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// katp.cpp — KATP (Kir6.2/SUR1) ATP-чувствительный K⁺ канал
// Тип: MOD_KATP (161)

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double katp_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    double atp = m->state[4];  // [ATP] µМ
    if (atp < 0.0f) atp = 0.0f;
    float Po = 1.0f / (1.0f + atp / 15.0f);  // IC50 15 µМ

    // глибенкламид (state[5], нМ)
    double glib = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    Po /= (1.0f + glib / GLIBENCLAMIDE_IC50_KATP);

    float gamma_ps = 75.0f;
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
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
    register_modifier_current(MOD_KATP, katp_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"KATP Kir6.2/SUR1","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":161,"name":"KATP Kir6.2/SUR1 ATP-sensitive K+","category":"Inward-rectifier K","description":"KATP (Kir6.2/SUR1) ATP-sensitive K+. ATP-gated (IC50 15 uM), weak inward rectification. Glibenclamide block (IC50 3 nM). GHK K+ (z=1).","release":false}]})BIOCORE";
}
