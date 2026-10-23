// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mapk_erk.cpp — MAPK/ERK каскад консолидации
// Тип: MOD_MAPK_ERK (87)
// Категория: Консолидация/Реконсолидация

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double mapk_erk_current_impl(Modifier* m, Segment* s, float dt) {
    float Ca = s->intra_ions[ION_CA] * 1000.0f; // uM
    double Kd_Ca = (m->state[4] > 0.0f) ? m->state[4] : 0.5f;
    double tau_erk = (m->state[5] > 0.0f) ? m->state[5] : 5000.0f;
    double tau_nuc = (m->state[6] > 0.0f) ? m->state[6] : 30000.0f;
    dt *= m->density;  // enzyme_amount

    // Ca2+ -> Ras (Hill n=3)
    float Ca3 = Ca * Ca * Ca;
    float Kd3 = Kd_Ca * Kd_Ca * Kd_Ca;
    float Ras_inf = Ca3 / (Ca3 + Kd3 + 1e-9f);
    m->state[2] += (Ras_inf - m->state[2]) * dt / 500.0f;

    // Ras -> MEK
    m->state[1] += (m->state[2] - m->state[1]) * dt / 1000.0f;

    // MEK -> ERK (dual phosphorylation, cooperative)
    double ERK_inf = m->state[1] * m->state[1];
    m->state[0] += (ERK_inf - m->state[0]) * dt / tau_erk;

    // ERK -> nuclear translocation
    m->state[3] += (m->state[0] * 0.8f - m->state[3]) * dt / tau_nuc;

    for (int i = 0; i < 4; i++) {
        if (m->state[i] < 0.0f) m->state[i] = 0.0f;
        if (m->state[i] > 1.0f) m->state[i] = 1.0f;
    }

    // Cross-talk: nuclear ERK boosts CREB pCREB on same segment
    for (uint8_t j = 0; j < s->modifier_count; j++) {
        if (s->modifiers[j].type == MOD_NB_CREB_TRANSCRIPTION) {
            s->modifiers[j].state[0] += m->state[3] * 0.01f * dt / 1000.0f;
            if (s->modifiers[j].state[0] > 1.0f) s->modifiers[j].state[0] = 1.0f;
        }
    }

    return 0.0f;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_MAPK_ERK, mapk_erk_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"MAPK/ERK Cascade","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":87,"name":"MAPK/ERK Cascade","category":"Consolidation/Reconsolidation","description":"MAPK/ERK consolidation cascade: Ca2+->Ras->Raf->MEK->ERK->pCREB. ERK nuclear translocation. Cross-talk: pERK boosts CREB transcription on same segment.","release":false}]})BIOCORE";
}
