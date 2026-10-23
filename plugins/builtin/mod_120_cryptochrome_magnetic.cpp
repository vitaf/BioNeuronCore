// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// cryptochrome_magnetic.cpp — Криптохромный магнитный сенсор (радикальные пары)
// Тип: MOD_CRYPTOCHROME_MAGNETIC (120). МОДУЛЯТОР (не генерирует собственный ток).
// Cry4 — флавопротеин: синий свет → FAD* + Trp → [FAD*- + Trp*+] радикальная пара.
// Магнитное поле изменяет спиновое состояние → выход реакции → модуляция светового ответа.
// Наносится на PINEAL-фоторецепторы как дополнительный модификатор.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double cryptochrome_magnetic_current_impl(Modifier* m, Segment* s, float dt) {
    double light = m->state[1];
    if (light < 0.0f) light = 0.0f;
    if (light > 1.0f) light = 1.0f;

    double sin2_theta = m->state[2];
    if (sin2_theta < 0.0f) sin2_theta = 0.0f;
    if (sin2_theta > 1.0f) sin2_theta = 1.0f;

    double wavelength_factor = (m->state[5] > 0.0f) ? m->state[5] : 1.0f;
    double sensitivity = (m->state[4] > 0.0f) ? m->state[4] : 1.0f;

    // Выход радикальной пары: RP ∝ light × sin²(θ) × wavelength
    float target_rp = light * sin2_theta * wavelength_factor * sensitivity;
    m->state[0] += (target_rp - m->state[0]) * dt / 50.0f;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    // Модуляция: найти MOD_LIGHT_SENSOR на этом сегменте и усилить его ответ
    for (int i = 0; i < s->modifier_count; i++) {
        if (s->modifiers[i].type == MOD_LIGHT_SENSOR) {
            float k_cry = 0.3f;
            double boost = 1.0f + k_cry * m->state[0];
            s->modifiers[i].state[0] *= boost;
            if (s->modifiers[i].state[0] > 1.0f) s->modifiers[i].state[0] = 1.0f;
            break;
        }
    }
    return 0.0f; // чисто модуляторный — не генерирует собственный ток
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_CRYPTOCHROME_MAGNETIC, cryptochrome_magnetic_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Криптохромный магнитный сенсор","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":120,"name":"Криптохром Cry4 — радикальные пары/магнитный компас","category":"Сенсорные рецепторы","description":"Химический магниторецептор через криптохром Cry4. Синий свет (400-500nm) возбуждает FAD -> радикальная пара [FAD*- + Trp*+]. Магнитное поле модулирует спиновую динамику -> выход реакции зависит от угла поля (sin2(theta)). Модулирует световой ответ PINEAL-фоторецепторов (MOD_LIGHT_SENSOR). Без собственного тока.","release":false}]})BIOCORE";
}
