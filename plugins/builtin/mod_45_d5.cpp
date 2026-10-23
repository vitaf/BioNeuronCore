// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// d5.cpp — D5-рецептор
// Тип: MOD_NT_RECEPTOR_D5 (45)
// Категория: Метаботропные Gs
// Автогенерация: generate_builtin_plugins.py

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double d5_current_impl(Modifier* m, Segment* s, float dt) {
    return gs_camp_signal(m, s, dt, NT_DOPAMINE);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NT_RECEPTOR_D5, d5_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"D5-рецептор","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":45,"name":"D5-рецептор","category":"Метаботропные Gs","description":"Метаботропный дофаминовый рецептор D5 (Gs) — повышает cAMP","release":false}]})BIOCORE";
}
