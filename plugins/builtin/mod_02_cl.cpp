// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// cl.cpp — Хлорный канал утечки (CLC-2 тип)
// Тип: MOD_IONOTROPIC_CL (2)
// Категория: Базовые модификаторы
// Автогенерация: generate_builtin_plugins.py

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double cl_current_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — CLC-2 leak; GHK-анион (ионы первичны), γ≈2–3 пС.
    float V = s->V_m;
    float area = effective_area(m, s);

    // Инициализация: канал всегда открыт (чистая утечка)
    if (m->state[0] < 0.5f) m->state[0] = 1.0f;

    // γ (single-channel, пС): state[2] > 0 → переопределение, default CLC2_GAMMA_PS.
    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : CLC2_GAMMA_PS;

    float cl_in  = s->intra_ions[ION_CL];
    float cl_out = s->extra_ions[ION_CL];
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_CL_MM);
    double flux = ghk_anion(V, cl_in, cl_out);
    double I_A = P_single * flux * (m->density * area) * m->state[0];
    float I_Cl = (float)(I_A * A_TO_PA);

    float volume = segment_volume(s);
    float charge_per_mole = FARADAY_CONST * volume / 1000.0f;
    s->intra_ions[ION_CL] += (I_Cl * dt) / charge_per_mole;   // Cl⁻: += (анион)
    s->extra_ions[ION_CL] -= (I_Cl * dt) / (charge_per_mole * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);

    return -I_Cl;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_IONOTROPIC_CL, cl_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Cl-канал утечки","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":2,"name":"Cl-канал утечки","category":"Базовые модификаторы","description":"Пассивный хлорный канал утечки (CLC-2). Всегда открыт, проводит Cl⁻ по градиенту Нернста. Стабилизирует V_m, шунтирующее торможение.","release":false}]})BIOCORE";
}
