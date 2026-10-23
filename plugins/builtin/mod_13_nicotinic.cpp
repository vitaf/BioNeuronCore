// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// nicotinic.cpp — Никотиновый рецептор
// Тип: MOD_NT_RECEPTOR_NICOTINIC (13)
// Категория: Ионотропные рецепторы
// Автогенерация: generate_builtin_plugins.py

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double nicotinic_current_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — Nicotinic α4β2 (высокоаффинный, низкий Ca²⁺): единый источник (units.h).
    return ionotropic_ghk_current(m, s, dt, 0.5f, NICOTINIC_A4B2_EC50_REL, NICOTINIC_A4B2_TAU_RISE_MS, NICOTINIC_A4B2_TAU_DECAY_MS,
                                  4, NICOTINIC_A4B2_P_NA_RATIO, NICOTINIC_A4B2_P_K_RATIO, NICOTINIC_A4B2_P_CA_RATIO, false, NICOTINIC_A4B2_TAU_DESENS_MS);
}

// Пресинаптический модулятор выброса
static float nicotinic_modulate_impl(Modifier* m, float* cleft_nt, float dt) {
    return modulate_release_ionotropic(m, cleft_nt, dt, 0);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NT_RECEPTOR_NICOTINIC, nicotinic_current_impl);
    register_modulator_release(MOD_NT_RECEPTOR_NICOTINIC, nicotinic_modulate_impl);
    return 2;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Никотиновый рецептор","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":13,"name":"Никотиновый рецептор","category":"Ионотропные рецепторы","description":"Ионотропный ацетилхолиновый рецептор (никотиновый) — катионная проводимость","release":true}]})BIOCORE";
}
