// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_65_nb_d1_modulator.cpp — NB_D1_Modulator
// Тип: MOD_NB_D1_MODULATOR (65)
// Категория: NB-рецепторы (Neuroblox-совместимые)
// Модель: MsnD1Receptor — Hill-occupancy dopamine → D1, M_NMDA1
// Humphries et al. (2009), Frontiers in Computational Neuroscience
//
// Вычисляет D1-модуляцию и сохраняет M_NMDA1 в state[1].
// Другие модификаторы (MsnNMDAR) могут читать этот фактор.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

#define NB_D1_K_D1    0.3f
#define NB_D1_N_D1    1.0f
#define NB_D1_TAU_PHI 100.0f
#define NB_D1_BETA1   0.5f

static double nb_d1_modulator_impl(Modifier* m, Segment* s, float dt) {
    float DA = s->neurotransmitter[NT_DOPAMINE];
    double phi1 = m->state[0];

    // Hill occupancy: ϕ1_inf = DA^n / (DA^n + K^n)
    float DA_n = powf(DA, NB_D1_N_D1);
    float K_n = powf(NB_D1_K_D1, NB_D1_N_D1);
    float phi1_inf = DA_n / (DA_n + K_n);

    // First-order relaxation
    phi1 += (phi1_inf - phi1) * dt / NB_D1_TAU_PHI;

    if (phi1 < 0.0f) phi1 = 0.0f;
    if (phi1 > 1.0f) phi1 = 1.0f;

    m->state[0] = phi1;
    m->state[1] = 1.0f + NB_D1_BETA1 * phi1;  // M_NMDA1

    return 0.0f;  // модулятор не генерирует ток напрямую
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NB_D1_MODULATOR, nb_d1_modulator_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"NB_D1_Modulator","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":65,"name":"NB_D1_Modulator","category":"NB-рецепторы","description":"D1-модулятор: Hill-occupancy DA->D1, M_NMDA1 (Humphries 2009)","release":false}]})BIOCORE";
}
