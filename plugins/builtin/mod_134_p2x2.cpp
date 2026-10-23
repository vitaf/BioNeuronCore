// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_134_p2x2.cpp — P2X2 (ATP-зависимый катионный, медленная десенситизация)
// Тип: MOD_P2X2 (134)
//
// P2X2: ATP-зависимый катионный канал (P_K≈P_Na, P_Ca/P_Na≈2.5), МЕДЛЕННАЯ
// десенситизация (~3-35с) — тонический ответ на длительный ATP.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double p2x2_current_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — P2X2: единый источник параметров (units.h), как в fallback.
    return ionotropic_ghk_current(m, s, dt, 0.5f, P2X2_EC50_REL, P2X2_TAU_RISE_MS, P2X2_TAU_DECAY_MS,
                                  10, P2X2_P_NA_RATIO, P2X2_P_K_RATIO, P2X2_P_CA_RATIO, false, P2X2_TAU_DESENS_MS);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_P2X2, p2x2_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"P2X2","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":134,"name":"P2X2 — ATP-зависимый катионный (медленная десенситизация)","category":"Ионотропные рецепторы","description":"P2X2: P_Ca/P_Na≈2.5, медленная десенситизация ~5с. Тонический ответ на ATP.","release":false}]})BIOCORE";
}
