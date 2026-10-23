// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_145_cabp4.cpp — CaBP4 (calcium-binding protein 4), модулятор Cav1.4.
// Тип: MOD_CABP4 (145)
// Категория: Базовые модификаторы
// Био: связывает IQ-мотив Cav1.4, снимает ингибицию ICDI/CTM (Shaltiel 2012).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double cabp4_current_impl(Modifier* m, Segment* s, float dt) {
    // CaBP4 — модулятор Cav1.4, НЕ канал (ток = 0).
    // state[0] = bound (доля Cav1.4, связанная CaBP4), насыщается к 1.
    m->state[0] += (1.0f - m->state[0]) * dt / CABP4_TAU_BIND_MS;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    return 0.0f;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_CABP4, cabp4_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"CaBP4 (calcium-binding protein 4)","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":145,"name":"CaBP4 — модулятор Cav1.4","category":"Базовые модификаторы","description":"Ca²⁺-binding protein 4. Связывает IQ-мотив Cav1.4, снимает ингибицию ICDI/CTM → ΔV½=−10 мВ + доступность ×5. НЕ канал (ток=0).","release":false}]})BIOCORE";
}
