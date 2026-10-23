// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_63_nb_nmda_moradi_full.cpp — NB_MoradiFullNMDAR
// Тип: MOD_NB_NMDA_MORADI_FULL (63)
// Категория: NB-рецепторы (Neuroblox-совместимые)
// Модель: NMDA (Mayer & Westbrook 1987) + Mg²⁺-блок (Woodhull).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double nb_nmda_moradi_full_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — Moradi A/B/C/g/z + Q10 + spk_proxy заменены на честный ionotropic + Mg-блок
    // (single-source с типом 9). Читает neurotransmitter[NT_GLUTAMATE].
    double I = ionotropic_ghk_current(m, s, dt, 1.0f, NMDA_EC50_REL, NMDA_TAU_RISE_MS, NMDA_TAU_DECAY_MS,
                                     NT_GLUTAMATE, NMDA_P_NA_RATIO, NMDA_P_K_RATIO, NMDA_P_CA_RATIO, false, NMDA_TAU_DESENS_MS);
    float mg = s->extra_ions[ION_MG];
    if (mg < 0.01f) mg = 1.0f;
    float mg_block = 1.0f / (1.0f + (mg / NMDA_MG_K0_MM) * expf(-NMDA_MG_COEF_PER_MV * s->V_m));
    return I * mg_block;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NB_NMDA_MORADI_FULL, nb_nmda_moradi_full_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"NB_MoradiFullNMDAR","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":63,"name":"NB_MoradiFullNMDAR","category":"NB-рецепторы","description":"NMDA с Mg²⁺-блоком (Mayer & Westbrook 1987)","release":false}]})BIOCORE";
}
