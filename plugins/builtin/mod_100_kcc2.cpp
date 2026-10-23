// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_100_kcc2.cpp — K⁺-Cl⁻ котранспортёр (KCC2)
// Тип: MOD_KCC2 (100)
// Категория: Ca²⁺/K⁺/Cl⁻ гомеостаз
//
// KCC2 (K⁺-Cl⁻ Cotransporter 2) — нейрон-специфичный электронейтральный
// котранспортёр. 1 K⁺ : 1 Cl⁻ наружу. Критичен для поддержания низкого
// внутриклеточного [Cl⁻] (~5 mM при внеклеточном ~130 mM).
// Без KCC2 глициновые/GABA-A рецепторы становятся возбуждающими.
//
// Архитектура: прямой транспорт ионов, электронейтральный (ток = 0).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double kcc2_current_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — KCC2 K⁺-Cl⁻ котранспортёр (Payne 1997, Rivera 1999). Электронейтральный.
    float area = effective_area(m, s);

    // state[1] = K_m_K_i (mM, default 6.5), state[2] = K_m_Cl_i (mM, 6.5)
    double Km_K_i = (m->state[1] > 0.0f) ? m->state[1] : KCC2_K_M_K_I_MM;
    double Km_Cl_i = (m->state[2] > 0.0f) ? m->state[2] : KCC2_K_M_CL_I_MM;

    float K_i  = s->intra_ions[ION_K];   // мМ ≡ моль/м³
    float Cl_i = s->intra_ions[ION_CL];
    float K_o  = s->extra_ions[ION_K];
    float Cl_o = s->extra_ions[ION_CL];
    if (K_i  < 1e-9f) K_i  = 1e-9f;
    if (Cl_i < 1e-9f) Cl_i = 1e-9f;
    if (K_o  < 1e-9f) K_o  = 1e-9f;
    if (Cl_o < 1e-9f) Cl_o = 1e-9f;

    float f_K = K_i / (K_i + Km_K_i);
    float f_Cl = Cl_i / (Cl_i + Km_Cl_i);

    // Термодинамическая движущая сила: net flux ∝ 1 − [K]o[Cl]o/([K]i[Cl]i).
    // >0 → выводит K+Cl; <0 → вводит; =0 → равновесие.
    float driving = 1.0f - (K_o * Cl_o) / (K_i * Cl_i);
    if (driving > 1.0f) driving = 1.0f;
    if (driving < -1.0f) driving = -1.0f;

    // Поток J [моль/тик] = V_max[нмоль/см²/с]·1e-20 · density·area[µm²]·f_K·f_Cl·driving·dt[мс].
    float flux_mol = KCC2_VMAX_NMOL_PER_CM2_S * NMOL_CM2_S_TO_MOL_UM2_MS
                   * m->density * area * f_K * f_Cl * driving * dt;

    // Δc [мМ] = J / vol[m³]. Электронейтральный 1:1 K⁺:Cl⁻ (знак задаёт направление).
    float dconc = flux_mol / (segment_volume(s) * UM3_TO_M3);

    // Ограничить транспорт доступными ионами: вывод — из intra, ввод — из extra.
    if (dconc > 0.0f) {
        float k_avail  = K_i  - 1e-9f;
        float cl_avail = Cl_i - 1e-9f;
        float min_avail = (k_avail < cl_avail) ? k_avail : cl_avail;
        if (min_avail < 0.0f) min_avail = 0.0f;
        if (dconc > min_avail) dconc = min_avail;
    } else {
        float k_avail  = (K_o  - 1e-9f) * EXTRA_VOLUME_RATIO;
        float cl_avail = (Cl_o - 1e-9f) * EXTRA_VOLUME_RATIO;
        float min_avail = (k_avail < cl_avail) ? k_avail : cl_avail;
        if (min_avail < 0.0f) min_avail = 0.0f;
        float mag = -dconc;
        if (mag > min_avail) mag = min_avail;
        dconc = -mag;
    }
    s->intra_ions[ION_K]  -= dconc;
    s->intra_ions[ION_CL] -= dconc;
    s->extra_ions[ION_K]  += dconc / EXTRA_VOLUME_RATIO;
    s->extra_ions[ION_CL] += dconc / EXTRA_VOLUME_RATIO;
    plugin_clamp_ions(s->intra_ions, ION_COUNT);

    return 0.0f;  // электронейтральный — нет тока
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_KCC2, kcc2_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"KCC2 K⁺-Cl⁻ котранспортёр","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":100,"name":"KCC2 — K⁺-Cl⁻ котранспортёр","category":"Ca²⁺/K⁺/Cl⁻ гомеостаз","description":"Нейрон-специфичный электронейтральный K⁺-Cl⁻ котранспортёр (1K⁺:1Cl⁻ out). Критичен для поддержания низкого внутриклеточного [Cl⁻] (~5mM). Транс-ингибирование высоким внеклеточным K⁺. Без KCC2 глицин/GABA-A становятся возбуждающими.","release":false}]})BIOCORE";
}
