// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// alpha1a.cpp — α1A-адренорецептор
// Тип: MOD_NT_RECEPTOR_ALPHA1A (55)
// Категория: Метаботропные Gq/11
// Автогенерация: generate_builtin_plugins.py

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double alpha1a_current_impl(Modifier* m, Segment* s, float dt) {
    return gq_signal(m, s, dt, NT_NORADRENALINE);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NT_RECEPTOR_ALPHA1A, alpha1a_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"α1A-адренорецептор","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":55,"name":"α1A-адренорецептор","category":"Метаботропные Gq/11","description":"Метаботропный адренорецептор α1A (Gq/11) — IP3/DAG, вазоконстрикция","release":false}]})BIOCORE";
}
