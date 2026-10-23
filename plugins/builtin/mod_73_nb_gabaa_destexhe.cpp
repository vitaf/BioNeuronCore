// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_73_nb_gabaa_destexhe.cpp — NB_GABAA_Destexhe
// Тип: MOD_NB_GABAA_DESTEXHE (73)
// Категория: NB-рецепторы (Neuroblox-совместимые)
// Модель: GABAA — Cl⁻-анионный канал (тормозной, E_rev=E_Cl).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double nb_gabaa_destexhe_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — damped oscillator + spk-суррогат заменены на честный ionotropic anion
    // (single-source с типом 11). Читает neurotransmitter[NT_GABA].
    return ionotropic_ghk_current(m, s, dt, 1.0f, GABAA_EC50_REL, GABAA_TAU_RISE_MS, GABAA_TAU_DECAY_MS,
                                  NT_GABA, 0.0f, 0.0f, 0.0f, true, GABAA_TAU_DESENS_MS);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NB_GABAA_DESTEXHE, nb_gabaa_destexhe_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"NB_GABAA_Destexhe","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":73,"name":"NB_GABAA_Destexhe","category":"NB-рецепторы","description":"GABAA: Cl⁻-анионный канал (тормозной)","release":false}]})BIOCORE";
}
