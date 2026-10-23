// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_192_rod_phototransduction.cpp — Родопсин фотокаскад (палочка).
// Тип: MOD_ROD_PHOTOTRANSDUCTION (192). Категория: Сенсор (свет).
// Полный каскад: свет → R* → трансдуцин (Gt) → PDE6 → cGMP↓ → CNG закрытие (гиперполяр.).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double rod_phototransduction_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    double light = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;
    m->state[0] += (light - m->state[0]) * dt / RHODOPSIN_TAU_R_MS;       // R* (τ~1.5 с)
    m->state[1] += (m->state[0] - m->state[1]) * dt / RHODOPSIN_TAU_GT_MS;  // Gt* (~130/с)
    m->state[2] += (m->state[1] - m->state[2]) * dt / RHODOPSIN_TAU_PDE_MS; // PDE* (τ~200 мс)
    m->state[3] += (1.0f - m->state[3]) * dt / RHODOPSIN_TAU_CGMP_SYN        // cGMP синтез (GC)
                 - m->state[2] * m->state[3] * dt / RHODOPSIN_TAU_CGMP_HYD; // минус гидролиз (PDE)
    if (m->state[0] < 0) m->state[0] = 0; if (m->state[0] > 1) m->state[0] = 1;
    if (m->state[1] < 0) m->state[1] = 0; if (m->state[1] > 1) m->state[1] = 1;
    if (m->state[2] < 0) m->state[2] = 0; if (m->state[2] > 1) m->state[2] = 1;
    if (m->state[3] < 0) m->state[3] = 0; if (m->state[3] > 1) m->state[3] = 1;
    double cg = m->state[3];
    float cg3 = cg * cg * cg;
    float kh = RHODOPSIN_CNG_KHALF, kh3 = kh * kh * kh;
    float Po = cg3 / (kh3 + cg3);  // CNG Hill n=3: открыт в темноте, закрыт на свету
    float area = effective_area(m, s);
    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);
    double P_Na, P_K, P_Ca;
    ghk_partial_P(CNG_GAMMA_PS, 0.5f, 0.3f, 0.2f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area * Po;
    float I_Na = (float)(P_Na * ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]) * N * A_TO_PA);
    float I_K  = (float)(P_K  * ghk_monovalent(V, s->intra_ions[ION_K],  s->extra_ions[ION_K])  * N * A_TO_PA);
    float I_Ca = (float)(P_Ca * ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]) * N * A_TO_PA);
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpm;
    s->intra_ions[ION_K]  -= (I_K  * dt) / cpm;
    s->intra_ions[ION_CA] -= (I_Ca * dt) / (2.0f * cpm);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    return -(I_Na + I_K + I_Ca);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_ROD_PHOTOTRANSDUCTION, rod_phototransduction_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Родопсин фотокаскад","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":192,"name":"Родопсин фотокаскад (палочка)","category":"Сенсор","description":"Полный фотокаскад палочки: свет(state[4])→R*→Gt→PDE6→cGMP↓→CNG закрытие. CNG открыт в темноте (тёмный ток Na/K/Ca), закрыт на свету (гиперполяр.). Leskov 2000; Yau & Nakatani 1985.","release":false}]})BIOCORE";
}
