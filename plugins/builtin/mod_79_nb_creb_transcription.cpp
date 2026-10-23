// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_79_nb_creb_transcription.cpp — NB_CREB_Transcription
// Тип: MOD_NB_CREB_TRANSCRIPTION (79)
// Категория: Сома-модификаторы
// Ядерный CREB-опосредованный транскрипционный модификатор.
// Механизм: Ca²⁺→кальмодулин→CaMK→pCREB→транскрипция генов.
//
// State[0] = pCREB_active (0..1) — активированный CREB
// State[1] = late_gene (0..1) — поздние гены (cFos, BDNF, и т.д.)
// State[2] = structural_plasticity (0..1) — медленная структурная пластичность
//
// Сигнал транскрипции (transcription_signal) диффундирует через IntraConnection
// к другим сегментам нейрона (шаг 2.5 симуляции), где локальные модификаторы
// могут его читать для модуляции своей плотности/чувствительности.

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

// CREB pathway parameters
#define NB_CREB_TAU_CAMK    500.0f    // ms, CaMK активация (быстрая)
#define NB_CREB_TAU_PCREB   5000.0f   // ms, фосфорилирование CREB (минуты)
#define NB_CREB_TAU_LATE    30000.0f  // ms, поздние гены (часы)
#define NB_CREB_TAU_STRUCT  300000.0f // ms, структурная пластичность (дни)
#define NB_CREB_K_CA        0.5f      // полуактивация CaMK по Ca²⁺ (mM)
#define NB_CREB_N_CA        2.0f      // кооперативность Ca²⁺ для CaMK
#define NB_CREB_DECAY       0.0001f   // базовый распад pCREB (1/ms)

static double nb_creb_transcription_impl(Modifier* m, Segment* s, float dt) {
    float Ca_i = s->intra_ions[ION_CA];  // mM
    double pCREB = m->state[0];
    double late = m->state[1];
    double struct_pl = m->state[2];

    // CaMK активация: Hill-функция от Ca²⁺
    float Ca_n = powf(Ca_i, NB_CREB_N_CA);
    double p_Ca_Kd = (m->state[3] > 0.0f) ? m->state[3] : NB_CREB_K_CA;
    float K_n  = powf(p_Ca_Kd, NB_CREB_N_CA);
    float CaMK_active = Ca_n / (Ca_n + K_n);

    // pCREB: фосфорилирование CaMK-зависимое, дефосфорилирование фосфатазами
    float dpCREB = (CaMK_active * (1.0f - pCREB)) / NB_CREB_TAU_PCREB
                   - NB_CREB_DECAY * pCREB;

    // Поздние гены: активируются pCREB
    float dLate = (pCREB * (1.0f - late)) / NB_CREB_TAU_LATE
                  - late * 0.00001f;  // медленный распад мРНК

    // Структурная пластичность: самая медленная
    float dStruct = (late * (1.0f - struct_pl)) / NB_CREB_TAU_STRUCT
                    - struct_pl * 0.000001f;

    pCREB     += dpCREB * dt;
    late      += dLate * dt;
    struct_pl += dStruct * dt;

    if (pCREB  < 0.0f) pCREB  = 0.0f; if (pCREB  > 1.0f) pCREB  = 1.0f;
    if (late   < 0.0f) late   = 0.0f; if (late   > 1.0f) late   = 1.0f;
    if (struct_pl < 0.0f) struct_pl = 0.0f; if (struct_pl > 1.0f) struct_pl = 1.0f;

    m->state[0] = pCREB;
    m->state[1] = late;
    m->state[2] = struct_pl;

    // Транскрипционный сигнал = взвешенная сумма быстрой и медленной компонент
    // Записывается в поле Segment.transcription_signal
    s->transcription_signal = 0.5f * pCREB + 0.3f * late + 0.2f * struct_pl;

    return 0.0f;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_NB_CREB_TRANSCRIPTION, nb_creb_transcription_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"NB_CREB_Transcription","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":79,"name":"NB_CREB_Transcription","category":"Сома-модификаторы","description":"Ядерный CREB: Ca²⁺→CaMK→pCREB→поздние гены→структурная пластичность","release":false}]})BIOCORE";
}
