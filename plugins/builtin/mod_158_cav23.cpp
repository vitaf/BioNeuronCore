// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// cav23.cpp — Cav2.3 R-type Ca²⁺ канал
// Тип: MOD_CAV23 (158)

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double cav23_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float Vhalf = -25.0f;
    float k     = 6.0f;
    float gamma_ps = 14.0f;

    double snx = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    float snx_occ = snx / (snx + SNX482_IC50);
    float Vhalf_eff = Vhalf + SNX482_VHALF_SHIFT * snx_occ;

    float m_inf = 1.0f / (1.0f + expf(-(V - Vhalf_eff) / (k + 1e-6f)));
    float tau_m = 2.0f;
    m->state[0] += (m_inf - m->state[0]) * dt / tau_m;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    // Двухкомпонентная инактивация: fast (~25 мс) + slow (~100 мс), V½=−60 (k=6).
    float h_inf = 1.0f / (1.0f + expf((V + 60.0f) / 6.0f));
    m->state[1] += (h_inf - m->state[1]) * dt / CAV23_TAU_H_FAST_MS;
    m->state[2] += (h_inf - m->state[2]) * dt / CAV23_TAU_H_SLOW_MS;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;
    if (m->state[2] < 0.0f) m->state[2] = 0.0f;
    if (m->state[2] > 1.0f) m->state[2] = 1.0f;

    double h_eff = CAV23_FAST_WEIGHT * m->state[1] + (1.0f - CAV23_FAST_WEIGHT) * m->state[2];
    double open_prob = m->state[0] * m->state[0] * h_eff;

    float Ca_uM = s->intra_ions[ION_CA] * 1000.0f;
    float CaM = powf(Ca_uM, 4.0f) / (1.0f + powf(Ca_uM, 4.0f));
    open_prob *= (1.0f - CDI_CAV23 * CaM);
    if (open_prob < 0.0f) open_prob = 0.0f;

    float Ca_i = s->intra_ions[ION_CA];
    float Ca_o = s->extra_ions[ION_CA];
    double P_single = ghk_single_channel_P(gamma_ps, 2.0f, GHK_CBAR_CA_MM);
    double flux = ghk_divalent(V, Ca_i, Ca_o);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    float I_Ca = (float)(I_A * A_TO_PA);

    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);
    s->intra_ions[ION_CA] -= (I_Ca * dt) / cpm;
    s->extra_ions[ION_CA] += (I_Ca * dt) / (cpm * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);

    return -I_Ca;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_CAV23, cav23_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Cav2.3 R-type Ca2+","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":158,"name":"Cav2.3 R-type Ca2+","category":"Voltage-gated Ca","description":"Cav2.3 (CACNA1E) R-type Ca2+. m2 HVA (V1/2 -25 mV) + slow partial inactivation. Weak CDI. SNX-482 (V1/2 shift). gamma=14 pS. GHK Ca2+ (z=2).","release":false}]})BIOCORE";
}
