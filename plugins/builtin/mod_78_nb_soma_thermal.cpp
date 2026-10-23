// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_78_nb_soma_thermal.cpp — NB_SomaThermal
// Тип: MOD_NB_SOMA_THERMAL (78)
// Категория: Сома-модификаторы
// Термальная регуляция клетки: сома как тепловое ядро.
// Температура диффундирует через IntraConnection (шаг 2.5 симуляции).
// Источники тепла: Na/K-АТФаза (ATP→ADP), ионные токи (джоулево тепло),
// митохондриальное дыхание (MOD_NB_SOMA_ATP).
//
// Q10-модификаторы рецепторов (MoradiFullNMDAR) уже зависят от s->temperature,
// поэтому тепловая модель автоматически влияет на всю кинетику.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

#define NB_SOMA_THERMAL_T_BODY   37.0f    // базовая температура тела (°C)
#define NB_SOMA_THERMAL_T_MIN    30.0f    // минимальная физиологическая
#define NB_SOMA_THERMAL_T_MAX    42.0f    // максимальная (heat shock)

static double nb_soma_thermal_impl(Modifier* m, Segment* s, float dt) {
    // state[0] = кумулятивный тепловой стресс (heat shock proxy)
    double heat_stress = m->state[0];

    // Терморегуляция: дрейф к T_body с постоянной времени
    float T = s->temperature;
    double p_tau_thermal = (m->state[1] > 0.0f) ? m->state[1] : 5000.0f;

    // Дрейф к базовой температуре (гомеостаз)
    T += (NB_SOMA_THERMAL_T_BODY - T) * dt / p_tau_thermal;

    // Ограничение
    if (T < NB_SOMA_THERMAL_T_MIN) T = NB_SOMA_THERMAL_T_MIN;
    if (T > NB_SOMA_THERMAL_T_MAX) T = NB_SOMA_THERMAL_T_MAX;

    // Heat stress: если T долго выше 39°C — накапливается
    if (T > 39.0f) {
        heat_stress += (T - 39.0f) * dt * 0.0001f;
    } else {
        heat_stress -= heat_stress * dt * 0.001f;  // медленное восстановление
    }
    if (heat_stress < 0.0f) heat_stress = 0.0f;
    if (heat_stress > 1.0f) heat_stress = 1.0f;

    s->temperature = T;
    m->state[0] = heat_stress;

    return 0.0f;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NB_SOMA_THERMAL, nb_soma_thermal_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"NB_SomaThermal","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":78,"name":"NB_SomaThermal","category":"Сома-модификаторы","description":"Термальная регуляция: сома как тепловое ядро, гомеостаз T_body=37°C","release":false}]})BIOCORE";
}
