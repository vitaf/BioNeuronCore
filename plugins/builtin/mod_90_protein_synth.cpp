// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// protein_synth.cpp — белковый синтез (трансляция), mTORC1-зависимый
// Тип: MOD_PROTEIN_SYNTH (90)
// Категория: Консолидация/Реконсолидация

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double protein_synth_current_impl(Modifier* m, Segment* s, float dt) {
    double tau_s6k = (m->state[4] > 0.0f) ? m->state[4] : 10000.0f;
    double tau_4ebp = (m->state[5] > 0.0f) ? m->state[5] : 8000.0f;
    double tau_translate = (m->state[6] > 0.0f) ? m->state[6] : 60000.0f;
    dt *= m->density;  // enzyme_amount

    // Read mTORC1 from same segment
    float mtorc1 = 0.0f;
    for (uint8_t j = 0; j < s->modifier_count; j++)
        if (s->modifiers[j].type == MOD_MTORC1)
            mtorc1 = s->modifiers[j].state[0];

    // mTORC1 -> S6K1
    m->state[1] += (mtorc1 - m->state[1]) * dt / tau_s6k;
    // mTORC1 -> 4E-BP phosphorylation (releases eIF4E)
    m->state[2] += (mtorc1 - m->state[2]) * dt / tau_4ebp;
    // Combined translation
    double trans_inf = m->state[1] * m->state[2];
    m->state[0] += (trans_inf - m->state[0]) * dt / tau_translate;
    m->state[3] = m->state[0] * 0.9f;

    for (int i = 0; i < 4; i++) {
        if (m->state[i] < 0.0f) m->state[i] = 0.0f;
        if (m->state[i] > 1.0f) m->state[i] = 1.0f;
    }

    // Protein synthesis consumes ATP
    s->ATP_level -= m->state[0] * 0.0001f * dt;
    if (s->ATP_level < 0.0f) s->ATP_level = 0.0f;

    return 0.0f;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_PROTEIN_SYNTH, protein_synth_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Protein Synthesis (Translation)","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":90,"name":"Protein Synthesis (Translation)","category":"Consolidation/Reconsolidation","description":"Protein synthesis (translation). mTORC1->S6K1/S6 + mTORC1->4E-BP->eIF4E — two parallel initiation pathways. Reads mTORC1 from same segment. Consumes ATP. Produces proteins for synaptic plasticity.","release":false}]})BIOCORE";
}
