// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// a2a.cpp — A2A-рецептор
// Тип: MOD_NT_RECEPTOR_A2A (46)
// Категория: Метаботропные Gs
// Автогенерация: generate_builtin_plugins.py

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double a2a_current_impl(Modifier* m, Segment* s, float dt) {
    return gs_camp_signal(m, s, dt, NT_ADENOSINE);
}

// Пресинаптический модулятор выброса
static float a2a_modulate_impl(Modifier* m, float* cleft_nt, float dt) {
    return modulate_release_gs(m, cleft_nt, dt, NT_ADENOSINE);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NT_RECEPTOR_A2A, a2a_current_impl);
    register_modulator_release(MOD_NT_RECEPTOR_A2A, a2a_modulate_impl);
    return 2;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"A2A-рецептор","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":46,"name":"A2A-рецептор","category":"Метаботропные Gs","description":"Метаботропный аденозиновый рецептор A2A (Gs) — повышает cAMP, пресинаптический","release":true}]})BIOCORE";
}
