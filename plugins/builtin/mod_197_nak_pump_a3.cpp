// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// nak_pump_a3.cpp — Na⁺/K⁺-АТФаза α3 (динамический насос, usAHP)
// Тип: MOD_PUMP_NAK_A3 (197)
// Категория: Базовые модификаторы
// Автогенерация: generate_builtin_plugins.py

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double nak_pump_a3_current_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — α3-изоформа Na/K-АТФазы (Zahler 1997, Blanco 2005):
    // НИЗКОЕ сродство к Na⁺_i (K_m≈25 мМ) → тонически почти неактивна, активируется
    // при росте [Na⁺]_i во время залпа спайков → usAHP (обрыв повторной стрельбы).
    float area = effective_area(m, s);

    float v_norm = nak_pump_vnorm(s->V_m, s->intra_ions[ION_NA], s->extra_ions[ION_K], s->ATP_level,
                                  NA_K_PUMP_A3_K_NA_MM, NA_K_PUMP_K_K_MM);

    // Уабаин-ингибиция (state[5], нМ) — конкурентный блок E2P-конформации (Blanco 1993).
    double oua = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    v_norm /= (1.0f + oua / NA_K_PUMP_OUABAIN_IC50_NM);

    // ЕДИНИЦА — молекула: I = N·k_cat·v_norm·e·Δz.
    double N = m->density * area;
    float I_pump = N * v_norm * NA_K_PUMP_TURNOVER_MAX_S * ELEMENTARY_CHARGE_C * A_TO_PA; // пА

    // Перенос ионов: 3 Na⁺ наружу, 2 K⁺ внутрь (стехиометрия цикла).
    float volume = segment_volume(s);
    float cpm = FARADAY_CONST * volume / 1000.0f;
    float transport = I_pump * dt / cpm;
    // Ограничить транспорт доступными ионами (как nak_pump_current: 3 Na intra, 2 K extra).
    float na_avail = (s->intra_ions[ION_NA] - 1e-9f) / 3.0f;
    float k_avail  = (s->extra_ions[ION_K]  - 1e-9f) * EXTRA_VOLUME_RATIO / 2.0f;
    if (na_avail < 0.0f) na_avail = 0.0f;
    if (k_avail  < 0.0f) k_avail  = 0.0f;
    float max_t = (na_avail < k_avail) ? na_avail : k_avail;
    if (transport > max_t) transport = max_t;

    s->intra_ions[ION_NA] -= 3.0f * transport;
    s->extra_ions[ION_NA] += 3.0f * transport / EXTRA_VOLUME_RATIO;
    s->intra_ions[ION_K]  += 2.0f * transport;
    s->extra_ions[ION_K]  -= 2.0f * transport / EXTRA_VOLUME_RATIO;
    clamp_ions(s->intra_ions, ION_COUNT);
    clamp_ions(s->extra_ions, ION_COUNT);

    s->ATP_level -= I_pump * dt * NA_K_PUMP_ATP_RATE_PA;
    if (s->ATP_level < 0.0f) s->ATP_level = 0.0f;

    return -I_pump; // электрогенный (+1 наружу) → гиперполяризующий
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_PUMP_NAK_A3, nak_pump_a3_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Na⁺/K⁺-АТФаза α3","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":197,"name":"Na⁺/K⁺-АТФаза α3","category":"Базовые модификаторы","description":"Динамический Na/K-насос α3 (низкое сродство к Na⁺_i K_m≈25 мМ), генерирует usAHP при залпе; уабаин-чувствительный","release":false}]})BIOCORE";
}
