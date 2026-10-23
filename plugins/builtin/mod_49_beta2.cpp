// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// beta2.cpp — β2-адренорецептор
// Тип: MOD_NT_RECEPTOR_BETA2 (49)
// Категория: Метаботропные Gs
// Автогенерация: generate_builtin_plugins.py

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double beta2_current_impl(Modifier* m, Segment* s, float dt) {
    return gs_camp_signal(m, s, dt, NT_NORADRENALINE);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NT_RECEPTOR_BETA2, beta2_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"β2-адренорецептор","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":49,"name":"β2-адренорецептор","category":"Метаботропные Gs","description":"Метаботропный адренорецептор β2 (Gs) — бронхиальный, повышает cAMP","release":false}]})BIOCORE";
}
