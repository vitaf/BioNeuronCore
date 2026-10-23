// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_99_ncx.cpp — Na⁺/Ca²⁺ обменник (NCX)
// Тип: MOD_NCX (99)
// Категория: Ca²⁺/K⁺/Cl⁻ гомеостаз
//
// NCX (Na⁺/Ca²⁺ Exchanger) — низкоаффинный, высокоёмкостный обменник.
// Стехиометрия: 3 Na⁺ внутрь : 1 Ca²⁺ наружу (forward mode).
// Электрогенный: нетто +1 заряд внутрь за цикл → деполяризующий ток.
// Может реверсировать при высоком [Na⁺]i или деполяризации.
// V-зависимость через eta-фактор.
//
// Архитектура: прямой транспорт ионов, без суррогатных формул.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double ncx_current_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — NCX Na⁺/Ca²⁺ обменник (Weber 2001, Shannon 2004). 3Na:1Ca, реверсивный.
    float area = effective_area(m, s);

    float Na_i = s->intra_ions[ION_NA]; if (Na_i < 1e-9f) Na_i = 1e-9f;
    float Na_o = s->extra_ions[ION_NA]; if (Na_o < 1e-9f) Na_o = 1e-9f;
    float Ca_i = s->intra_ions[ION_CA]; if (Ca_i < 1e-9f) Ca_i = 1e-9f;
    float Ca_o = s->extra_ions[ION_CA]; if (Ca_o < 1e-9f) Ca_o = 1e-9f;

    // Реверсивный поток (знак меняется при E_NaCa), нормированный на I_max.
    float flux_norm = ncx_flux_norm(s->V_m, Na_i, Na_o, Ca_i, Ca_o);

    // ЕДИНИЦА — молекула: I = N·k_cat·flux_norm·e·Δz (Δz=+1, forward = +1 внутрь).
    double N = m->density * area;
    float I_ncx = N * NCX_TURNOVER_MAX_S * flux_norm * ELEMENTARY_CHARGE_C * A_TO_PA; // пА

    // Транспорт: 1 Ca²⁺ и 3 Na⁺ на цикл (знаки разворачиваются в reverse автоматически).
    float volume = segment_volume(s);
    float cpv = FARADAY_CONST * volume / 1000.0f;
    float transport = I_ncx * dt / cpv;
    s->intra_ions[ION_CA] -= transport;             // 1 Ca наружу (forward)
    s->extra_ions[ION_CA] += transport / EXTRA_VOLUME_RATIO;
    s->intra_ions[ION_NA] += 3.0f * transport;      // 3 Na внутрь (forward)
    s->extra_ions[ION_NA] -= 3.0f * transport / EXTRA_VOLUME_RATIO;
    plugin_clamp_ions(s->intra_ions, ION_COUNT);

    return I_ncx;  // forward (+1 внутрь) = деполяризующий
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NCX, ncx_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"NCX Na⁺/Ca²⁺ обменник","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":99,"name":"NCX — Na⁺/Ca²⁺ обменник","category":"Ca²⁺/K⁺/Cl⁻ гомеостаз","description":"Низкоаффинный высокоёмкостный Na⁺/Ca²⁺ обменник. Стехиометрия 3Na⁺ in : 1Ca²⁺ out (forward mode). Электрогенный (+1 заряд/цикл, деполяризующий). V-зависимость через eta-фактор. Allosteric Ca²⁺-активация. Совместно с PMCA поддерживает базальный [Ca²⁺]i.","release":false}]})BIOCORE";
}
