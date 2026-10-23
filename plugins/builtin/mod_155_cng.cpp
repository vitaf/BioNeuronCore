// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// cng.cpp — CNG циклический нуклеотид-зависимый катионный канал
// Тип: MOD_CNG (155)
// Категория: Ligand-gated

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double cng_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double cAMP = m->state[4];  // внешний вход (по умолчанию)
    for (uint8_t j = 0; j < s->modifier_count; j++)
        if (s->modifiers[j].type == MOD_CAMP_PKA) { cAMP = s->modifiers[j].state[0]; break; }  // честный пул cAMP (MOD_CAMP_PKA 135)
    if (cAMP < 0.0f) cAMP = 0.0f;
    float Kd = 35.0f;   // µM (CNGA2 гомомер)
    float n  = 1.5f;    // Hill (cAMP)
    float Po_inf = powf(cAMP, n) / (powf(Kd, n) + powf(cAMP, n));
    m->state[0] += (Po_inf - m->state[0]) * dt / 2.0f;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    // Ca²⁺/CaM обратная связь (ингибиция, адаптация): Ca²⁺ через CNG → CaM → CNG закрывается
    float Ca_uM = s->intra_ions[ION_CA] * 1000.0f;  // mM → µM
    float K_cam = 1.0f;  // µM (CaM Kd для Ca²⁺)
    float CaM = powf(Ca_uM, 4.0f) / (powf(K_cam, 4.0f) + powf(Ca_uM, 4.0f));  // Hill 4
    double open_prob = m->state[0] / (1.0f + 2.0f * CaM);  // CaM ингибирует CNG (до ~1/3)

    float gamma_ps = 13.0f;  // γ (гетеромерный нативный; гомомер CNGA2 29 пС)
    double P_Na, P_K, P_Ca;
    ghk_partial_P(13.0f, 1.0f, 0.0f, 3.0f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double flux_Ca = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    float I_Na = (float)(P_Na * flux_Na * N * open_prob * A_TO_PA);
    float I_Ca = (float)(P_Ca * flux_Ca * N * open_prob * A_TO_PA);

    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpm;
    s->extra_ions[ION_NA] += (I_Na * dt) / (cpm * EXTRA_VOLUME_RATIO);
    s->intra_ions[ION_CA] -= (I_Ca * dt) / cpm;
    s->extra_ions[ION_CA] += (I_Ca * dt) / (cpm * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);

    return -(I_Na + I_Ca);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_CNG, cng_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"CNG cyclic nucleotide-gated channel","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":155,"name":"CNG cyclic nucleotide-gated channel","category":"Ligand-gated","description":"CNG (CNGA2/A4/B1b) cyclic nucleotide-gated cation channel. Ligand cAMP (Hill 1.5, Kd 35 uM). gamma=13 pS (heteromeric). Conducts Na+/Ca2+ (PCa/PNa ~3).","release":false}]})BIOCORE";
}
