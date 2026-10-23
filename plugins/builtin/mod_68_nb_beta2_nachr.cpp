// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_68_nb_beta2_nachr.cpp — NB_Beta2_nAChR
// Тип: MOD_NB_BETA2_NACHR (68)
// Категория: NB-рецепторы (Neuroblox-совместимые)
// Модель: α4β2 nAChR (Dani & Bertrand 2007) — высокоаффинный, медленная десенс.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double nb_beta2_nachr_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — Марков/Ник-десенс заменены на честный ionotropic (single-source с типом 13).
    // Читает neurotransmitter[NT_ACETYLCHOLINE].
    return ionotropic_ghk_current(m, s, dt, 0.5f, NICOTINIC_A4B2_EC50_REL, NICOTINIC_A4B2_TAU_RISE_MS, NICOTINIC_A4B2_TAU_DECAY_MS,
                                  NT_ACETYLCHOLINE, NICOTINIC_A4B2_P_NA_RATIO, NICOTINIC_A4B2_P_K_RATIO, NICOTINIC_A4B2_P_CA_RATIO, false, NICOTINIC_A4B2_TAU_DESENS_MS);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NB_BETA2_NACHR, nb_beta2_nachr_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"NB_Beta2_nAChR","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":68,"name":"NB_Beta2_nAChR","category":"NB-рецепторы","description":"α4β2 nAChR (Dani & Bertrand 2007)","release":false}]})BIOCORE";
}
