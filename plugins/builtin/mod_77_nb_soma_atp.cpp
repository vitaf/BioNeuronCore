// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_77_nb_soma_atp.cpp — NB_SomaATP
// Тип: MOD_NB_SOMA_ATP (77)
// Категория: Сома-модификаторы
// Митохондриальный синтез ATP в соме.
// ATP_level диффундирует через IntraConnection (шаг 2.5 симуляции).
// Потребляется Na/K-АТФазой (MOD_PUMP_NAK) и другими метаболическими процессами.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

// Параметры синтеза ATP митохондриями
#define NB_SOMA_ATP_MAX_RATE    0.01f    // максимальная скорость синтеза (ед./мс)
#define NB_SOMA_ATP_KM_O2       0.01f    // полунасыщение по кислороду (arb. units)
#define NB_SOMA_ATP_ATP_MAX     5.0f     // максимальный уровень ATP

static double nb_soma_atp_impl(Modifier* m, Segment* s, float dt) {
    // Митохондриальный метаболизм использует состояние[0] как NADH-подобный пул
    double nadh = m->state[0];

    // Синтез ATP: зависит от доступности NADH и уровня ATP (feedback inhibition)
    float atp = s->ATP_level;
    float inhibition = 1.0f - atp / NB_SOMA_ATP_ATP_MAX;
    if (inhibition < 0.0f) inhibition = 0.0f;

    // NADH пополняется глюкозой/пируватом (упрощение: константный приток)
    float glucose_flux = 0.001f;
    nadh += (glucose_flux - nadh * 0.01f) * dt;
    if (nadh < 0.0f) nadh = 0.0f;
    if (nadh > 1.0f) nadh = 1.0f;

    // Синтез ATP
    double p_synth_rate = (m->state[1] > 0.0f) ? m->state[1] : NB_SOMA_ATP_MAX_RATE;
    double atp_synth = p_synth_rate * nadh * inhibition * m->density;
    s->ATP_level += atp_synth * dt;
    if (s->ATP_level > NB_SOMA_ATP_ATP_MAX) s->ATP_level = NB_SOMA_ATP_ATP_MAX;

    m->state[0] = nadh;

    // Митохондрии выделяют тепло
    float heat = atp_synth * 0.5f * dt;
    s->temperature += heat * 0.001f;
    if (s->temperature > 42.0f) s->temperature = 42.0f;

    return 0.0f;  // не генерирует мембранный ток
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NB_SOMA_ATP, nb_soma_atp_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"NB_SomaATP","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":77,"name":"NB_SomaATP","category":"Сома-модификаторы","description":"Митохондриальный синтез ATP в соме, feedback inhibition, NADH-пул","release":false}]})BIOCORE";
}
