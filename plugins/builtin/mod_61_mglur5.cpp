// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mglur5.cpp — mGluR5-рецептор
// Тип: MOD_NT_RECEPTOR_MGLUR5 (61)
// Категория: Метаботропные Gq/11
// Автогенерация: generate_builtin_plugins.py

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double mglur5_current_impl(Modifier* m, Segment* s, float dt) {
    return gq_signal(m, s, dt, NT_GLUTAMATE);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NT_RECEPTOR_MGLUR5, mglur5_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"mGluR5-рецептор","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":61,"name":"mGluR5-рецептор","category":"Метаботропные Gq/11","description":"Метаботропный глутаматный рецептор mGluR5 (Gq/11) — IP3/DAG, постсинаптический","release":false}]})BIOCORE";
}
