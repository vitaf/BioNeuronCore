// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_113_pp1_inhibitor_darpp32.cpp — PP1/Inhibitor-1/DARPP-32 каскад
// Тип: MOD_PP1_INHIBITOR_DARPP32 (113)
//
// D1→Gs→cAMP→PKA→фосфорилирует DARPP-32 (Thr34) и Inhibitor-1 (Thr35)
// p-DARPP-32 и p-Inhibitor-1 — мощные ингибиторы PP1
// PP1 дефосфорилирует CaMKII-мишени → opposes LTP
// Когда PP1 ингибирован (D1 активен) → LTD подавлен → LTP облегчён
//
// Интеграция: AMPAR trafficking считывает PP1_active (state[3]),
// умножает LTD-скорость. Допамин (через D1) → больше cAMP → больше
// p-DARPP32 → меньше PP1 → LTD ослаблен → LTP доминирует.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double pp1_inhibitor_darpp32_current_impl(Modifier* m, Segment* s, float dt) {
    double tau = (m->state[4] > 0.0f) ? m->state[4] : 5000.0f;
    double basal = (m->state[5] > 0.0f) ? m->state[5] : 0.5f;
    dt *= m->density;  // enzyme_amount

    // PKA из честного пула MOD_CAMP_PKA (135)
    float PKA_act = segment_pka_act(s);

    // PKA → p-DARPP-32 (Thr34) + p-Inhibitor-1 (Thr35)
    m->state[0] += (PKA_act - m->state[0]) * dt / tau;
    m->state[1] += (PKA_act - m->state[1]) * dt / tau;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    // PP1 inhibition: IC50 ~1nM for both p-DARPP-32 and p-Inhibitor-1
    double inhib = 10.0f * m->state[0] + 5.0f * m->state[1];
    m->state[3] = basal / (1.0f + inhib + 1e-9f);
    m->state[6] = PKA_act;

    return 0.0f;  // signaling only
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_PP1_INHIBITOR_DARPP32, pp1_inhibitor_darpp32_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"PP1/Inhibitor-1/DARPP-32","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":113,"name":"PP1/Inhibitor-1/DARPP-32 — LTP-модуляция","category":"Сигнальные каскады пластичности","description":"PP1/Inhibitor-1/DARPP-32 каскад: D1→Gs→cAMP→PKA→p-DARPP32(Thr34)+p-Inhibitor1(Thr35)→inhibit PP1. PP1 дефосфорилирует CaMKII-мишени→oppose LTP. Когда PP1 ингибирован (D1 активен)→LTD подавлен→LTP облегчён. State[0]=p_DARPP32, [1]=p_Inhibitor1, [2]=cAMP, [3]=PP1_active, [4]=tau(5000ms), [5]=basal_PP1(0.5), [6]=PKA_active. Интегрирован в AMPAR trafficking.","release":false}]})BIOCORE";
}
