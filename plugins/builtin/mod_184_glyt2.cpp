// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_184_glyt2.cpp — GlyT2 (SLC6A5), нейрональный транспортер глицина.
// Тип: MOD_GLYT2 (184). 3 Na⁺ + 1 Cl⁻ + глицин (внутрь), net +2 (Roux & Supplisson 2000).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"

static double glyt2_current_impl(Modifier* m, Segment* s, float dt) {
    float I = glial_nt_uptake_current(m, s, dt, NT_GLYCINE, GLYT2_K_M_REL,
                                      GLYT2_IMAX_UA_PER_CM2, GLYT2_GLY_REL_PER_MM, 3, 1, 0, 2);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);
    return I;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_GLYT2, glyt2_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"GlyT2 глициновый транспортер","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":184,"name":"GlyT2 (SLC6A5) — нейрональный транспортер глицина","category":"Базовые модификаторы","description":"Нейрональный транспортер глицина (пресинаптический reuptake). 3 Na⁺ + 1 Cl⁻ + глицин внутрь (net +2, Roux & Supplisson 2000). K_m≈83.5 µM (Erdem 2019).","release":false}]})BIOCORE";
}
