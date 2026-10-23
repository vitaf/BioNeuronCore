// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_102_ca_buffer.cpp — Быстрый Ca²⁺ буфер (парвальбумин-подобный)
// Тип: MOD_CA_BUFFER (102)
// Категория: Ca²⁺/K⁺/Cl⁻ гомеостаз
//
// Моделирует быстрый Ca²⁺-связывающий белок (парвальбумин/кальбиндин).
// Кинетика: Ca²⁺ + B ⇌ CaB (обратимое связывание).
// Снижает амплитуду Ca²⁺-транзиентов, сглаживает пики [Ca²⁺]i.
// Критичен для предотвращения Ca²⁺-перегрузки при высокочастотной активности CPG.
//
// Архитектура: прямой учёт свободного/связанного Ca²⁺, без суррогатных формул.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double ca_buffer_current_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — Ca²⁺-буфер (парвальбумин-подобный, Schwaller 2010). Без тока.
    // density масштабирует ЁМКОСТЬ B_total (количество белка), кинетика k_on/k_off — свойство молекулы.

    double B_total_ref = (m->state[0] > 0.0f) ? m->state[0] : CA_BUFFER_B_TOTAL_MM;
    double B_total = B_total_ref * m->density;   // density → ёмкость
    double K_d = (m->state[2] > 0.0f) ? m->state[2] : CA_BUFFER_K_D_MM;
    double k_on = (m->state[3] > 0.0f) ? m->state[3] : CA_BUFFER_K_ON_MM_MS;
    double k_off = (m->state[4] > 0.0f) ? m->state[4] : (K_d * k_on);

    // Инициализация Ca_bound при первом вызове
    double Ca_bound = m->state[1];
    if (Ca_bound <= 0.0f && B_total > 0.0f) {
        float Ca_free = s->intra_ions[ION_CA];
        Ca_bound = B_total * Ca_free / (Ca_free + K_d);
        m->state[1] = Ca_bound;
    }

    // Прямая кинетика: Ca²⁺ + B ⇌ CaB
    float Ca_free = s->intra_ions[ION_CA];
    if (Ca_free < 1e-9f) Ca_free = 1e-9f;

    float B_free = B_total - Ca_bound;
    if (B_free < 0.0f) B_free = 0.0f;

    // d[CaB]/dt = k_on × [Ca] × [B_free] - k_off × [CaB]
    float dCa_bound = (k_on * Ca_free * B_free - k_off * Ca_bound) * dt;
    Ca_bound += dCa_bound;
    if (Ca_bound < 0.0f) Ca_bound = 0.0f;
    if (Ca_bound > B_total) Ca_bound = B_total;

    // Перемещение Ca²⁺ между свободным и связанным пулами
    s->intra_ions[ION_CA] -= dCa_bound;
    if (s->intra_ions[ION_CA] < 1e-9f) {
        // Возвращаем из буфера если свободный истощился
        float deficit = 1e-9f - s->intra_ions[ION_CA];
        s->intra_ions[ION_CA] = 1e-9f;
        Ca_bound -= deficit;
        if (Ca_bound < 0.0f) Ca_bound = 0.0f;
    }

    m->state[1] = Ca_bound;
    plugin_clamp_ions(s->intra_ions, ION_COUNT);

    return 0.0f;  // буфер не создаёт тока
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_CA_BUFFER, ca_buffer_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Ca²⁺ буфер (парвальбумин)","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":102,"name":"Ca²⁺ буфер — парвальбумин-подобный","category":"Ca²⁺/K⁺/Cl⁻ гомеостаз","description":"Быстрый Ca²⁺-связывающий белок (парвальбумин/кальбиндин). Обратимая кинетика: Ca²⁺+B⇌CaB. B_total=150µM, K_d=500nM. Снижает амплитуду Ca²⁺-транзиентов, предотвращает Ca²⁺-перегрузку при высокочастотной активности CPG.","release":false}]})BIOCORE";
}
