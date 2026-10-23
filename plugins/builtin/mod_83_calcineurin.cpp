// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// calcineurin.cpp — кальциневрин (PP2B), Ca²⁺/кальмодулин-зависимая фосфатаза
// Тип: MOD_CALCINEURIN (83)
// Категория: STDP Cascade

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double calcineurin_current_impl(Modifier* m, Segment* s, float dt) {
    float Ca = s->intra_ions[ION_CA];       // mM
    float Ca_uM = Ca * 1000.0f;             // -> uM
    double Kd = (m->state[1] > 0.0f) ? m->state[1] : 0.05f;
    double tau = (m->state[2] > 0.0f) ? m->state[2] : 300.0f;
    dt *= m->density;  // enzyme_amount

    // Михаэлис-Ментен (n_Hill~2 для кальциневрина)
    float Ca_n = Ca_uM * Ca_uM;
    float CaN_inf = Ca_n / (Ca_n + Kd*Kd + 1e-9f);

    m->state[0] += (CaN_inf - m->state[0]) * dt / tau;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    // Ток не генерирует — только обновляет state[]
    return 0.0f;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_CALCINEURIN, calcineurin_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Calcineurin (PP2B)","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":83,"name":"Calcineurin (PP2B)","category":"STDP Cascade","description":"Ca2+/calmodulin-dependent phosphatase (PP2B). Very high Ca2+ affinity (Kd=0.05uM). No autophosphorylation — faster deactivation than CaMKII. Key LTD driver.","release":false}]})BIOCORE";
}
