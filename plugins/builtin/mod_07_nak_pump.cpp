// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// nak_pump.cpp — Na⁺/K⁺-АТФаза
// Тип: MOD_PUMP_NAK (7)
// Категория: Базовые модификаторы
// Автогенерация: generate_builtin_plugins.py

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double nak_pump_current_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — Na/K-АТФаза, цикл Поста–Альберса (Smith & Crampin 2004, Läuger 1991).
    float area = effective_area(m, s);

    float v_norm = nak_pump_vnorm(s->V_m, s->intra_ions[ION_NA], s->extra_ions[ION_K], s->ATP_level,
                                  NA_K_PUMP_K_NA_MM, NA_K_PUMP_K_K_MM);

    // ЕДИНИЦА — молекула: I = N·k_cat·v_norm·e·Δz.
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

    return -I_pump; // электрогенный (+1 наружу) → гиперполяризующий
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_PUMP_NAK, nak_pump_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Na⁺/K⁺-АТФаза","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":7,"name":"Na⁺/K⁺-АТФаза","category":"Базовые модификаторы","description":"Электрогенный натрий-калиевый насос (3 Na⁺ наружу, 2 K⁺ внутрь), ATP-зависимый","release":false}]})BIOCORE";
}
