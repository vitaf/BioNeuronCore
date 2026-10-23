// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_140_cav21.cpp — пресинаптический Ca²⁺ канал P/Q-type (CACNA1A)
// Тип: MOD_CAV21 (140)
// Категория: Базовые модификаторы
// Био: триггер выброса нейромедиатора. Быстрая активация + медленная инактивация.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double cav21_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = plugin_segment_area(s);

    float m_inf = cav21_m_inf(V);
    m->state[0] += (m_inf - m->state[0]) * dt / CAV21_TAU_M_MS;

    float Ca = s->intra_ions[ION_CA] * MM_TO_UM; // µM
    float h_inf = cav21_h_inf(V, Ca);
    m->state[1] += (h_inf - m->state[1]) * dt / CAV21_TAU_H_MS;

    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    // Cav2.1 — пресинаптический (P/Q-type). Ca²⁺-ток идёт ТОЛЬКО в микродомен бутона
    // (bouton_ca_step), НЕ в сому. Соматический Ca-ток удалён (иначе держал dIN на плато).
    return 0.0f;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_CAV21, cav21_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Cav2.1 P/Q-type Ca²⁺ канал","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":140,"name":"Cav2.1 P/Q-type пресинаптический Ca²⁺ канал","category":"Базовые модификаторы","description":"Пресинаптический Ca²⁺ канал (CACNA1A), триггер выброса нейромедиатора. Быстрая активация (τ_m≈1мс, V½=−15мВ) + медленная инактивация (τ_h≈300мс, V½=−50мВ). GHK z=2, γ=15пС.","release":false}]})BIOCORE";
}
