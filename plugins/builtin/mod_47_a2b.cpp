// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// a2b.cpp — A2B-рецептор
// Тип: MOD_NT_RECEPTOR_A2B (47)
// Категория: Метаботропные Gs
// Автогенерация: generate_builtin_plugins.py

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double a2b_current_impl(Modifier* m, Segment* s, float dt) {
    return gs_camp_signal(m, s, dt, NT_ADENOSINE);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NT_RECEPTOR_A2B, a2b_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"A2B-рецептор","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":47,"name":"A2B-рецептор","category":"Метаботропные Gs","description":"Метаботропный аденозиновый рецептор A2B (Gs) — повышает cAMP","release":false}]})BIOCORE";
}
