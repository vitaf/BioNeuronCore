// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_122_nkcc1.cpp — Na⁺-K⁺-2Cl⁻ котранспортёр (NKCC1)
// Тип: MOD_NKCC1 (122)
// Категория: Cl⁻/K⁺/Na⁺ гомеостаз
//
// NKCC1 (Na-K-2Cl Cotransporter 1, SLC12A2) — электронейтральный котранспортёр.
// Импорт 1Na⁺:1K⁺:2Cl⁻ ВНУТРЬ клетки. Движущая сила — Na⁺ градиент
// (создаётся Na/K-АТФазой). Функциональный антагонист KCC2.
//
// Ключевая роль в онтогенетическом Cl⁻ переключателе:
// - Раннее развитие (Xenopus ст.22-31): NKCC1↑ KCC2↓ → [Cl⁻]i ~25mM → E_Cl ~-45mV
//   → GABA/глицин ДЕПОЛЯРИЗУЮЩИЕ (возбуждающие)
// - Зрелое развитие (ст.33+): NKCC1↓ KCC2↑ → [Cl⁻]i ~5mM → E_Cl ~-80mV
//   → GABA/глицин ГИПЕРПОЛЯРИЗУЮЩИЕ (тормозные)
//
// Архитектура: прямой транспорт ионов, электронейтральный (ток = 0).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double nkcc1_current_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — NKCC1 Na⁺-K⁺-2Cl⁻ котранспортёр (Delpire & Gagnon 2011). Электронейтральный.
    float area = effective_area(m, s);

    double Km_Na_i = (m->state[1] > 0.0f) ? m->state[1] : NKCC1_K_M_NA_I_MM;
    double Km_K_i = (m->state[2] > 0.0f) ? m->state[2] : NKCC1_K_M_K_I_MM;
    double Km_Cl_i = (m->state[3] > 0.0f) ? m->state[3] : NKCC1_K_M_CL_I_MM;
    double Km_Na_o = (m->state[4] > 0.0f) ? m->state[4] : NKCC1_K_M_NA_O_MM;
    double Km_K_o = (m->state[5] > 0.0f) ? m->state[5] : NKCC1_K_M_K_O_MM;

    float na_i = s->intra_ions[ION_NA];  // мМ ≡ моль/м³
    float k_i  = s->intra_ions[ION_K];
    float cl_i = s->intra_ions[ION_CL];
    float na_o = s->extra_ions[ION_NA];
    float k_o  = s->extra_ions[ION_K];
    if (na_i < 1e-9f) na_i = 1e-9f;
    if (k_i  < 1e-9f) k_i  = 1e-9f;
    if (cl_i < 1e-9f) cl_i = 1e-9f;
    if (na_o < 1e-9f) na_o = 1e-9f;
    if (k_o  < 1e-9f) k_o  = 1e-9f;

    float na_factor = na_i / (na_i + Km_Na_i);
    float k_factor  = k_i  / (k_i  + Km_K_i);
    float cl_factor = cl_i / (cl_i + Km_Cl_i);
    float trans_stim  = na_o / (na_o + Km_Na_o);
    float trans_inhib = 1.0f / (1.0f + k_o / Km_K_o);

    // Поток J [моль/тик] = V_max[нмоль/см²/с]·1e-20 · density·area[µm²]·factors·dt[мс].
    float flux_mol = NKCC1_VMAX_NMOL_PER_CM2_S * NMOL_CM2_S_TO_MOL_UM2_MS
                   * m->density * area * na_factor * k_factor * cl_factor * trans_stim * trans_inhib * dt;

    // Δc [мМ] = J / vol[m³]. Электронейтральный 1:1:2 внутрь.
    float dconc = flux_mol / (segment_volume(s) * UM3_TO_M3);
    s->intra_ions[ION_NA] += dconc;
    s->intra_ions[ION_K]  += dconc;
    s->intra_ions[ION_CL] += 2.0f * dconc;
    s->extra_ions[ION_NA] -= dconc / EXTRA_VOLUME_RATIO;
    s->extra_ions[ION_K]  -= dconc / EXTRA_VOLUME_RATIO;
    s->extra_ions[ION_CL] -= 2.0f * dconc / EXTRA_VOLUME_RATIO;
    plugin_clamp_ions(s->intra_ions, ION_COUNT);

    return 0.0f;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NKCC1, nkcc1_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return "{"
        "\"id\": 122,"
        "\"name\": \"NKCC1\","
        "\"full_name\": \"Na-K-2Cl Cotransporter 1 (SLC12A2)\","
        "\"category\": \"Cl/K/Na homeostasis\","
        "\"description\": \"Electroneutral Na-K-2Cl cotransporter. Imports Cl- using Na+ gradient. Antagonist of KCC2. Key to developmental Cl- switch.\","
        "\"electrogenic\": false,"
        "\"ions\": [\"Na+\", \"K+\", \"Cl-\"],"
        "\"state_vars\": [\"v_max\", \"Km_Na_i\", \"Km_K_i\", \"Km_Cl_i\", \"Km_Na_o\", \"Km_K_o\"]"
        "}";
}
