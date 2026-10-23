// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// p2x.cpp — P2X-рецептор
// Тип: MOD_NT_RECEPTOR_P2X (15)
// Категория: Ионотропные рецепторы
// Автогенерация: generate_builtin_plugins.py

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double p2x_current_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — P2X: единый источник параметров (units.h), как в fallback.
    return ionotropic_ghk_current(m, s, dt, 0.5f, P2X_EC50_REL, P2X_TAU_RISE_MS, P2X_TAU_DECAY_MS,
                                  10, P2X_P_NA_RATIO, P2X_P_K_RATIO, P2X_P_CA_RATIO, false, P2X_TAU_DESENS_MS);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NT_RECEPTOR_P2X, p2x_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"P2X-рецептор","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":15,"name":"P2X-рецептор","category":"Ионотропные рецепторы","description":"Ионотропный пуриновый рецептор P2X — АТФ-активируемый катионный канал","release":false}]})BIOCORE";
}
