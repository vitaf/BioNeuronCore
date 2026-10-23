// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_185_gat1.cpp — GAT1 (SLC6A1), нейрональный транспортер GABA.
// Тип: MOD_GAT1 (185). 3 Na⁺ + 1 Cl⁻ + GABA (внутрь), net +2 (Willford 2015).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"

static double gat1_current_impl(Modifier* m, Segment* s, float dt) {
    float I = glial_nt_uptake_current(m, s, dt, NT_GABA, GAT1_K_M_REL,
                                      GAT1_IMAX_UA_PER_CM2, GAT1_GABA_REL_PER_MM, 3, 1, 0, 2);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);
    return I;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_GAT1, gat1_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"GAT1 GABA транспортер","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":185,"name":"GAT1 (SLC6A1) — нейрональный транспортер GABA","category":"Базовые модификаторы","description":"Нейрональный транспортер GABA (пресинаптический reuptake). 3 Na⁺ + 1 Cl⁻ + GABA внутрь (net +2, Willford 2015). K_m≈7 µM.","release":false}]})BIOCORE";
}
