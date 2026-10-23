// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mmp9.cpp — MMP-9, металлопротеиназа, ремоделирование ECM
// Тип: MOD_MMP9 (92)
// Категория: Консолидация/Реконсолидация

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double mmp9_current_impl(Modifier* m, Segment* s, float dt) {
    double tau_mmp9 = (m->state[4] > 0.0f) ? m->state[4] : 20000.0f;
    double tau_timp = (m->state[5] > 0.0f) ? m->state[5] : 15000.0f;
    double tau_ecm = (m->state[6] > 0.0f) ? m->state[6] : 100000.0f;
    dt *= m->density;  // enzyme_amount

    // MMP-9 transcription depends on ERK + NF-kB (nuclear)
    float erk = 0.0f, nfkb = 0.0f;
    for (uint8_t j = 0; j < s->modifier_count; j++) {
        if (s->modifiers[j].type == MOD_MAPK_ERK) erk = s->modifiers[j].state[3];
        if (s->modifiers[j].type == MOD_IKK_NFKB) nfkb = s->modifiers[j].state[3];
    }
    float mmp9_transcription = (erk + nfkb) * 0.5f;

    // MMP-9 activation
    m->state[0] += (mmp9_transcription - m->state[0]) * dt / tau_mmp9;
    // TIMP anti-correlated
    double timp_target = 1.0f - m->state[0] * 0.5f;
    m->state[1] += (timp_target - m->state[1]) * dt / tau_timp;
    // ECM degradation vs synthesis
    double degradation = m->state[0] * (1.0f - m->state[1]) * 0.01f;
    float synthesis = 0.005f;
    m->state[2] += (synthesis - degradation * m->state[2]) * dt / tau_ecm;
    if (m->state[2] < 0.1f) m->state[2] = 0.1f;
    if (m->state[2] > 1.0f) m->state[2] = 1.0f;
    // Integrin activation (inverse to ECM density)
    double integrin_inf = 1.0f - m->state[2];
    m->state[3] += (integrin_inf - m->state[3]) * dt / 10000.0f;

    for (int i = 0; i < 4; i++) {
        if (m->state[i] < 0.0f) m->state[i] = 0.0f;
        if (m->state[i] > 1.0f) m->state[i] = 1.0f;
    }

    return 0.0f;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_MMP9, mmp9_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"MMP-9 (ECM Remodeling)","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":92,"name":"MMP-9 (ECM Remodeling)","category":"Consolidation/Reconsolidation","description":"MMP-9 — matrix metalloproteinase, ECM remodeling. ERK+NF-kB->MMP-9 transcription->ECM degradation. TIMP inhibits. Integrins sense ECM stiffness. Clears territory for structural plasticity.","release":false}]})BIOCORE";
}
