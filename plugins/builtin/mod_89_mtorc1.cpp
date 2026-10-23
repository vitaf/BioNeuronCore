// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mtorc1.cpp — mTORC1, регулятор трансляции (двухключевая активация)
// Тип: MOD_MTORC1 (89)
// Категория: Консолидация/Реконсолидация

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double mtorc1_current_impl(Modifier* m, Segment* s, float dt) {
    double tau_mtor = (m->state[5] > 0.0f) ? m->state[5] : 15000.0f;
    double tau_akt = (m->state[6] > 0.0f) ? m->state[6] : 8000.0f;
    double Kd_AA = (m->state[4] > 0.0f) ? m->state[4] : 0.3f;
    dt *= m->density;  // enzyme_amount

    // Key 1: AA -> Rheb-GTP. AA = state[3] (внешний parametric input — нутриенты).
    double AA = m->state[3];
    float Rheb_inf = AA / (AA + Kd_AA + 1e-9f);
    m->state[1] += (Rheb_inf - m->state[1]) * dt / 5000.0f;

    // Key 2: PI3K/AKT
    float AKT_inf = AA * 0.8f;
    m->state[2] += (AKT_inf - m->state[2]) * dt / tau_akt;

    // Both keys: mTORC1 = Rheb * AKT
    double mTOR_inf = m->state[1] * m->state[2];

    // ATP clamp: low ATP inhibits mTORC1
    float ATP_norm = s->ATP_level / 5.0f;
    if (ATP_norm > 1.0f) ATP_norm = 1.0f;
    if (ATP_norm < 0.0f) ATP_norm = 0.0f;
    mTOR_inf *= ATP_norm;

    m->state[0] += (mTOR_inf - m->state[0]) * dt / tau_mtor;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[2] < 0.0f) m->state[2] = 0.0f;

    return 0.0f;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_MTORC1, mtorc1_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"mTORC1","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":89,"name":"mTORC1","category":"Consolidation/Reconsolidation","description":"mTORC1 — translation regulator. Two-key activation: (1) AA->Rag->Rheb-GTP, (2) PI3K/AKT->TSC inhibition. ATP-dependent inhibition via AMPK. Master switch for protein synthesis and synaptic plasticity.","release":false}]})BIOCORE";
}
