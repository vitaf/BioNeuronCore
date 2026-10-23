// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_107_k2p_leak.cpp — K2P канал пассивной K⁺ утечки
// Тип: MOD_K2P_LEAK (107)
//
// K2P (Two-Pore Domain K⁺ channels: TREK, TRAAK, TASK, TWIK) —
// конститутивно открытые каналы фоновой K⁺ проводимости.
// Обеспечивают пассивный K⁺ эффлюкс, стабилизируют потенциал покоя.
// Без гейтинга — всегда проводит K⁺ по электрохимическому градиенту.
// Критичен для K⁺ гомеостаза: высокий K_i → большой эффлюкс → авторегуляция.
//
// Архитектура: прямой GHK-поток K⁺, без гейтинга (open_prob = 1.0).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double k2p_leak_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    // state[0] = gamma (single-channel γ, пС, default 100 — TREK-1)
    // state[1] = Po (open probability, default 0.012 — TREK-1 покой)
    double gamma_ps = (m->state[0] > 0.0f) ? m->state[0] : 100.0f;
    double Po = (m->state[1] > 0.0f) ? m->state[1] : 0.012f;
    // ✅ ДОСТОВЕРНО — менять ТОЛЬКО с согласования с пользователем (алгоритм аудита модификаторов).

    float K_i = s->intra_ions[ION_K];  // моль/м³
    float K_o = s->extra_ions[ION_K];
    // Одноканальная проницаемость [м³/с] = γ·RT/(z²F²·c̄_K) — chord-conductance (Hille).
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    // Ток [пА] = P_single·flux·(density·area)·Po  (density=каналов/µm², area=µm²)
    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * Po;
    float I_K = (float)(I_A * A_TO_PA);

    float volume = segment_volume(s);
    float charge_per_mole_vol = FARADAY_CONST * volume / 1000.0f;
    s->intra_ions[ION_K] -= (I_K * dt) / charge_per_mole_vol;
    // Биологическая точность: K+ из клетки → во внеклеточное пространство
    // (объём внеклетки ~10× внутриклеточного)
    s->extra_ions[ION_K] += (I_K * dt) / (charge_per_mole_vol * EXTRA_VOLUME_RATIO);

    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);

    return -I_K;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_K2P_LEAK, k2p_leak_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"K2P канал пассивной K⁺ утечки","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":107,"name":"K2P — пассивная K⁺ утечка (TREK/TRAAK/TASK)","category":"Ионные каналы ритмогенеза","description":"Конститутивно открытый двухпоровый K⁺ канал фоновой проводимости (K2P: TREK, TRAAK, TASK, TWIK). Без гейтинга — всегда проводит K⁺ по градиенту. Обеспечивает пассивный K⁺ эффлюкс и стабилизацию потенциала покоя. Критичен для K⁺ гомеостаза. GHK-поток K⁺.","release":false}]})BIOCORE";
}
