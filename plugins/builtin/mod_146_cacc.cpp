// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_146_cacc.cpp — CaCC/ANO2 (TMEM16B), Ca²⁺-активируемый Cl⁻-канал.
// Тип: MOD_CACC (146)
// Категория: Базовые модификаторы
// Био: Ca²⁺ EC50=4 µM, Hill n=2 (Cenedese 2011); Cl⁻-селективен; γ=2 пС (структурная оценка).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double cacc_current_impl(Modifier* m, Segment* s, float dt) {
    // CaCC/ANO2 (TMEM16B) — Ca²⁺-активируемый Cl⁻-канал. GHK Cl⁻ (ghk_anion).
    // Гейтинг — единый источник modifier_helpers.h (cacc_m_inf), паритет с fallback.
    float V = s->V_m;
    float area = effective_area(m, s);
    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : CACC_GAMMA_PS;

    float ca_um = s->intra_ions[ION_CA] * MM_TO_UM;
    if (ca_um < 0.0f) ca_um = 0.0f;
    float m_inf = cacc_m_inf(ca_um);
    m->state[0] += (m_inf - m->state[0]) * dt / CACC_TAU_MS;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    float cl_in  = s->intra_ions[ION_CL];
    float cl_out = s->extra_ions[ION_CL];
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_CL_MM);
    double flux = ghk_anion(V, cl_in, cl_out);
    double I_A = P_single * flux * (m->density * area) * m->state[0];
    float I_Cl = (float)(I_A * A_TO_PA);

    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);
    s->intra_ions[ION_CL] += (I_Cl * dt) / cpm;  // Cl⁻: += (анион)
    plugin_clamp_ions(s->intra_ions, ION_COUNT);

    return -I_Cl;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_CACC, cacc_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"CaCC/ANO2 (TMEM16B) Cl⁻-канал","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":146,"name":"CaCC/ANO2 — Ca²⁺-активируемый Cl⁻-канал","category":"Базовые модификаторы","description":"Ca²⁺-активируемый Cl⁻-канал (TMEM16B). Hill n=2, EC50=4 µM (Cenedese 2011), γ=2 пС (структурная оценка). Cl⁻-эффлюкс усиливает деполяризацию (обоняние).","release":false}]})BIOCORE";
}
