// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_74_nb_gabab_destexhe.cpp — NB_GABAB_Destexhe
// Тип: MOD_NB_GABAB_DESTEXHE (74)
// Категория: NB-рецепторы (Neuroblox-совместимые)
// Модель: GABAB — метаботропный Gi/o → Gβγ → GIRK (Kir3.x).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double nb_gabab_destexhe_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — damped oscillator + spk-суррогат заменены на честный gi_o_ghk_effect
    // (single-source с типом 17). Читает neurotransmitter[NT_GABA].
    return gi_o_ghk_effect(m, s, dt, NT_GABA);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NB_GABAB_DESTEXHE, nb_gabab_destexhe_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"NB_GABAB_Destexhe","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":74,"name":"NB_GABAB_Destexhe","category":"NB-рецепторы","description":"GABAB: метаботропный Gi/o → GIRK","release":false}]})BIOCORE";
}
