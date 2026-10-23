// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_147_glast.cpp — GLAST/GLT-1 (EAAT1/2), глиальный глутаматный транспортер.
// Тип: MOD_GLAST (147)
// Категория: Базовые модификаторы
// Био: 3 Na⁺ + 1 H⁺ + 1 глутамат (внутрь), 1 K⁺ (наружу), чистый +2. Заменяет tau_clearance.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double glast_current_impl(Modifier* m, Segment* s, float dt) {
    // GLAST/GLT-1 — глиальный глутаматный транспортер (Michaelis-Menten).
    // Гейтинг — единый источник modifier_helpers.h (glast_saturation), паритет с fallback.
    float area = effective_area(m, s);

    float Glu = s->neurotransmitter[NT_GLUTAMATE];  // отн.ед.
    if (Glu < 0.0f) Glu = 0.0f;
    float frac = glast_saturation(Glu);

    double I_glast = GLAST_IMAX_UA_PER_CM2 * m->density * area * UA_PER_CM2_TO_PA * frac;  // пА

    float volume = segment_volume(s);
    float cpm = FARADAY_CONST * volume / 1000.0f;
    float transport = I_glast * dt / (2.0f * cpm);

    float glu_avail = Glu / GLAST_GLU_REL_PER_MM;
    float na_avail = s->extra_ions[ION_NA] * EXTRA_VOLUME_RATIO / 3.0f;
    float k_avail  = s->intra_ions[ION_K];
    if (transport > glu_avail) transport = glu_avail;
    if (transport > na_avail) transport = na_avail;
    if (transport > k_avail)  transport = k_avail;
    if (transport < 0.0f) transport = 0.0f;

    s->intra_ions[ION_NA] += 3.0f * transport;
    s->extra_ions[ION_NA] -= 3.0f * transport / EXTRA_VOLUME_RATIO;
    s->intra_ions[ION_K]  -= 1.0f * transport;
    s->extra_ions[ION_K]  += 1.0f * transport / EXTRA_VOLUME_RATIO;
    s->neurotransmitter[NT_GLUTAMATE] -= transport * GLAST_GLU_REL_PER_MM;
    if (s->neurotransmitter[NT_GLUTAMATE] < 0.0f) s->neurotransmitter[NT_GLUTAMATE] = 0.0f;
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);

    return I_glast;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_GLAST, glast_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"GLAST/GLT-1 глутаматный транспортер","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":147,"name":"GLAST/GLT-1 (EAAT1/2) — глиальный глутаматный транспортер","category":"Базовые модификаторы","description":"Глутаматный транспортер глии. 3 Na⁺ + 1 H⁺ + 1 глутамат внутрь, 1 K⁺ наружу (чистый +2). Michaelis-Menten K_m=15 µM. Заменяет tau_clearance.","release":false}]})BIOCORE";
}
