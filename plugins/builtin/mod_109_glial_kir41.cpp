// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_109_glial_kir41.cpp — Kir4.1 глиальный входящий выпрямитель K⁺
// Тип: MOD_GLIAL_KIR41 (109)
//
// Kir4.1 (KCNJ10) — основной K⁺-канал глиальных клеток (астроциты,
// радиальная глия, Müller cells). Критичен для пространственного
// K⁺-буферинга: забирает избыток внеклеточного K⁺ в глиальный синцитий.
//
// Отличия от нейронального Kir2.x:
//   - Более сильная inward rectification (крутой полиаминовый блок)
//   - [K]o-чувствительность: деполяризация при высоком [K]o → больше K⁺ внутрь
//   - Медленная кинетика (нет быстрых гейтов — constitutive + rectification)
//   - Работает КАК НАСОС для K⁺: из extra → в intra глии
//
// Архитектура: GHK-поток K⁺ × rectification × [K]o-зависимая проницаемость.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double glial_kir41_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    // Kir4.1 — конститутивный inward rectifier (нет voltage-gated активации).
    // state[3] = γ (пС, default 25) — single-channel проводимость.
    double gamma_ps = (m->state[3] > 0.0f) ? m->state[3] : 25.0f;

    // ── GHK-поток K⁺, P в см/с ───────────────────────────────────────
    float K_i = s->intra_ions[ION_K];  // моль/м³
    float K_o = s->extra_ions[ION_K];

    // ── Сильная inward rectification (Kir4.1: круче чем Kir2.x) ─────
    float E_K = (GAS_CONST * CELSIUS_37 / FARADAY_CONST) * logf(K_o / K_i) * 1000.0f;
    float rect = 1.0f / (1.0f + expf((V - E_K) / 12.0f));

    // ── [K]o-чувствительность: пропускаемость растёт с [K]o ──────────
    // Биологически: Kir4.1 проводимость растёт пропорционально sqrt([K]o)
    // при повышении внеклеточного K⁺ (механизм spatial buffering)
    float K_o_baseline = 4.5f;  // mM — нормальный внеклеточный K⁺
    float K_o_mM = s->extra_ions[ION_K];  // уже в mM
    float ko_factor = sqrtf(K_o_mM / K_o_baseline);
    if (ko_factor < 0.3f) ko_factor = 0.3f;
    if (ko_factor > 5.0f) ko_factor = 5.0f;

    // ── Итоговый K⁺ ток (входящий = отрицательный) ──────────────────
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * ko_factor * rect;
    float I_K = (float)(I_A * A_TO_PA);

    // Перемещение K⁺: входящий ток → K⁺ входит в глию (intra ↑)
    float volume = segment_volume(s);
    float charge_per_mole_vol = FARADAY_CONST * volume / 1000.0f;
    s->intra_ions[ION_K] -= (I_K * dt) / charge_per_mole_vol;
    // K⁺ забирается из внеклеточного пространства
    s->extra_ions[ION_K] += (I_K * dt) / (charge_per_mole_vol * EXTRA_VOLUME_RATIO);

    plugin_clamp_ions(s->intra_ions, ION_COUNT);

    return -I_K;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_GLIAL_KIR41, glial_kir41_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Kir4.1 глиальный K⁺ канал","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":109,"name":"Kir4.1 — глиальный входящий выпрямитель K⁺","category":"Глиальные","description":"Kir4.1 (KCNJ10) — основной K⁺-канал астроцитов/радиальной глии. Сильная inward rectification (полиаминовый блок), [K]o-чувствительность (проводимость ∝ sqrt([K]o)), медленная кинетика. Критичен для пространственного K⁺-буферинга: забирает избыток внеклеточного K⁺ в глиальный синцитий. GHK-поток K⁺.","release":false}]})BIOCORE";
}
