// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_144_cav14.cpp — L-type Ca²⁺ канал (Ca_v1.4 / CACNA1F), фоторецептор.
// Тип: MOD_CAV14 (144)
// Категория: Базовые модификаторы
// Био: γ=4 пС (Doering 2005), БЕЗ CDI (ICDI), медленная VDI. CaBP4 снимает ингибицию ICDI/CTM.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double cav14_current_impl(Modifier* m, Segment* s, float dt) {
    // Cav1.4 (CACNA1F) — L-type Ca²⁺, фоторецептор. Без инактивации (h≡1).
    // CaBP4 связывает IQ → снимает ингибицию ICDI/CTM → ΔV½=−10 мВ + boost×5.
    // Гейтинг — единый источник modifier_helpers.h (cav14_m_inf), паритет с fallback.
    float V = s->V_m;
    float area = effective_area(m, s);
    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : CAV14_GAMMA_PS;

    float bound = segment_cabp4_bound(s);
    float Vhalf = CAV14_VHALF_BARE + CAV14_CABP4_DVHALF * bound;

    float m_inf = cav14_m_inf(V, Vhalf);
    m->state[0] += (m_inf - m->state[0]) * dt / CAV14_TAU_M_MS;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    // GHK для Ca²⁺ (z=2), P из γ (честная γ→P шкала, паритет с fallback).
    float ca_in  = s->intra_ions[ION_CA];
    float ca_out = s->extra_ions[ION_CA];
    double P_single = ghk_single_channel_P(gamma_ps, 2.0f, GHK_CBAR_CA_MM);
    double open_prob = m->state[0] * m->state[0]; // m² (сдвиг V½ от CaBP4 уже в Vhalf)

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
    register_modifier_current(MOD_CAV14, cav14_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Cav1.4 L-type Ca²⁺ канал","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":144,"name":"Cav1.4 L-type Ca²⁺ канал (фоторецептор)","category":"Базовые модификаторы","description":"L-type Ca²⁺ канал (CACNA1F), γ=4 пС, БЕЗ CDI, медленная VDI. CaBP4 снимает ингибицию ICDI/CTM (ΔV½=−10 мВ, boost×5). GHK-флюкс Ca²⁺ (z=2).","release":false}]})BIOCORE";
}
