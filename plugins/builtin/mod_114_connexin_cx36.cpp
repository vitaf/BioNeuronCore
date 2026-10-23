// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_114_connexin_cx36.cpp — Коннексин-36 (gap junction молекулярная биология)
// Тип: MOD_CONNEXIN_CX36 (114)
//
// Cx36 — основной нейрональный коннексин. Синтез в ER → сборка коннексона
// (6×Cx36 → гексамер) → транспорт → мембрана. Полужизнь ~1-3 часа.
// Фосфорилирование: CaMKII → ↓P_open, PKA(cAMP) → ↑P_open.
// pH-зависимость: закрытие при pH<6.8.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double connexin_cx36_current_impl(Modifier* m, Segment* s, float dt) {
    double synth = (m->state[2] > 0.0f) ? m->state[2] : 0.001f;
    double degrad = (m->state[3] > 0.0f) ? m->state[3] : 0.0005f;
    double pH = (m->state[5] > 4.0f) ? m->state[5] : 7.2f;

    m->state[0] += (synth - degrad * m->state[0]) * dt / 3600000.0f;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    float camkii_act = 0.0f;
    for (uint8_t j = 0; j < s->modifier_count; j++)
        if (s->modifiers[j].type == 82) camkii_act = s->modifiers[j].state[0]; // MOD_CAMKII

    double p_target = camkii_act * 0.6f + m->state[6] * 0.3f;
    m->state[1] += (p_target - m->state[1]) * dt / 5000.0f;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    float pH_f = (pH < 6.8f) ? (pH - 6.0f) / 0.8f : 1.0f;
    if (pH_f < 0.0f) pH_f = 0.0f;
    m->state[4] = (0.5f*(1.0f-m->state[1]*0.5f)+0.5f*(1.0f+m->state[1]*0.3f))*pH_f;
    if (m->state[4] < 0.05f) m->state[4] = 0.05f;
    if (m->state[4] > 2.0f)  m->state[4] = 2.0f;

    return 0.0f;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_CONNEXIN_CX36, connexin_cx36_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Коннексин-36 (Cx36)","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":114,"name":"Cx36 — синтез/сборка/фосфор./деград. gap jct","category":"Синаптическая биология","description":"Cx36 — основной нейрональный коннексин. Синтез→сборка коннексона(6×Cx36)→мембрана. Полужизнь 1-3ч. CaMKII→↓P_open, PKA(cAMP)→↑P_open. pH<6.8→закрытие. State[0]=connexon_count, [1]=p_Cx36, [2]=synth_rate, [3]=degrad_rate, [4]=P_open, [5]=pH, [6]=cAMP. Интегрирован в gap junction проводимость.","release":false}]})BIOCORE";
}
