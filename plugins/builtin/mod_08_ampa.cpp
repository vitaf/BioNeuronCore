// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// ampa.cpp — AMPA-рецептор
// Тип: MOD_NT_RECEPTOR_AMPA (8)
// Категория: Ионотропные рецепторы
// Автогенерация: generate_builtin_plugins.py

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double ampa_current_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — AMPA (GluA2-содержащий): единый источник параметров (units.h), как в fallback.
    return ionotropic_ghk_current(m, s, dt, 1.0f, AMPA_EC50_REL, AMPA_TAU_RISE_MS, AMPA_TAU_DECAY_MS,
                                  0, AMPA_P_NA_RATIO, AMPA_P_K_RATIO, AMPA_P_CA_RATIO, false, AMPA_TAU_DESENS_MS);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NT_RECEPTOR_AMPA, ampa_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"AMPA-рецептор","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":8,"name":"AMPA-рецептор","category":"Ионотропные рецепторы","description":"Ионотропный глутаматный рецептор AMPA — быстрая деполяризация","release":false}]})BIOCORE";
}
