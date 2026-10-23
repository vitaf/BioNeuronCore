// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// camkii.cpp — Ca²⁺/кальмодулин-зависимая киназа II
// Тип: MOD_CAMKII (82)
// Категория: STDP Cascade

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double camkii_current_impl(Modifier* m, Segment* s, float dt) {
    float Ca = s->intra_ions[ION_CA];       // mM
    float Ca_uM = Ca * 1000.0f;             // -> uM
    double Kd = (m->state[2] > 0.0f) ? m->state[2] : 1.0f;
    double tau = (m->state[3] > 0.0f) ? m->state[3] : 2000.0f;
    double autophos_rate = (m->state[5] > 0.0f) ? m->state[5] : 0.0005f;
    dt *= m->density;  // enzyme_amount

    // Hill-активация: Ca²⁺ связывается с кальмодулином (n=4, высокая кооперативность)
    float Ca_n = Ca_uM * Ca_uM * Ca_uM * Ca_uM;
    float CaMKII_inf = Ca_n / (Ca_n + Kd*Kd*Kd*Kd + 1e-9f);

    // Динамика: быстрая Ca²⁺-зависимая активация + медленная автофосфориляция
    m->state[0] += (CaMKII_inf - m->state[0]) * dt / tau;
    m->state[1] += (m->state[0] * autophos_rate - m->state[1] * 0.0001f) * dt;

    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    // Ток не генерирует — только обновляет state[]
    return 0.0f;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_CAMKII, camkii_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"CaMKII (Ca2+/calmodulin kinase II)","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":82,"name":"CaMKII (Ca2+/calmodulin kinase II)","category":"STDP Cascade","description":"Ca2+/calmodulin-dependent kinase II. Hill-4 activation by Ca2+. Autophosphorylation (Thr286) gives Ca2+-independent activity. Key LTP driver: phosphorylates AMPAR -> exocytosis.","release":false}]})BIOCORE";
}
