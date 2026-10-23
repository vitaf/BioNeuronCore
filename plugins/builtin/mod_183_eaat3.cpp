// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_183_eaat3.cpp — EAAT3/EAAC1 (SLC1A1), нейрональный глутаматный транспортер.
// Тип: MOD_EAAT3 (183). 3 Na⁺ + 1 H⁺ + 1 глутамат (внутрь), 1 K⁺ (наружу), net +2.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"

static double eaat3_current_impl(Modifier* m, Segment* s, float dt) {
    float I = glial_nt_uptake_current(m, s, dt, NT_GLUTAMATE, EAAT3_K_M_REL,
                                      EAAT3_IMAX_UA_PER_CM2, EAAT3_GLU_REL_PER_MM, 3, 0, 1, 2);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);
    return I;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_EAAT3, eaat3_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"EAAT3 глутаматный транспортер","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":183,"name":"EAAT3/EAAC1 (SLC1A1) — нейрональный глутаматный транспортер","category":"Базовые модификаторы","description":"Нейрональный глутаматный транспортер (пресинаптический/соматический reuptake). 3 Na⁺ + 1 H⁺ + 1 глутамат внутрь, 1 K⁺ наружу (чистый +2). Michaelis-Menten K_m≈30 µM.","release":false}]})BIOCORE";
}
