// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// cav13.cpp — Потенциал-зависимый Ca²⁺ канал (L-тип, Ca_v1.3 / CACNA1D)
// Тип: MOD_CAV13 (1)
// Категория: Базовые модификаторы
// Автогенерация: generate_builtin_plugins.py

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double cav13_current_impl(Modifier* m, Segment* s, float dt) {
    // Cav1.3 (CACNA1D) — L-type Ca²⁺, пейсмейкерный (Xu & Lipscombe 2001).
    float V = s->V_m;
    float area = effective_area(m, s);
    double Vhalf = (m->state[3] != 0.0f) ? -fabs(m->state[3]) : -35.0f; // активация Cav1.3
    double k = (m->state[4] > 0.0f) ? m->state[4] : 7.0f;  // slope активации
    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : 25.0f;  // γ (пС, L-type)

    // Активация: m² (L-type — два независимых гейта активации)
    float m_inf = 1.0f / (1.0f + expf(-(V - Vhalf) / (k + 1e-6f)));
    float tau_m = 2.0f; // мс — быстрая активация
    m->state[0] += (m_inf - m->state[0]) * dt / tau_m;

    // Инактивация: медленная VDI (V½=−52) + слабая CDI (Kd=3 µM, слабее Cav1.2)
    float Ca = s->intra_ions[ION_CA] * MM_TO_UM; // µM
    float h_inf = 1.0f / (1.0f + expf((V + 52.0f) / 8.0f));
    h_inf *= 1.0f / (1.0f + Ca / 3.0f);
    float tau_h = 100.0f; // мс — медленная инактивация → плато
    m->state[1] += (h_inf - m->state[1]) * dt / tau_h;

    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    // GHK для Ca²⁺ (z=2), P в см/с (честная γ→P шкала, паритет с fallback hh_biological.cpp)
    float ca_in  = s->intra_ions[ION_CA];
    float ca_out = s->extra_ions[ION_CA];
    double P_single = ghk_single_channel_P(gamma_ps, 2.0f, GHK_CBAR_CA_MM);
    double open_prob = m->state[0] * m->state[0] * m->state[1]; // m²·h
    double flux = ghk_divalent(V, ca_in, ca_out);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    float I_Ca = (float)(I_A * A_TO_PA);

    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);
    s->intra_ions[ION_CA] -= (I_Ca * dt) / (2.0f * cpm);
    s->extra_ions[ION_CA] -= (I_Ca * dt) / (2.0f * cpm * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);

    return -I_Ca;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_CAV13, cav13_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Cav1.3 L-type Ca²⁺ канал","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":1,"name":"Cav1.3 L-type Ca²⁺ канал","category":"Базовые модификаторы","description":"Пейсмейкерный L-type Ca²⁺ канал (Ca_v1.3, CACNA1D). Активация m² при V½=−35 мВ, медленная инактивация V½=−52 мВ, слабая Ca²⁺-зависимая инактивация (CDI). GHK-флюкс для Ca²⁺ (z=2), честная P-шкала γ=25 пС.","release":false}]})BIOCORE";
}
