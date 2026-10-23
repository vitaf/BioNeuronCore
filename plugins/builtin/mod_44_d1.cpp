// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// d1.cpp — D1-рецептор
// Тип: MOD_NT_RECEPTOR_D1 (44)
// Категория: Метаботропные Gs
// Автогенерация: generate_builtin_plugins.py

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double d1_current_impl(Modifier* m, Segment* s, float dt) {
    return gs_camp_signal(m, s, dt, NT_DOPAMINE);
}

// Пресинаптический модулятор выброса
static float d1_modulate_impl(Modifier* m, float* cleft_nt, float dt) {
    return modulate_release_gs(m, cleft_nt, dt, NT_DOPAMINE);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NT_RECEPTOR_D1, d1_current_impl);
    register_modulator_release(MOD_NT_RECEPTOR_D1, d1_modulate_impl);
    return 2;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"D1-рецептор","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":44,"name":"D1-рецептор","category":"Метаботропные Gs","description":"Метаботропный дофаминовый рецептор D1 (Gs) — повышает cAMP, возбуждающий","release":true}]})BIOCORE";
}
