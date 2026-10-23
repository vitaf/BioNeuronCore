// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// gabab.cpp — GABA-B-рецептор
// Тип: MOD_NT_RECEPTOR_GABAB (17)
// Категория: Метаботропные Gi/o
// Автогенерация: generate_builtin_plugins.py

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double gabab_current_impl(Modifier* m, Segment* s, float dt) {
    return gi_o_ghk_effect(m, s, dt, NT_GABA);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NT_RECEPTOR_GABAB, gabab_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"GABA-B-рецептор","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":17,"name":"GABA-B-рецептор","category":"Метаботропные Gi/o","description":"Метаботропный GABA-B рецептор (Gi/o) — активация GIRK, ингибиторный","release":false}]})BIOCORE";
}
