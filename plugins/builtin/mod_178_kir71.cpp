// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_178_kir71.cpp — Kir7.1 (KCNJ13) фоновый K⁺
// Тип: MOD_KIR71 (178). Категория: Inward-rectifier K
// Био: Kir7.1 — фоновый K⁺-канал хориоидного сплетения (Krapivinsky 1998).
// GHK-парадигма (γ=0.05 пС), time-independent утечка (open=1.0, нет инактивации).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double kir71_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    // time-independent утечка (нет инактивации)
    float open = 1.0f;

    double P_single = ghk_single_channel_P(KIR71_GAMMA_PS, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double I_A = P_single * flux * (m->density * area) * open;
    float I = (float)(I_A * A_TO_PA);

    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);
    s->intra_ions[ION_K] -= (I * dt) / cpm;
    s->extra_ions[ION_K] += (I * dt) / (cpm * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);

    return -I;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_KIR71, kir71_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Kir7.1 (KCNJ13) фоновый K⁺ канал","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":178,"name":"Kir7.1 (KCNJ13) фоновый K⁺","category":"Inward-rectifier K","description":"Kir7.1 (KCNJ13) фоновый K⁺-канал хориоидного сплетения. Time-independent утечка (open=1.0, нет инактивации). γ=0.05 пС. GHK K⁺ (z=1). Krapivinsky 1998.","release":false}]})BIOCORE";
}
