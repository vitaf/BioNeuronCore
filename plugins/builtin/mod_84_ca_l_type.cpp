// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// ca_l_type.cpp — L-type Ca²⁺ канал (Cav1.x, HVA)
// Тип: MOD_CA_L_TYPE (84)
// Категория: STDP Cascade

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double ca_l_type_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double Vhalf = (m->state[3] != 0.0f) ? -fabs(m->state[3]) : -11.0f; // HVA (Cav1.2)
    double k = (m->state[4] > 0.0f) ? m->state[4] : 6.9f;  // slope (Cav1.2)
    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : 25.0f;  // γ (пС, L-type)

    // Активация: Boltzmann, высокий порог
    float m_inf = 1.0f / (1.0f + expf(-(V - Vhalf) / (k + 1e-6f)));
    float tau_m = 3.0f;
    m->state[0] += (m_inf - m->state[0]) * dt / tau_m;

    // VDI (voltage-dependent inactivation) — медленная, V½=−37.3 (Cav1.2)
    float vdi_inf = 1.0f / (1.0f + expf((V + 37.3f) / 8.0f));
    m->state[1] += (vdi_inf - m->state[1]) * dt / CAV12_TAU_VDI_MS;

    // CDI (Ca²⁺-dependent inactivation) — быстрая, Kd=1 µM (кальмодулин)
    float Ca = s->intra_ions[ION_CA] * MM_TO_UM; // µM
    float cdi_inf = 1.0f / (1.0f + Ca / CAV12_CDI_KD_UM);
    m->state[5] += (cdi_inf - m->state[5]) * dt / CAV12_TAU_CDI_MS;

    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[5] < 0.0f) m->state[5] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;
    if (m->state[5] > 1.0f) m->state[5] = 1.0f;

    // GHK для Ca²⁺ (z=2), P в см/с
    float ca_in  = s->intra_ions[ION_CA];  // моль/м³
    float ca_out = s->extra_ions[ION_CA];
    double P_single = ghk_single_channel_P(gamma_ps, 2.0f, GHK_CBAR_CA_MM);
    double open_prob = m->state[0] * m->state[0] * m->state[1] * m->state[5];  // m²·h_VDI·h_CDI (L-type)
    double flux = ghk_divalent(V, ca_in, ca_out);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    float I_Ca = (float)(I_A * A_TO_PA);

    // Прямое перемещение Ca²⁺ (z=2)
    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);
    s->intra_ions[ION_CA] -= (I_Ca * dt) / (2.0f * cpm);
    s->extra_ions[ION_CA] -= (I_Ca * dt) / (2.0f * cpm * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);

    return -I_Ca;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_CA_L_TYPE, ca_l_type_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Cav1.x L-type Ca2+","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":84,"name":"Cav1.x L-type Ca2+","category":"STDP Cascade","description":"High-voltage-activated (HVA) L-type Ca2+ channel (Cav1.2/Cav1.3). Activation >-25mV. Ca2+-dependent inactivation (CDI). GHK flux for Ca2+ (z=2). Critical for STDP post-spike Ca2+ entry.","release":false}]})BIOCORE";
}
