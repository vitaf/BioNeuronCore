// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_67_nb_alpha7_er_nachr.cpp — NB_Alpha7ER_nAChR
// Тип: MOD_NB_ALPHA7_ER_NACHR (67)
// Категория: NB-рецепторы (Neuroblox-совместимые)
// Модель: α7 nAChR (Dani & Bertrand 2007) — быстрый, высокий Ca²⁺.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double nb_alpha7_er_nachr_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — Марков-гейтинг/ER Ca-store/хардкод E_Ca заменены на честный ionotropic
    // (single-source с типом α7=97). Читает neurotransmitter[NT_ACETYLCHOLINE].
    return ionotropic_ghk_current(m, s, dt, 0.5f, NICOTINIC_A7_EC50_REL, NICOTINIC_A7_TAU_RISE_MS, NICOTINIC_A7_TAU_DECAY_MS,
                                  NT_ACETYLCHOLINE, NICOTINIC_A7_P_NA_RATIO, NICOTINIC_A7_P_K_RATIO, NICOTINIC_A7_P_CA_RATIO, false, NICOTINIC_A7_TAU_DESENS_MS);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NB_ALPHA7_ER_NACHR, nb_alpha7_er_nachr_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"NB_Alpha7ER_nAChR","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":67,"name":"NB_Alpha7ER_nAChR","category":"NB-рецепторы","description":"α7 nAChR (Dani & Bertrand 2007)","release":false}]})BIOCORE";
}
