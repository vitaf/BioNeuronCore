// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_110_glial_nak_pump.cpp — Глиальная Na⁺/K⁺-АТФаза (α2-изоформа)
// Тип: MOD_GLIAL_NAK_PUMP (110)
//
// Глиальная Na/K-АТФаза (α2-изоформа) — ключевой элемент
// пространственного K⁺-буферинга. Отличия от нейрональной (α1/α3):
//   - Низкая скорость (p_rate ≈ 0.08 vs 0.15 нейрональной)
//   - Высокое сродство к внеклеточному K⁺ (Km ≈ 1.0 mM vs 1.5)
//   - Экструдирует K⁺ из глии обратно во внеклеточное пространство
//     в зонах с нормальным [K]o (вдали от активных нейронов)
//
// Совместно с Kir4.1 образует цикл: Kir4.1 забирает K⁺ в глию
// в активных зонах → K⁺ диффундирует по синцитию → Na/K-насос
// возвращает K⁺ во внеклетку в неактивных зонах.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double glial_nak_pump_current_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — глиальная Na/K-АТФаза (α2-изоформа). Тот же цикл Поста–Альберса, что α3,
    // но ниже I_max (2 µA/см²) и выше сродство к K⁺_o (K_m=1.0 мМ). Замыкает цикл K⁺-буферинга.
    float area = effective_area(m, s);

    float v_norm = nak_pump_vnorm(s->V_m, s->intra_ions[ION_NA], s->extra_ions[ION_K], s->ATP_level,
                                  NA_K_PUMP_K_NA_MM, NA_K_PUMP_GLIAL_K_K_MM);

    // ЕДИНИЦА — молекула: I = N·k_cat·v_norm·e·Δz (α2 тот же k_cat, различие — плотность, клетка).
    double N = m->density * area;
    float I_pump = N * v_norm * NA_K_PUMP_TURNOVER_MAX_S * ELEMENTARY_CHARGE_C * A_TO_PA; // пА

    // Перенос ионов: 3 Na⁺ наружу, 2 K⁺ внутрь (стехиометрия цикла).
    float volume = segment_volume(s);
    float cpm = FARADAY_CONST * volume / 1000.0f;
    float transport = I_pump * dt / cpm;
    s->intra_ions[ION_NA] -= 3.0f * transport;
    s->extra_ions[ION_NA] += 3.0f * transport / EXTRA_VOLUME_RATIO;
    s->intra_ions[ION_K]  += 2.0f * transport;
    s->extra_ions[ION_K]  -= 2.0f * transport / EXTRA_VOLUME_RATIO;
    clamp_ions(s->intra_ions, ION_COUNT);
    clamp_ions(s->extra_ions, ION_COUNT);

    s->ATP_level -= I_pump * dt * NA_K_PUMP_ATP_RATE_PA;
    if (s->ATP_level < 0.0f) s->ATP_level = 0.0f;

    return -I_pump;  // электрогенный (+1 наружу) → гиперполяризующий
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_GLIAL_NAK_PUMP, glial_nak_pump_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Глиальная Na⁺/K⁺-АТФаза (α2)","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":110,"name":"Глиальная Na⁺/K⁺-АТФаза (α2-изоформа)","category":"Глиальные","description":"Глиальная Na/K-АТФаза (α2-изоформа). Низкая скорость (p_rate=0.08), высокое сродство к внеклеточному K⁺ (Km=1.0mM). Экструдирует K⁺ из глии обратно во внеклетку в зонах с нормальным [K]o. Замыкает цикл пространственного K⁺-буферинга: Kir4.1 забирает → синцитий разносит → Na/K-насос возвращает.","release":false}]})BIOCORE";
}
