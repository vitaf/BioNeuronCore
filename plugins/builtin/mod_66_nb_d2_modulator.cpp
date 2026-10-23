// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_66_nb_d2_modulator.cpp — NB_D2_Modulator
// Тип: MOD_NB_D2_MODULATOR (66)
// Категория: NB-рецепторы (Neuroblox-совместимые)
// Модель: MsnD2Receptor — Hill-occupancy dopamine → D2, M_AMPA2
// Humphries et al. (2009), Frontiers in Computational Neuroscience
//
// Вычисляет D2-модуляцию и сохраняет M_AMPA2 в state[1].
// Другие модификаторы (MsnAMPAR) могут читать этот фактор.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

#define NB_D2_K_D2    0.3f
#define NB_D2_N_D2    1.0f
#define NB_D2_TAU_PHI 100.0f
#define NB_D2_BETA2   0.3f

static double nb_d2_modulator_impl(Modifier* m, Segment* s, float dt) {
    float DA = s->neurotransmitter[NT_DOPAMINE];
    double phi2 = m->state[0];

    float DA_n = powf(DA, NB_D2_N_D2);
    float K_n = powf(NB_D2_K_D2, NB_D2_N_D2);
    float phi2_inf = DA_n / (DA_n + K_n);

    phi2 += (phi2_inf - phi2) * dt / NB_D2_TAU_PHI;

    if (phi2 < 0.0f) phi2 = 0.0f;
    if (phi2 > 1.0f) phi2 = 1.0f;

    m->state[0] = phi2;
    m->state[1] = 1.0f - NB_D2_BETA2 * phi2;  // M_AMPA2

    return 0.0f;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NB_D2_MODULATOR, nb_d2_modulator_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"NB_D2_Modulator","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":66,"name":"NB_D2_Modulator","category":"NB-рецепторы","description":"D2-модулятор: Hill-occupancy DA->D2, M_AMPA2 (Humphries 2009)","release":false}]})BIOCORE";
}
