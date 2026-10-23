// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// ikk_nfkb.cpp — IKK/NF-κB сигнальный путь реконсолидации
// Тип: MOD_IKK_NFKB (88)
// Категория: Консолидация/Реконсолидация

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double ikk_nfkb_current_impl(Modifier* m, Segment* s, float dt) {
    float Ca = s->intra_ions[ION_CA] * 1000.0f; // uM
    double Kd_Ca = (m->state[4] > 0.0f) ? m->state[4] : 0.3f;
    double tau_nfkb = (m->state[5] > 0.0f) ? m->state[5] : 10000.0f;
    double tau_ikb = (m->state[6] > 0.0f) ? m->state[6] : 5000.0f;
    dt *= m->density;  // enzyme_amount

    // Ca2+ -> IKK
    float IKK_inf = Ca / (Ca + Kd_Ca + 1e-9f);
    m->state[1] += (IKK_inf - m->state[1]) * dt / 2000.0f;

    // IKK -> IkB degradation
    m->state[2] += (1.0f - m->state[1] * 0.9f - m->state[2]) * dt / tau_ikb;
    if (m->state[2] < 0.1f) m->state[2] = 0.1f;

    // NF-kB released from IkB
    double NFkB_free = 1.0f - m->state[2];
    m->state[0] += (NFkB_free - m->state[0]) * dt / tau_nfkb;

    // NF-kB nuclear translocation
    m->state[3] += (m->state[0] * 0.7f - m->state[3]) * dt / (tau_nfkb * 2.0f);

    for (int i = 0; i < 4; i++) {
        if (m->state[i] < 0.0f) m->state[i] = 0.0f;
        if (m->state[i] > 1.0f) m->state[i] = 1.0f;
    }

    // NF-kB adds to transcription signal (reconsolidation)
    s->transcription_signal += m->state[3] * 0.1f;
    if (s->transcription_signal > 1.0f) s->transcription_signal = 1.0f;

    return 0.0f;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_IKK_NFKB, ikk_nfkb_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"IKK/NF-kB Pathway","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":88,"name":"IKK/NF-kB Pathway","category":"Consolidation/Reconsolidation","description":"IKK/NF-kB reconsolidation pathway: Ca2+->IKK->IkB degradation->NF-kB release->nuclear translocation. Adds to transcription_signal. Drives Zif268 expression for memory reconsolidation.","release":false}]})BIOCORE";
}
