// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// kainate.cpp — Каинатный рецептор
// Тип: MOD_NT_RECEPTOR_KAINATE (10)
// Категория: Ионотропные рецепторы
// Автогенерация: generate_builtin_plugins.py

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double kainate_current_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — Kainate (GluK2(R), edited): единый источник параметров (units.h), как в fallback.
    return ionotropic_ghk_current(m, s, dt, KAINATE_GAMMA_PS, KAINATE_EC50_REL, KAINATE_TAU_RISE_MS, KAINATE_TAU_DECAY_MS,
                                  NT_GLUTAMATE, KAINATE_P_NA_RATIO, KAINATE_P_K_RATIO, KAINATE_P_CA_RATIO, false, KAINATE_TAU_DESENS_MS);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NT_RECEPTOR_KAINATE, kainate_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Каинатный рецептор","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":10,"name":"Каинатный рецептор","category":"Ионотропные рецепторы","description":"Ионотропный глутаматный рецептор каината","release":false}]})BIOCORE";
}
