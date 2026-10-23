// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_160_girk.cpp — GIRK (Kir3.x, KCNJ3/5/6/9), Gβγ-активируемый inward-rectifier K⁺.
// Тип: MOD_GIRK (160). Категория: Базовые модификаторы (ионные каналы).
// Био: γ=30–40 пС (гетеромер Kir3.1/3.2); Gβγ-зависимый (state[4]), НЕ V-зависимый;
// inward-выпрямление; tertiapin-Q (state[5], нМ, IC50 8 нМ). GHK K⁺ (z=1).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double girk_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double gbg = m->state[4];  // Gβγ активация (0–1)
    if (gbg < 0.0f) gbg = 0.0f; if (gbg > 1.0f) gbg = 1.0f;
    float Po = gbg;
    // tertiapin-Q (state[5], нМ)
    double tert = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    Po /= (1.0f + tert / TERTIAPINQ_IC50_GIRK);
    float gamma_ps = 35.0f;  // γ гетеромер (⚠️ G в нативных не найдена → density плейсхолдер)
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double I_A = P_single * flux * (m->density * area) * Po;
    float I_K = (float)(I_A * A_TO_PA);
    float vol = segment_volume(s);
    float cpv = charge_per_mole(vol);
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);
    return -I_K;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_GIRK, girk_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"GIRK (Kir3.x) — Gβγ-активируемый inward-rectifier K⁺","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":160,"name":"GIRK (Kir3.x) — Gβγ-активируемый inward-rectifier K⁺","category":"Базовые модификаторы","description":"GIRK (Kir3.1/3.2/3.4, KCNJ3/6/9) Gβγ-зависимый inward-rectifier K⁺. γ=35 пС (30–40 пС), НЕ V-зависимый. GHK K⁺ (z=1). tertiapin-Q IC50 8 нМ.","release":false}]})BIOCORE";
}
