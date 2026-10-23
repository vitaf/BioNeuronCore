// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// nmda.cpp — NMDA-рецептор
// Тип: MOD_NT_RECEPTOR_NMDA (9)
// Категория: Ионотропные рецепторы
// Автогенерация: generate_builtin_plugins.py

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double nmda_current_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — NMDA GluN1/GluN2A (Mayer & Westbrook 1987): единый источник параметров (units.h),
    // как в fallback. Mg²⁺-блок (Woodhull) + CDI (Ca²⁺/CaM) + глицин-коагонист (насыщен).
    double I = ionotropic_ghk_current(m, s, dt, 1.0f, NMDA_EC50_REL, NMDA_TAU_RISE_MS, NMDA_TAU_DECAY_MS,
                                     0, NMDA_P_NA_RATIO, NMDA_P_K_RATIO, NMDA_P_CA_RATIO, false, NMDA_TAU_DESENS_MS);
    float mg_block = 1.0f / (1.0f + (1.0f / NMDA_MG_K0_MM) * expf(-NMDA_MG_COEF_PER_MV * s->V_m));
    return I * mg_block * nmda_cdi_coagonist(m, s, dt);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NT_RECEPTOR_NMDA, nmda_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"NMDA-рецептор","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":9,"name":"NMDA-рецептор","category":"Ионотропные рецепторы","description":"Ионотропный глутаматный рецептор NMDA — кальциевая проводимость с Mg²⁺-блоком","release":false}]})BIOCORE";
}
