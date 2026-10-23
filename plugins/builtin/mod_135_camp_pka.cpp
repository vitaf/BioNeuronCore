// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_135_camp_pka.cpp — cAMP/PKA каскад (общий пул на сегмент)
// Тип: MOD_CAMP_PKA (135)
//
// Gs-рецепторы (D1/D5/β1/β2/5-HT4/6/7/A2A/A2B/H2/TAAR1) стимулируют AC (аденилатциклазу).
// AC: ATP → cAMP (Михаэлис-Ментен по ATP). PDE: cAMP → 5'-AMP (Михаэлис-Ментен по cAMP).
// PKA: Hill(cAMP)·(1 − depth·desens). HCN читает [cAMP] → сдвиг V½; L-type Ca²⁺ читает PKA → буст.
// state[0]=[cAMP]µM, [1]=PKA (0-1), [2]=desens (0-1), [3..6]=overrides (AC/PDE Vmax, PKA EC50, Hill).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double camp_pka_current_impl(Modifier* m, Segment* s, float dt) {
    return camp_pka_step(m, s, dt);  // единая реализация — modifier_helpers.h
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_CAMP_PKA, camp_pka_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"cAMP/PKA каскад","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":135,"name":"cAMP/PKA каскад (Gs→AC→cAMP→PKA)","category":"Сигнальные каскады","description":"Общий пул cAMP/PKA на сегмент. Gs-рецепторы→AC→cAMP; PDE деградирует cAMP; PKA=Hill(cAMP). HCN читает [cAMP]→сдвиг V½; L-type Ca²⁺ читает PKA→буст. state[0]=[cAMP]µM,[1]=PKA,[2]=desens,[3..6]=overrides.","release":false}]})BIOCORE";
}
