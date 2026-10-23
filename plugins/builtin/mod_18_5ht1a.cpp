// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// 5ht1a.cpp — 5-HT1A-рецептор
// Тип: MOD_NT_RECEPTOR_5HT1A (18)
// Категория: Метаботропные Gi/o
// Автогенерация: generate_builtin_plugins.py

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double ht1a_current_impl(Modifier* m, Segment* s, float dt) {
    return gi_o_ghk_effect(m, s, dt, NT_SEROTONIN);
}

// Пресинаптический модулятор выброса
static float ht1a_modulate_impl(Modifier* m, float* cleft_nt, float dt) {
    return modulate_release_gi(m, cleft_nt, dt, NT_SEROTONIN);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NT_RECEPTOR_5HT1A, ht1a_current_impl);
    register_modulator_release(MOD_NT_RECEPTOR_5HT1A, ht1a_modulate_impl);
    return 2;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"5-HT1A-рецептор","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":18,"name":"5-HT1A-рецептор","category":"Метаботропные Gi/o","description":"Метаботропный серотониновый рецептор 5-HT1A (Gi/o) — ауторецептор, снижает cAMP","release":true}]})BIOCORE";
}
