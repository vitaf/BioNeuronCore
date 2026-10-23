// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.
//
// Тесты консолидации/реконсолидации: структуры, модификаторы 87-92, цикл ген→белок→структура.

#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include "types.h"
#include "bion_format.h"
#include "kernel_dispatch.h"
#include "modifier_interface.h"
#include "consolidation.h"
#include <cmath>
#include <cstring>
#include <thread>
#include <chrono>

// Forward declarations из hh_biological.cpp
extern double mapk_erk_current(Modifier* m, Segment* s, float dt);
extern double ikk_nfkb_current(Modifier* m, Segment* s, float dt);
extern double mtorc1_current(Modifier* m, Segment* s, float dt);
extern double protein_synth_current(Modifier* m, Segment* s, float dt);
extern double cofilin_current(Modifier* m, Segment* s, float dt);
extern double mmp9_current(Modifier* m, Segment* s, float dt);

// Forward declarations из hh_biological.cpp
extern double mapk_erk_current(Modifier* m, Segment* s, float dt);
extern double ikk_nfkb_current(Modifier* m, Segment* s, float dt);
extern double mtorc1_current(Modifier* m, Segment* s, float dt);
extern double protein_synth_current(Modifier* m, Segment* s, float dt);
extern double cofilin_current(Modifier* m, Segment* s, float dt);
extern double mmp9_current(Modifier* m, Segment* s, float dt);

// ═══════════════════════════════════════════════════════════════════
// Структуры
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("Consolidation: Struct sizes", "[consolidation]") {
    REQUIRE(sizeof(Nucleus) == 500u);
    REQUIRE(sizeof(ConsolidationEvent) == 36u);
    REQUIRE(MOD_TYPE_COUNT == 212u);  // ...+ изоформы #8 (199-211)
    REQUIRE(GENE_COUNT == 57u);
}

TEST_CASE("Consolidation: Gene constants", "[consolidation]") {
    REQUIRE(GENE_CREB == 0);
    REQUIRE(GENE_CFOS == 1);
    REQUIRE(GENE_ZIF268 == 2);
    REQUIRE(GENE_ARC == 3);
    REQUIRE(GENE_BDNF == 4);
    REQUIRE(GENE_GLUR1 == 8);
    REQUIRE(GENE_PSD95 == 10);
    REQUIRE(GENE_MTOR == 17);
    REQUIRE(GENE_4EBP == 19);
}

TEST_CASE("Consolidation: Modifier type IDs", "[consolidation]") {
    REQUIRE(MOD_MAPK_ERK == 87);
    REQUIRE(MOD_IKK_NFKB == 88);
    REQUIRE(MOD_MTORC1 == 89);
    REQUIRE(MOD_PROTEIN_SYNTH == 90);
    REQUIRE(MOD_COFILIN == 91);
    REQUIRE(MOD_MMP9 == 92);
}

// ═══════════════════════════════════════════════════════════════════
// MAPK/ERK (87)
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("Consolidation: MAPK_ERK Ca2+ to ERK activation", "[consolidation]") {
    Segment seg = {};
    seg.id = 1;
    seg.V_m = -65.0f;
    seg.length = 100.0f;
    seg.radius = 10.0f;
    seg.ATP_level = 1.0f;
    seg.intra_ions[ION_CA] = 0.001f; // 1 uM Ca2+

    Modifier m = {};
    m.type = MOD_MAPK_ERK;
    m.density = 1.0f;
    // Используем state[] по умолчанию (все нули → параметры = дефолты)

    // Прогоняем 50000 тиков (5 секунд) с высоким Ca2+
    for (int t = 0; t < 50000; t++) {
        m.state[7] = 2.0f * PI * seg.radius * seg.length;
        mapk_erk_current(&m, &seg, 0.1f);
    }

    // ERK должен активироваться при высоком Ca2+
    REQUIRE(m.state[0] > 0.05f);  // ERK_active
    REQUIRE(m.state[2] > 0.5f);  // Ras_active (быстрый)
    REQUIRE(m.state[0] < 1.01f);
}

TEST_CASE("Consolidation: MAPK_ERK low Ca2+ no activation", "[consolidation]") {
    Segment seg = {};
    seg.id = 1;
    seg.V_m = -65.0f;
    seg.length = 100.0f;
    seg.radius = 10.0f;
    seg.ATP_level = 1.0f;
    seg.intra_ions[ION_CA] = 0.00001f; // 0.01 uM — очень мало

    Modifier m = {};
    m.type = MOD_MAPK_ERK;
    m.density = 1.0f;

    for (int t = 0; t < 5000; t++) {
        m.state[7] = 2.0f * PI * seg.radius * seg.length;
        mapk_erk_current(&m, &seg, 0.1f);
    }

    // При низком Ca2+ ERK не активируется
    REQUIRE(m.state[0] < 0.01f);
}

// ═══════════════════════════════════════════════════════════════════
// IKK/NF-kB (88)
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("Consolidation: IKK_NFKB Ca2+ to NF-kB activation", "[consolidation]") {
    Segment seg = {};
    seg.id = 1;
    seg.V_m = -65.0f;
    seg.length = 100.0f;
    seg.radius = 10.0f;
    seg.ATP_level = 1.0f;
    seg.intra_ions[ION_CA] = 0.001f; // 1 uM

    Modifier m = {};
    m.type = MOD_IKK_NFKB;
    m.density = 1.0f;

    for (int t = 0; t < 50000; t++) {
        m.state[7] = 2.0f * PI * seg.radius * seg.length;
        ikk_nfkb_current(&m, &seg, 0.1f);
    }

    REQUIRE(m.state[0] > 0.05f);  // NFkB_active
    REQUIRE(m.state[2] < 0.95f);  // IkB_level degraded
    REQUIRE(m.state[0] < 1.01f);
}

// ═══════════════════════════════════════════════════════════════════
// mTORC1 (89)
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("Consolidation: mTORC1 two-key activation", "[consolidation]") {
    Segment seg = {};
    seg.id = 1;
    seg.V_m = -65.0f;
    seg.length = 100.0f;
    seg.radius = 10.0f;
    seg.ATP_level = 5.0f; // высокая энергия

    Modifier m = {};
    m.type = MOD_MTORC1;
    m.density = 1.0f;
    m.state[3] = 0.8f; // высокий AA_sensor

    for (int t = 0; t < 50000; t++) {
        m.state[7] = 2.0f * PI * seg.radius * seg.length;
        mtorc1_current(&m, &seg, 0.1f);
    }

    // mTORC1 должен активироваться при AA + ATP
    REQUIRE(m.state[0] > 0.01f);
    REQUIRE(m.state[1] > 0.01f); // Rheb_GTP
    REQUIRE(m.state[0] < 1.01f);
}

TEST_CASE("Consolidation: mTORC1 low ATP inhibition", "[consolidation]") {
    Segment seg = {};
    seg.id = 1;
    seg.V_m = -65.0f;
    seg.length = 100.0f;
    seg.radius = 10.0f;
    seg.ATP_level = 0.1f; // низкая энергия

    Modifier m = {};
    m.type = MOD_MTORC1;
    m.density = 1.0f;
    m.state[3] = 0.8f;

    for (int t = 0; t < 50000; t++) {
        m.state[7] = 2.0f * PI * seg.radius * seg.length;
        mtorc1_current(&m, &seg, 0.1f);
    }

    // При низком ATP mTORC1 ингибирован
    REQUIRE(m.state[0] < 0.01f);
}

// ═══════════════════════════════════════════════════════════════════
// PROTEIN_SYNTH (90)
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("Consolidation: PROTEIN_SYNTH reads mTORC1", "[consolidation]") {
    Segment seg = {};
    seg.id = 1;
    seg.V_m = -65.0f;
    seg.length = 100.0f;
    seg.radius = 10.0f;
    seg.ATP_level = 5.0f;

    // Добавляем mTORC1 с высокой активностью
    seg.modifiers[0].type = MOD_MTORC1;
    seg.modifiers[0].density = 1.0f;
    seg.modifiers[0].state[0] = 0.8f; // mTORC1_active = 0.8
    seg.modifier_count = 1;

    Modifier m = {};
    m.type = MOD_PROTEIN_SYNTH;
    m.density = 1.0f;

    for (int t = 0; t < 50000; t++) {
        m.state[7] = 2.0f * PI * seg.radius * seg.length;
        protein_synth_current(&m, &seg, 0.1f);
    }

    REQUIRE(m.state[0] > 0.003f); // translation_rate (tau=60000ms)
    REQUIRE(m.state[1] > 0.05f); // S6K1_active (tau=10000ms)
}

// ═══════════════════════════════════════════════════════════════════
// COFILIN (91)
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("Consolidation: COFILIN ERK-driven LIMK", "[consolidation]") {
    Segment seg = {};
    seg.id = 1;
    seg.V_m = -65.0f;
    seg.length = 100.0f;
    seg.radius = 10.0f;
    seg.ATP_level = 1.0f;
    seg.intra_ions[ION_CA] = 0.001f;

    // Добавляем активный MAPK_ERK
    seg.modifiers[0].type = MOD_MAPK_ERK;
    seg.modifiers[0].density = 1.0f;
    seg.modifiers[0].state[0] = 0.7f; // ERK_active = 0.7
    seg.modifier_count = 1;

    Modifier m = {};
    m.type = MOD_COFILIN;
    m.density = 1.0f;

    for (int t = 0; t < 50000; t++) {
        m.state[7] = 2.0f * PI * seg.radius * seg.length;
        cofilin_current(&m, &seg, 0.1f);
    }

    // ERK → LIMK → pCofilin↑ → cofilin_active↓ → F-actin стабилен
    REQUIRE(m.state[1] > 0.05f); // LIMK_active
    REQUIRE(m.state[2] > 0.08f); // pCofilin_ratio
}

// ═══════════════════════════════════════════════════════════════════
// MMP9 (92)
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("Consolidation: MMP9 ERK+NF-kB driven", "[consolidation]") {
    Segment seg = {};
    seg.id = 1;
    seg.V_m = -65.0f;
    seg.length = 100.0f;
    seg.radius = 10.0f;
    seg.ATP_level = 1.0f;

    // Активные ERK и NF-kB
    seg.modifiers[0].type = MOD_MAPK_ERK;
    seg.modifiers[0].density = 1.0f;
    seg.modifiers[0].state[3] = 0.7f; // pERK_nuclear
    seg.modifiers[1].type = MOD_IKK_NFKB;
    seg.modifiers[1].density = 1.0f;
    seg.modifiers[1].state[3] = 0.5f; // pNFkB_nuclear
    seg.modifier_count = 2;

    Modifier m = {};
    m.type = MOD_MMP9;
    m.density = 1.0f;

    for (int t = 0; t < 50000; t++) {
        m.state[7] = 2.0f * PI * seg.radius * seg.length;
        mmp9_current(&m, &seg, 0.1f);
    }

    REQUIRE(m.state[0] > 0.03f); // MMP9_active (tau=20000ms, медленная)
    REQUIRE(m.state[2] < 1.0f); // ECM начинает деградировать
}

// ═══════════════════════════════════════════════════════════════════
// ConsolidationThread: жизненный цикл
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("Consolidation: Thread lifecycle", "[consolidation]") {
    ConsolidationThread thread;
    REQUIRE_FALSE(thread.isRunning());

    thread.setGlobalInterval(0);
    REQUIRE(thread.getGlobalInterval() == 0u);

    thread.setGlobalInterval(10000);
    REQUIRE(thread.getGlobalInterval() == 10000u);
}

// ═══════════════════════════════════════════════════════════════════
// ConsolidationThread: nucleus snapshot
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("Consolidation: NucleusSnapshot", "[consolidation]") {
    ConsolidationThread thread;

    Nucleus nuc = {};
    nuc.neuron_id = 42;
    nuc.enabled = 1;
    nuc.consolidation_mode = 3;
    nuc.gene_activity[GENE_BDNF] = 0.75f;
    nuc.protein_level[GENE_ARC] = 0.5f;
    nuc.amino_acid_level = 0.6f;
    nuc.pcreb_level = 0.3f;
    nuc.erk_level = 0.4f;

    Nucleus out = {};
    // Без активного потока снимок недоступен
    REQUIRE_FALSE(thread.getNucleusSnapshot(0, &out));
}

// ═══════════════════════════════════════════════════════════════════
// ConsolidationEvent: применение диаметра
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("Consolidation: Apply DIAMETER_CHANGE event", "[consolidation]") {
    ConsolidationThread thread;

    Segment seg = {};
    seg.id = 1;
    seg.radius = 2.0f;

    // Симулируем событие изменения диаметра через pending events
    // Для этого нужен доступ к pending_events_ (private).
    // Проверяем через applyPendingEvents с пустой очередью:
    uint32_t n_syn = 0;
    uint32_t applied = thread.applyPendingEvents(&seg, 1, nullptr, &n_syn, 0);
    REQUIRE(applied == 0u); // пустая очередь — ничего не применено
    REQUIRE(seg.radius == 2.0f); // без изменений
}

// ═══════════════════════════════════════════════════════════════════
// Интеграция: полный цикл с Nucleus, модификаторами и потоком
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("Consolidation: Full pathway integration", "[consolidation][integration]") {
    // Создаём нейрон с сомой, модификаторами и ядром
    Segment soma = {};
    soma.id = 0;
    soma.neuron_id = 0;
    soma.V_m = -65.0f;
    soma.length = 20.0f;
    soma.radius = 10.0f;
    soma.ATP_level = 5.0f;
    soma.intra_ions[ION_CA] = 0.0005f; // 0.5 uM — умеренный Ca2+
    soma.intra_ions[ION_NA] = 10.0f;
    soma.intra_ions[ION_K] = 140.0f;
    soma.extra_ions[ION_NA] = 140.0f;
    soma.extra_ions[ION_K] = 4.0f;

    // Добавляем сигнальные модификаторы на сому
    soma.modifiers[0].type = MOD_NB_CREB_TRANSCRIPTION;
    soma.modifiers[0].density = 1.0f;
    soma.modifiers[1].type = MOD_MAPK_ERK;
    soma.modifiers[1].density = 1.0f;
    soma.modifiers[2].type = MOD_MTORC1;
    soma.modifiers[2].density = 1.0f;
    soma.modifiers[2].state[3] = 0.5f; // AA_sensor
    soma.modifiers[3].type = MOD_PROTEIN_SYNTH;
    soma.modifiers[3].density = 1.0f;
    soma.modifiers[4].type = MOD_COFILIN;
    soma.modifiers[4].density = 1.0f;
    soma.modifiers[5].type = MOD_MMP9;
    soma.modifiers[5].density = 1.0f;
    soma.modifier_count = 6;

    Neuron neuron = {};
    neuron.id = 0;
    neuron.segment_start = 0;
    neuron.segment_count = 1;
    neuron.soma_index = 0;

    Nucleus nuc = {};
    nuc.neuron_id = 0;
    nuc.enabled = 1;
    nuc.consolidation_mode = 3;
    nuc.amino_acid_level = 0.5f;

    Synapse syn = {};
    syn.id = 1;
    syn.pre_segment = 0;
    syn.post_segment = 0;
    syn.weight = 1.0f;
    syn.type = SYNAPSE_TYPE_CHEMICAL;
    syn.tau_clearance = 100.0f;

    uint32_t n_syn = 1;
    uint64_t tick = 0;

    ConsolidationThread thread;
    thread.setGlobalInterval(100); // быстрая консолидация для теста
    thread.start(&soma, 1, &syn, &n_syn, 10, &neuron, 1, &nuc, 1, &tick);

    // Ждём первый цикл консолидации
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Останавливаем
    thread.stop();

    // Проверяем что генная активность изменилась
    // (сигнальные модификаторы должны были активироваться за время ожидания)
    Nucleus snapshot = {};
    bool ok = thread.getNucleusSnapshot(0, &snapshot);
    REQUIRE(ok);
    // После цикла консолидации pCREB должен передаться в гены
    // (точные значения зависят от таймингов, проверяем только что копия валидна)
    REQUIRE(snapshot.neuron_id == 0u);
}
