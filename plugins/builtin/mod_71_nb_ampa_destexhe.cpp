// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_71_nb_ampa_destexhe.cpp — NB_AMPA_Destexhe
// Тип: MOD_NB_AMPA_DESTEXHE (71)
// Категория: NB-рецепторы (Neuroblox-совместимые)
// Модель: AMPA (GluA2, Traynelis 2010).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double nb_ampa_destexhe_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — damped oscillator + spk-суррогат заменены на честный ionotropic (single-source с типом 8).
    // Читает neurotransmitter[NT_GLUTAMATE].
    return ionotropic_ghk_current(m, s, dt, 1.0f, AMPA_EC50_REL, AMPA_TAU_RISE_MS, AMPA_TAU_DECAY_MS,
                                  NT_GLUTAMATE, AMPA_P_NA_RATIO, AMPA_P_K_RATIO, AMPA_P_CA_RATIO, false, AMPA_TAU_DESENS_MS);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NB_AMPA_DESTEXHE, nb_ampa_destexhe_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"NB_AMPA_Destexhe","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":71,"name":"NB_AMPA_Destexhe","category":"NB-рецепторы","description":"AMPA (GluA2, Traynelis 2010)","release":false}]})BIOCORE";
}
