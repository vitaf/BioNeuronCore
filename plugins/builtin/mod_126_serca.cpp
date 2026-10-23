// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// SERCA: Ca²⁺-АТФаза ЭПР. Закачивает Ca²⁺ из цитозоля в ER (s->ER_Ca2).
// Тип: MOD_SERCA (126). BION v10 — ER_Ca2 compartment activated.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double serca_current_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — SERCA Ca²⁺-АТФаза ЭПР (Verkhratsky 2005). 2 Ca²⁺ : 1 ATP, Hill-2, K_m=0.27µM.
    float area = effective_area(m, s);
    double K_m_Ca = (m->state[1] > 0.0f) ? m->state[1] : SERCA_K_M_CA_M_M;

    float ca = s->intra_ions[ION_CA];
    if (ca < 1e-9f) ca = 1e-9f;
    float f_Ca = (ca * ca) / (ca * ca + K_m_Ca * K_m_Ca);  // Hill-2

    float atp = s->ATP_level;
    if (atp < 0.0f) atp = 0.0f;
    float f_ATP = atp / (atp + SERCA_K_ATP_MM);  // Michaelis

    // ЕДИНИЦА — молекула: flux = N·k_cat·f_Ca·f_ATP·(2 Ca/цикл)·dt/N_A.
    double N = m->density * area;
    float flux_mol = N * SERCA_TURNOVER_MAX_S * f_Ca * f_ATP * 2.0f * dt * MS_TO_S / AVOGADRO;

    // Δc [мМ] = J / vol[m³]. Ca²⁺ из цитозоля → в ЭПР (2 Ca уже в V_max).
    float dconc = flux_mol / (segment_volume(s) * UM3_TO_M3);
    // Масса сохраняется: перемещаем не больше доступного Ca²⁺.
    float ca_avail = s->intra_ions[ION_CA] - 1e-9f;
    if (ca_avail < 0.0f) ca_avail = 0.0f;
    float actual = (dconc < ca_avail) ? dconc : ca_avail;
    s->intra_ions[ION_CA] -= actual;
    s->ER_Ca2 += actual;
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    if (s->ER_Ca2 < 0.0f) s->ER_Ca2 = 0.0f;

    // ATP: 1 ATP на 2 Ca²⁺.
    s->ATP_level -= flux_mol * SERCA_ATP_NORM_PER_MOL_CA;
    if (s->ATP_level < 0.0f) s->ATP_level = 0.0f;

    return 0.0f;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_SERCA, serca_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"SERCA","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":126,"name":"SERCA","category":"Ca2+ homeostasis","description":"ER Ca2+-ATPase: pumps Ca2+ from cytosol into ER. ATP-dependent, Hill n=2, Km~0.3uM.","release":false}]})BIOCORE";
}
