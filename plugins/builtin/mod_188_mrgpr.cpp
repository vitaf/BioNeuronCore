// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mrgpr.cpp — Mrgpr (Mas-related GPCR), метаботропный рецептор зуда/хемоноцицепции.
// Тип: MOD_MRGPR (188). Категория: Метаботропные Gq/11.
// Лиганд (хлороквин/BAM8-22) из state[4]; нижестоящий TRPA1 через Ca²⁺ (анкириновые повторы).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"

static double mrgpr_current_impl(Modifier* m, Segment* s, float dt) {
    return mrgpr_signal(m, s, dt);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_MRGPR, mrgpr_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Mrgpr itch GPCR","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":188,"name":"Mrgpr (рецептор зуда, GPCR)","category":"Метаботропные Gq/11","description":"Mrgpr (Mas-related GPCR): метаботропный рецептор зуда/хемоноцицепции. Gαq/Gβγ → Ca²⁺ → TRPA1. Лиганд: хлороквин EC50 28 µM (Liu 2009)/BAM8-22 (Wilson 2011).","release":false}]})BIOCORE";
}
