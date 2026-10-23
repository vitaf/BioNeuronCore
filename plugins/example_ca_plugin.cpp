// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// example_ca_plugin.cpp — пример плагина для BioNeuronCore.
// Регистрирует пользовательский Ca-канал (тип 64).
//
// Сборка:
//   g++ -std=c++17 -fPIC -shared -I../include -o example_ca_plugin.so example_ca_plugin.cpp
//
// Установка: скопировать .so в plugins/ рядом с BioNeuronStudio или указать путь.

#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

// Вспомогательная функция для получения ионных концентраций
static float get_ion(const Segment* s, int ion_idx) {
    if (ion_idx >= 0 && ion_idx < ION_COUNT) return s->intra_ions[ion_idx];
    return 1.0f;
}

// Пользовательский Ca-канал с voltage-gated кинетикой
static float custom_ca_current(Modifier* m, Segment* s, float dt) {
    float Vm        = s->V_m;
    double density = m->density;
    float g_max     = density * 0.5f;  // максимальная проводимость (нСм)

    // Voltage-gated активация Ca-канала
    float v_half  = -20.0f;
    float k_slope = 10.0f;
    float m_inf   = 1.0f / (1.0f + expf(-(Vm - v_half) / k_slope));
    float tau_m   = 2.0f + 8.0f / (1.0f + expf(-(Vm + 40.0f) / 5.0f));  // мс

    // Обновление state-переменной (m — gate activation)
    if (m->active_vars > 0) {
        float& m_val = m->state[0];
        m_val += (dt / tau_m) * (m_inf - m_val);
        m_val = m_val < 0.0f ? 0.0f : (m_val > 1.0f ? 1.0f : m_val);
    } else {
        m->state[0] = m_inf;
        m->active_vars = 1;
    }

    float ca_intra = get_ion(s, ION_CA);
    float ca_extra = 2.0f;  // мМ (типичная внеклеточная концентрация)
    float E_ca = (GAS_CONST * CELSIUS_37 / (2.0f * FARADAY_CONST)) * logf(ca_extra / (ca_intra + 1e-6f)) * 1000.0f;

    double I_ca = g_max * m->state[0] * (Vm - E_ca);

    // Обновление внутриклеточного Ca
    if (ION_CA < ION_COUNT) {
        s->intra_ions[ION_CA] += -I_ca * dt * 0.001f;
        if (s->intra_ions[ION_CA] < 0.0f) s->intra_ions[ION_CA] = 0.0f;
    }

    return I_ca;
}

// Инициализация плагина — регистрирует модификатор
extern "C" int biocore_plugin_init() {
    register_user_modifier(64, custom_ca_current);
    return 1;  // зарегистрирован 1 модификатор
}

// Дескриптор плагина (JSON)
extern "C" const char* biocore_plugin_desc() {
    return R"({
"name":"Примерный Ca-канал",
"version":"1.0.0",
"author":"BioNeuronStudio User",
"modifiers":[
  {"id":64,"name":"Пользовательский Ca-канал","category":"Ионотропные","description":"Модифицированный кальциевый канал с voltage-gated кинетикой и Ca-зависимостью","release":false}
]
})";
}
