// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_198_connexin_cx43.cpp — Коннексин-43 (глиальный gap junction)
// Тип: MOD_CONNEXIN_CX43 (198)
//
// Cx43 (GJA1) — основной коннексин астроцитов/радиальной глии. Формирует межклеточный
// синцитий для пространственного K⁺-буферинга. Унитарная проводимость γ≈100 пС
// (Veenstra 1995) — в 7× выше нейронального Cx36 (14 пС). Синтез в ER → сборка
// коннексона (6×Cx43 → гексамер) → мембрана. Полужизнь ~1-5 ч.
// Фосфорилирование: CaMKII → ↓P_open, PKA(cAMP) → ↑P_open. pH-зависимость: закрытие pH<6.8.
// Проводимость интегрирована в gap_junction_g_eff (run_tick_common.h).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double connexin_cx43_current_impl(Modifier* m, Segment* s, float dt) {
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
    register_modifier_current(MOD_CONNEXIN_CX43, connexin_cx43_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Коннексин-43 (Cx43)","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":198,"name":"Cx43 — глиальный gap junction (синцитий K-буферинга)","category":"Синаптическая биология","description":"Cx43 (GJA1) — коннексин астроцитов/радиальной глии, γ=100 пС. Синтез→сборка коннексона(6×Cx43)→мембрана. Полужизнь 1-5ч. CaMKII→↓P_open, PKA(cAMP)→↑P_open. pH<6.8→закрытие. State[0]=connexon_count, [1]=p_Cx43, [2]=synth_rate, [3]=degrad_rate, [4]=P_open, [5]=pH, [6]=cAMP. Интегрирован в gap junction проводимость.","release":false}]})BIOCORE";
}
