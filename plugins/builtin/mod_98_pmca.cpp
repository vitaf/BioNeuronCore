// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_98_pmca.cpp — Ca²⁺-АТФаза плазмалеммы (PMCA)
// Тип: MOD_PMCA (98)
// Категория: Ca²⁺/K⁺/Cl⁻ гомеостаз
//
// PMCA (Plasma Membrane Ca²⁺ ATPase) — высокоаффинный, низкоёмкостный насос.
// Использует ATP для выброса 1 Ca²⁺ из клетки за цикл.
// Электрогенный: выброс 2 положительных зарядов → гиперполяризующий ток.
// Кинетика Hill n=2, K_m ≈ 300 nM (0.0003 mM).
// Совместно с NCX поддерживает базальный [Ca²⁺]i ≈ 50-100 nM.
//
// Архитектура: прямой транспорт ионов, без суррогатных формул.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double pmca_current_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — PMCA Ca²⁺-АТФаза (Carafoli 1991). Hill-2, K_m=300нМ, электрогенный Ca²⁺ наружу.
    float area = effective_area(m, s);

    // state[1] = K_m_Ca (mM, default 300 нМ), state[2] = Hill_n (default 2)
    double K_m_Ca = (m->state[1] > 0.0f) ? m->state[1] : PMCA_K_M_CA_M_M;
    double hill_n = (m->state[2] > 0.0f) ? m->state[2] : PMCA_HILL_N;

    // ATP: концентрация в мМ (свойство клетки). Michaelis-Menten, K_m_ATP в мМ.
    float atp = s->ATP_level;
    if (atp < 0.0f) atp = 0.0f;
    float f_ATP = atp / (atp + PMCA_K_ATP_MM);

    // Ca²⁺-зависимость: Hill-2
    float ca = s->intra_ions[ION_CA];
    if (ca < 1e-9f) ca = 1e-9f;
    float ca_pow = powf(ca, hill_n);
    float f_Ca = ca_pow / (ca_pow + powf(K_m_Ca, hill_n));

    // ЕДИНИЦА — молекула: I = N·k_cat·f_Ca·f_ATP·e·Δz (Δz=+2, 1 Ca²⁺ наружу).
    double N = m->density * area;
    float I_pump = N * PMCA_TURNOVER_MAX_S * f_Ca * f_ATP * ELEMENTARY_CHARGE_C * 2.0f * A_TO_PA; // пА

    // Транспорт 1 Ca²⁺ наружу (z=2 → /2).
    float volume = segment_volume(s);
    float cpv = FARADAY_CONST * volume / 1000.0f;
    float transport = I_pump * dt / (2.0f * cpv);

    s->intra_ions[ION_CA] -= transport;
    s->extra_ions[ION_CA] += transport / EXTRA_VOLUME_RATIO;
    plugin_clamp_ions(s->intra_ions, ION_COUNT);

    // ATP: 1 ATP на 1 Ca²⁺.
    s->ATP_level -= I_pump * dt * NA_K_PUMP_ATP_RATE_PA;
    if (s->ATP_level < 0.0f) s->ATP_level = 0.0f;

    // Электрогенный (+2 наружу) → гиперполяризующий → отрицательный ток.
    return -I_pump;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_PMCA, pmca_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"PMCA Ca²⁺-АТФаза","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":98,"name":"PMCA — Ca²⁺-АТФаза плазмалеммы","category":"Ca²⁺/K⁺/Cl⁻ гомеостаз","description":"Высокоаффинная Ca²⁺-АТФаза плазматической мембраны. Использует ATP для выброса 1 Ca²⁺ за цикл. Hill n=2, K_m=300nM. Электрогенный гиперполяризующий ток. Совместно с NCX поддерживает базальный [Ca²⁺]i ≈ 50-100 nM.","release":false}]})BIOCORE";
}
