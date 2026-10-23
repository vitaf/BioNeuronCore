// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// alpha2.cpp — α2-адренорецептор
// Тип: MOD_NT_RECEPTOR_ALPHA2 (35)
// Категория: Метаботропные Gi/o
// Автогенерация: generate_builtin_plugins.py

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double alpha2_current_impl(Modifier* m, Segment* s, float dt) {
    return gi_o_ghk_effect(m, s, dt, NT_NORADRENALINE);
}

// Пресинаптический модулятор выброса
static float alpha2_modulate_impl(Modifier* m, float* cleft_nt, float dt) {
    return modulate_release_gi(m, cleft_nt, dt, NT_NORADRENALINE);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NT_RECEPTOR_ALPHA2, alpha2_current_impl);
    register_modulator_release(MOD_NT_RECEPTOR_ALPHA2, alpha2_modulate_impl);
    return 2;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"α2-адренорецептор","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":35,"name":"α2-адренорецептор","category":"Метаботропные Gi/o","description":"Метаботропный адренорецептор α2 (Gi/o) — ингибиторный, пресинаптический","release":true}]})BIOCORE";
}
