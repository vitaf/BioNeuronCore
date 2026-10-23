// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_75_nb_msn_nmdar.cpp — NB_MsnNMDAR
// Тип: MOD_NB_MSN_NMDAR (75)
// Категория: NB-рецепторы (Neuroblox-совместимые)
// Модель: NMDA (Mayer & Westbrook 1987) + Mg²⁺-блок + D1-модуляция (стриатум MSN).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double nb_msn_nmdar_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — spk-суррогат/Moradi-кинетика заменены на честный ionotropic + Mg-блок + D1-усиление
    // (mod_scale = M_NMDA1 из D1-модулятора на сегменте). Читает neurotransmitter[NT_GLUTAMATE].
    float M_NMDA1 = 1.0f;
    for (uint8_t j = 0; j < s->modifier_count; j++) {
        if (s->modifiers[j].type == MOD_NB_D1_MODULATOR) {
            M_NMDA1 = s->modifiers[j].state[1];
            if (M_NMDA1 < 0.0f) M_NMDA1 = 0.0f;
            break;
        }
    }
    double I = ionotropic_ghk_current(m, s, dt, 1.0f, NMDA_EC50_REL, NMDA_TAU_RISE_MS, NMDA_TAU_DECAY_MS,
                                     NT_GLUTAMATE, NMDA_P_NA_RATIO, NMDA_P_K_RATIO, NMDA_P_CA_RATIO, false, NMDA_TAU_DESENS_MS, 0.0f, M_NMDA1);
    float mg = s->extra_ions[ION_MG];
    if (mg < 0.01f) mg = 1.0f;
    float mg_block = 1.0f / (1.0f + (mg / NMDA_MG_K0_MM) * expf(-NMDA_MG_COEF_PER_MV * s->V_m));
    return I * mg_block;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NB_MSN_NMDAR, nb_msn_nmdar_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"NB_MsnNMDAR","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":75,"name":"NB_MsnNMDAR","category":"NB-рецепторы","description":"NMDA + Mg²⁺-блок + D1-модуляция (стриатум MSN)","release":false}]})BIOCORE";
}
