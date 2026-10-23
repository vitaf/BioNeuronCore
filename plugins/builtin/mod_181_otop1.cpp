// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_181_otop1.cpp — OTOP1 протонный канал
// Тип: MOD_OTOP1 (181). Категория: Other
// Био: OTOP1 — протонный канал (кислый вкус тип III), H⁺-селективный (Tu 2018).
// pH-гейт (pH<6), τ=10 мс. GHK H⁺ (γ=20 пС — placeholder, [H⁺]_o=10^(3−pH)). С ионной консервацией.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double otop1_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    // pH внеклеточный (state[4], default 7.4)
    double ph = (m->state[4] > 0.0f) ? m->state[4] : 7.4f;
    float r = (ph < 6.0f) ? 1.0f : 0.0f;  // порог pH<6 (крутая активация)
    m->state[0] += (r - m->state[0]) * (dt / 10.0f);
    if (m->state[0] < 0.0f) m->state[0] = 0.0f; if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    double open = m->state[0];

    // Одномолекулярная GHK H⁺: [H⁺]_o из pH (state[4]) → 10^(3−pH) мМ.
    double c_H_in  = s->intra_ions[ION_H];      // pH 7.2 → 6.3e-5 мМ
    double c_H_out = pow(10.0, 3.0 - ph);       // pH<6 → >1e-3 мМ (движущая сила H⁺-входа)
    double P_single = ghk_single_channel_P(OTOP1_GAMMA_PS, 1.0f, GHK_CBAR_H_MM);
    double flux = ghk_monovalent(V, c_H_in, c_H_out);
    double I_A = P_single * flux * (m->density * area) * open;
    float I = (float)(I_A * A_TO_PA);
    double zn = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;  // Zn²⁺ (µМ)
    I /= (1.0f + zn / OTOP1_ZN_IC50_UM);  // Zn²⁺ IC50 0.19 мМ (Tu 2018)

    // Ионная консервация H⁺ (входящий H⁺ = деполяризующий).
    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);
    s->intra_ions[ION_H] -= (I * dt) / cpm;
    s->extra_ions[ION_H] += (I * dt) / (cpm * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);
    return -I;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_OTOP1, otop1_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"OTOP1 протонный канал","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":181,"name":"OTOP1 протонный канал","category":"Other","description":"OTOP1 — протонный канал (кислый вкус тип III), H⁺-селективный. pH-гейт (state[4], pH<6, τ=10 мс). γ=20 пС (placeholder). GHK H⁺ (intra 0.0001 / extra 0.00004). Без ионной консервации. Tu 2018.","release":false}]})BIOCORE";
}
