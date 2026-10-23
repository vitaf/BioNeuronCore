// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// taar1.cpp — TAAR1-рецептор
// Тип: MOD_NT_RECEPTOR_TAAR1 (51)
// Категория: Метаботропные Gs
// Автогенерация: generate_builtin_plugins.py

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double taar1_current_impl(Modifier* m, Segment* s, float dt) {
    return gs_camp_signal(m, s, dt, NT_DOPAMINE);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NT_RECEPTOR_TAAR1, taar1_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"TAAR1-рецептор","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":51,"name":"TAAR1-рецептор","category":"Метаботропные Gs","description":"Метаботропный следовой аминовый рецептор TAAR1 (Gs)","release":false}]})BIOCORE";
}
