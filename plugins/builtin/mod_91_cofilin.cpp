// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// cofilin.cpp — кофилин, регуляция актинового цитоскелета
// Тип: MOD_COFILIN (91)
// Категория: Консолидация/Реконсолидация

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double cofilin_current_impl(Modifier* m, Segment* s, float dt) {
    double tau_cof = (m->state[4] > 0.0f) ? m->state[4] : 5000.0f;
    double tau_limk = (m->state[5] > 0.0f) ? m->state[5] : 8000.0f;
    double tau_actin = (m->state[6] > 0.0f) ? m->state[6] : 30000.0f;
    dt *= m->density;  // enzyme_amount

    // Read ERK from same segment
    float erk = 0.0f;
    for (uint8_t j = 0; j < s->modifier_count; j++)
        if (s->modifiers[j].type == MOD_MAPK_ERK)
            erk = s->modifiers[j].state[0];

    // ERK/RSK -> LIMK
    m->state[1] += (erk * 0.8f - m->state[1]) * dt / tau_limk;
    // LIMK -> phospho-Cofilin (inactive)
    double pCofilin_target = m->state[1] * 0.9f;
    m->state[2] += (pCofilin_target - m->state[2]) * dt / tau_cof;
    // Active cofilin = 1 - phospho-cofilin
    double cof_target = 1.0f - m->state[2];
    m->state[0] += (cof_target - m->state[0]) * dt / tau_cof;
    // F-actin: polymerization minus severing
    float polymerization = 0.3f;
    double severing = m->state[0] * 0.1f;
    m->state[3] += (polymerization - severing * m->state[3]) * dt / tau_actin;

    for (int i = 0; i < 4; i++) {
        if (m->state[i] < 0.0f) m->state[i] = 0.0f;
        if (m->state[i] > 1.0f) m->state[i] = 1.0f;
    }

    return 0.0f;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_COFILIN, cofilin_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Cofilin (Actin Remodeling)","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":91,"name":"Cofilin (Actin Remodeling)","category":"Consolidation/Reconsolidation","description":"Cofilin — actin cytoskeleton regulator. ERK->LIMK->phospho-Cofilin (inactive). Active cofilin severs F-actin->G-actin. Reads ERK from same segment. Controls spine morphology and structural plasticity.","release":false}]})BIOCORE";
}
