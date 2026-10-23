// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_106_kir_channel.cpp — Входящий выпрямитель K⁺ (Kir2.x)
// Тип: MOD_KIR_CHANNEL (106)
//
// Kir (Inward Rectifier K⁺, Kir2.x) — стабилизатор потенциала покоя.
// Проводит K⁺ преимущественно внутрь при V < E_K
// (физиологический inward rectifier).
// Блокируется полиаминами/Mg²⁺ при деполяризации → выходящий ток подавлен.
// Активируется гиперполяризацией (V_half ≈ -80 mV).
//
// Архитектура: прямой GHK-поток K⁺ × Больцман-активация × rectification factor.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double kir_channel_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    // state[3] = gamma (пС, single-channel, default 30 — Kir2.1)
    double gamma_ps = (m->state[3] > 0.0f) ? m->state[3] : KIR_GAMMA_PS;
    // ✅ ДОСТОВЕРНО — менять ТОЛЬКО с согласования с пользователем (алгоритм аудита модификаторов).

    float K_i = s->intra_ions[ION_K];  // моль/м³
    float K_o = s->extra_ions[ION_K];

    // Woodhull N-сайтовый блок (Mg²⁺ + spermine) — честный inward-rectification.
    float f_open = kir_f_open(V);

    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * f_open;
    float I_K = (float)(I_A * A_TO_PA);

    float volume = segment_volume(s);
    float charge_per_mole_vol = FARADAY_CONST * volume / 1000.0f;
    s->intra_ions[ION_K] -= (I_K * dt) / charge_per_mole_vol;

    plugin_clamp_ions(s->intra_ions, ION_COUNT);

    return -I_K;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_KIR_CHANNEL, kir_channel_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Kir канал (Kir2.x)","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":106,"name":"Kir канал — входящий выпрямитель K⁺ (Kir2.x)","category":"Ионные каналы ритмогенеза","description":"Входящий выпрямитель K⁺ (Kir2.1/2.2/2.3). Активируется гиперполяризацией (V_half=-80mV). Rectification: блок полиаминами/Mg²⁺ при деполяризации → подавление выходящего тока. Стабилизирует RMP вблизи E_K (~ -80mV). Критичен для быстрого восстановления после спайка. GHK-поток K⁺.","release":false}]})BIOCORE";
}
