// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_149_gat3.cpp — GAT3 (SLC6A11), глиальный транспортер GABA.
// Тип: MOD_GAT3 (149). 2 Na⁺ + 1 Cl⁻ + GABA (внутрь), net +1.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"

static double gat3_current_impl(Modifier* m, Segment* s, float dt) {
    float I = glial_nt_uptake_current(m, s, dt, NT_GABA, GAT3_K_M_REL,
                                      GAT3_IMAX_UA_PER_CM2, GAT3_GABA_REL_PER_MM, 2, 1, 0, 1);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);
    return I;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_GAT3, gat3_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"GAT3 GABA транспортер","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":149,"name":"GAT3 (SLC6A11) — глиальный транспортер GABA","category":"Базовые модификаторы","description":"Глиальный транспортер GABA. 2 Na⁺ + 1 Cl⁻ + GABA внутрь (net +1). Заменяет tau_clearance для GABA.","release":false}]})BIOCORE";
}
