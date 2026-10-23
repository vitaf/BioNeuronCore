// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_69_nb_ca_trpm4r.cpp — NB_CaTRPM4R
// Тип: MOD_NB_CA_TRPM4R (69)
// Категория: NB-рецепторы (Neuroblox-совместимые)
// Модель: TRPM4 — Ca²⁺-активируемый НЕселективный катионный канал (Na⁺/K⁺).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double nb_ca_trpm4r_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — CCh-суррогат + фиктивный I_Ca заменены на честный Ca²⁺-гейт Hill n=2 + GHK Na/K
    // (single-source хелпер trpm4_cation_current). Агонист — внутриклеточный Ca²⁺, не нейромедиатор.
    return trpm4_cation_current(m, s, dt, TRPM4_EC50_CA_UM, TRPM4_HILL, TRPM4_TAU_MS);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NB_CA_TRPM4R, nb_ca_trpm4r_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"NB_CaTRPM4R","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":69,"name":"NB_CaTRPM4R","category":"NB-рецепторы","description":"TRPM4: Ca²⁺-активируемый неселективный катионный канал (Ullrich 2005)","release":false}]})BIOCORE";
}
