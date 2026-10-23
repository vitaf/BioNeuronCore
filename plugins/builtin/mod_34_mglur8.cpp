// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mglur8.cpp — mGluR8-рецептор
// Тип: MOD_NT_RECEPTOR_MGLUR8 (34)
// Категория: Метаботропные Gi/o
// Автогенерация: generate_builtin_plugins.py

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double mglur8_current_impl(Modifier* m, Segment* s, float dt) {
    return gi_o_ghk_effect(m, s, dt, NT_GLUTAMATE);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NT_RECEPTOR_MGLUR8, mglur8_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"mGluR8-рецептор","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":34,"name":"mGluR8-рецептор","category":"Метаботропные Gi/o","description":"Метаботропный глутаматный рецептор mGluR8 (Gi/o)","release":false}]})BIOCORE";
}
