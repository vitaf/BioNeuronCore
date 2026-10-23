// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// gabaa.cpp — GABA-A-рецептор
// Тип: MOD_NT_RECEPTOR_GABAA (11)
// Категория: Ионотропные рецепторы
// Автогенерация: generate_builtin_plugins.py

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double gabaa_current_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — GABAA: единый источник параметров (units.h), как в fallback.
    return ionotropic_ghk_current(m, s, dt, 1.0f, GABAA_EC50_REL, GABAA_TAU_RISE_MS, GABAA_TAU_DECAY_MS,
                                  1, 0.0f, 0.0f, 0.0f, true, GABAA_TAU_DESENS_MS);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NT_RECEPTOR_GABAA, gabaa_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"GABA-A-рецептор","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":11,"name":"GABA-A-рецептор","category":"Ионотропные рецепторы","description":"Ионотропный GABA-рецептор — хлорная проводимость, ингибиторный","release":false}]})BIOCORE";
}
