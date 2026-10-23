// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_148_glyt1.cpp — GlyT1 (SLC6A9), глиальный транспортер глицина.
// Тип: MOD_GLYT1 (148). 2 Na⁺ + 1 Cl⁻ + глицин (внутрь), net +1.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"

static double glyt1_current_impl(Modifier* m, Segment* s, float dt) {
    float I = glial_nt_uptake_current(m, s, dt, NT_GLYCINE, GLYT1_K_M_REL,
                                      GLYT1_IMAX_UA_PER_CM2, GLYT1_GLY_REL_PER_MM, 2, 1, 0, 1);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);
    return I;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_GLYT1, glyt1_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"GlyT1 глициновый транспортер","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":148,"name":"GlyT1 (SLC6A9) — глиальный транспортер глицина","category":"Базовые модификаторы","description":"Глиальный транспортер глицина. 2 Na⁺ + 1 Cl⁻ + глицин внутрь (net +1). Заменяет tau_clearance для глицина.","release":false}]})BIOCORE";
}
