// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_97_nicotinic_alpha7.cpp — α7 nAChR (ионотропный никотиновый, α7-подтип)
// Тип: MOD_NICOTINIC_ALPHA7 (97, бывш. PIR_HCN — deprecated)
//
// α7 nAChR: низкоаффинный (EC50 ACh ~100µM), высокий Ca²⁺ (P_Ca/P_Na ≈ 6-10),
// быстрая десенситизация (τ ~ 10-100 мс). Центральная холинергическая передача головастика.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double nicotinic_alpha7_current_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — Nicotinic α7 (быстрый, высокий Ca²⁺): единый источник (units.h), как в fallback.
    return ionotropic_ghk_current(m, s, dt, 0.5f, NICOTINIC_A7_EC50_REL, NICOTINIC_A7_TAU_RISE_MS, NICOTINIC_A7_TAU_DECAY_MS,
                                  4, NICOTINIC_A7_P_NA_RATIO, NICOTINIC_A7_P_K_RATIO, NICOTINIC_A7_P_CA_RATIO, false, NICOTINIC_A7_TAU_DESENS_MS);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NICOTINIC_ALPHA7, nicotinic_alpha7_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"α7 nAChR","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":97,"name":"α7 nAChR — ионотропный никотиновый (быстрый, высокий Ca²⁺)","category":"Ионотропные рецепторы","description":"α7 nAChR: P_Ca/P_Na≈6-10, быстрая десенситизация. Центральная холинергическая передача.","release":false}]})BIOCORE";
}
