// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_76_nb_msn_ampar.cpp — NB_MsnAMPAR
// Тип: MOD_NB_MSN_AMPAR (76)
// Категория: NB-рецепторы (Neuroblox-совместимые)
// Модель: AMPA (GluA2, Traynelis 2010) + D2-модуляция (стриатум MSN).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double nb_msn_ampar_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — damped oscillator/spk-суррогат заменены на честный ionotropic + D2-подавление
    // (mod_scale = M_AMPA2 из D2-модулятора на сегменте). Читает neurotransmitter[NT_GLUTAMATE].
    float M_AMPA2 = 1.0f;
    for (uint8_t j = 0; j < s->modifier_count; j++) {
        if (s->modifiers[j].type == MOD_NB_D2_MODULATOR) {
            M_AMPA2 = s->modifiers[j].state[1];
            if (M_AMPA2 < 0.0f) M_AMPA2 = 0.0f;
            break;
        }
    }
    return ionotropic_ghk_current(m, s, dt, 1.0f, AMPA_EC50_REL, AMPA_TAU_RISE_MS, AMPA_TAU_DECAY_MS,
                                  NT_GLUTAMATE, AMPA_P_NA_RATIO, AMPA_P_K_RATIO, AMPA_P_CA_RATIO, false, AMPA_TAU_DESENS_MS, 0.0f, M_AMPA2);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NB_MSN_AMPAR, nb_msn_ampar_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"NB_MsnAMPAR","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":76,"name":"NB_MsnAMPAR","category":"NB-рецепторы","description":"AMPA + D2-модуляция (стриатум MSN)","release":false}]})BIOCORE";
}
