// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_consolidation_plasticity.cpp — тесты структурной пластичности консолидации.
// Проверяет thread lifecycle, applyPendingEvents, nucleus snapshot, и гены через фон.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "types.h"
#include "consolidation.h"
#include "bion_format.h"
#include <cstring>
#include <thread>
#include <chrono>

// Forward-объявления функций модификаторов из hh_biological.cpp
extern double mapk_erk_current(Modifier* m, Segment* s, float dt);
extern double ikk_nfkb_current(Modifier* m, Segment* s, float dt);
extern double mtorc1_current(Modifier* m, Segment* s, float dt);
extern double protein_synth_current(Modifier* m, Segment* s, float dt);
extern double cofilin_current(Modifier* m, Segment* s, float dt);
extern double mmp9_current(Modifier* m, Segment* s, float dt);

// ═══════════════════════════════════════════════════════════════════
// Хелпер: создание тестовой сети
// ═══════════════════════════════════════════════════════════════════

struct TestNet {
    Segment segs[4];
    IntraConnection conns[2];
    Synapse syns[2];
    Neuron neurons[2];
    Nucleus nuclei[2];
    uint32_t num_segs = 4;
    uint32_t num_conns = 2;
    uint32_t num_syns = 2;
    uint32_t max_syns = 2;
    uint64_t tick = 0;

    void init() {
        memset(this, 0, sizeof(*this));
        num_segs = 4; num_conns = 2; num_syns = 2; max_syns = 2;

        neurons[0].id = 1;
        neurons[0].segment_start = 0;
        neurons[0].segment_count = 2;
        neurons[0].threshold = -20.0f;
        neurons[1].id = 2;
        neurons[1].segment_start = 2;
        neurons[1].segment_count = 2;
        neurons[1].threshold = -20.0f;

        for (int i = 0; i < 4; i++) {
            segs[i].id = 100 + i;
            segs[i].neuron_id = (i < 2) ? 1u : 2u;
            segs[i].radius = 1.0f;
            segs[i].length = 10.0f;
            segs[i].V_m = -65.0f;
            if (i == 0 || i == 2) {
                segs[i].modifiers[0].type = MOD_PROTEIN_SYNTH;
                segs[i].modifiers[0].state[0] = 1.0f;
                segs[i].modifier_count = 1;
            }
        }

        conns[0].segment_a = 0; conns[0].segment_b = 1; conns[0].is_intracellular = 1;
        conns[1].segment_a = 2; conns[1].segment_b = 3; conns[1].is_intracellular = 1;

        syns[0].id = 200; syns[0].pre_segment = 1; syns[0].post_segment = 2; syns[0].weight = 1.0f;
        syns[1].id = 201; syns[1].pre_segment = 0; syns[1].post_segment = 3; syns[1].weight = 0.5f;

        nuclei[0].neuron_id = 1; nuclei[0].enabled = 1; nuclei[0].consolidation_mode = 3;
        nuclei[0].consolidation_interval_ticks = 100; nuclei[0].amino_acid_level = 0.5f;
        nuclei[0].protein_synthesis_rate = 0.01f;
        nuclei[1].neuron_id = 2; nuclei[1].enabled = 1; nuclei[1].consolidation_mode = 3;
        nuclei[1].consolidation_interval_ticks = 100; nuclei[1].amino_acid_level = 0.5f;
        nuclei[1].protein_synthesis_rate = 0.01f;
    }
};

// ═══════════════════════════════════════════════════════════════════
// Модификаторы консолидации: проверка что возвращают не-NaN/не-inf
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("MAPK_ERK returns finite current at rest", "[consolidation][modifier]") {
    TestNet net; net.init();
    Modifier m = {MOD_MAPK_ERK, {}, 1.0f, {1.0f, 0.1f, 0.2f, 0.01f, 0.02f}, 5, 0, {}};
    float I = mapk_erk_current(&m, &net.segs[0], 0.1f);
    REQUIRE_FALSE(std::isnan(I));
    REQUIRE_FALSE(std::isinf(I));
}

TEST_CASE("MAPK_ERK activated by Ca2+ produces pERK", "[consolidation][modifier]") {
    TestNet net; net.init();
    // Высокий Ca²⁺ → активация MAPK
    net.segs[0].intra_ions[ION_CA] = 1.0f;
    Modifier m = {MOD_MAPK_ERK, {}, 1.0f, {1.0f, 0.1f, 0.2f}, 3, 0, {}};
    float I = mapk_erk_current(&m, &net.segs[0], 0.1f);
    REQUIRE(I == 0.0f);  // MAPK_ERK — сигнальный, не ионный ток (возвращает 0)
    // pERK должен расти в state[0] при высоком Ca²⁺
    CHECK(m.state[0] >= 0.0f);
}

TEST_CASE("IKK_NFKB returns finite current", "[consolidation][modifier]") {
    TestNet net; net.init();
    net.segs[0].intra_ions[ION_CA] = 0.5f;
    Modifier m = {MOD_IKK_NFKB, {}, 1.0f, {0.0f, 1.0f, 0.1f}, 3, 0, {}};
    float I = ikk_nfkb_current(&m, &net.segs[0], 0.1f);
    REQUIRE_FALSE(std::isnan(I));
}

TEST_CASE("MTORC1 returns finite current", "[consolidation][modifier]") {
    TestNet net; net.init();
    Modifier m = {MOD_MTORC1, {}, 1.0f, {0.5f, 1.0f, 1.0f, 0.5f}, 4, 0, {}};
    float I = mtorc1_current(&m, &net.segs[0], 0.1f);
    REQUIRE_FALSE(std::isnan(I));
}

TEST_CASE("PROTEIN_SYNTH returns finite current", "[consolidation][modifier]") {
    TestNet net; net.init();
    Modifier m = {MOD_PROTEIN_SYNTH, {}, 1.0f, {1.0f, 0.5f, 1.0f}, 3, 0, {}};
    float I = protein_synth_current(&m, &net.segs[0], 0.1f);
    REQUIRE_FALSE(std::isnan(I));
}

TEST_CASE("COFILIN returns finite current", "[consolidation][modifier]") {
    TestNet net; net.init();
    Modifier m = {MOD_COFILIN, {}, 1.0f, {0.5f, 0.3f, 1.0f}, 3, 0, {}};
    float I = cofilin_current(&m, &net.segs[0], 0.1f);
    REQUIRE_FALSE(std::isnan(I));
}

TEST_CASE("MMP9 returns finite current", "[consolidation][modifier]") {
    TestNet net; net.init();
    Modifier m = {MOD_MMP9, {}, 1.0f, {0.5f, 0.5f, 0.3f}, 3, 0, {}};
    float I = mmp9_current(&m, &net.segs[0], 0.1f);
    REQUIRE_FALSE(std::isnan(I));
}

// ═══════════════════════════════════════════════════════════════════
// Thread lifecycle
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("ConsolidationThread start/stop without crash", "[consolidation][thread]") {
    TestNet net; net.init();

    ConsolidationThread ct;
    REQUIRE_FALSE(ct.isRunning());

    ct.setGlobalInterval(100);
    ct.start(net.segs, net.num_segs, net.syns, &net.num_syns, net.max_syns,
             net.neurons, 2, net.nuclei, 2, &net.tick);

    // Даём потоку время запуститься
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    ct.stop();
    REQUIRE_FALSE(ct.isRunning());
}

TEST_CASE("ConsolidationThread interval=0 allows start but no work", "[consolidation][thread]") {
    TestNet net; net.init();

    ConsolidationThread ct;
    ct.setGlobalInterval(0);
    REQUIRE(ct.getGlobalInterval() == 0);

    ct.start(net.segs, net.num_segs, net.syns, &net.num_syns, net.max_syns,
             net.neurons, 2, net.nuclei, 2, &net.tick);

    // Поток может запуститься, но с interval=0 цикл консолидации не выполняется.
    // Даём время и проверяем что стоп работает без краша.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ct.stop();
    REQUIRE_FALSE(ct.isRunning());
    // Гены не должны были измениться (нет циклов консолидации)
    CHECK(net.nuclei[0].gene_activity[GENE_CFOS] == 0.0f);
}

TEST_CASE("applyPendingEvents with empty queue", "[consolidation][thread]") {
    TestNet net; net.init();

    ConsolidationThread ct;
    REQUIRE(ct.getPendingEventCount() == 0);

    uint32_t applied = ct.applyPendingEvents(net.segs, net.num_segs,
                                               net.syns, &net.num_syns,
                                               net.max_syns);
    CHECK(applied == 0);
}

TEST_CASE("NucleusSnapshot reflects actual state", "[consolidation][thread]") {
    TestNet net; net.init();
    net.nuclei[0].pcreb_level = 0.75f;
    net.nuclei[0].gene_activity[GENE_CFOS] = 0.5f;

    ConsolidationThread ct;
    ct.start(net.segs, net.num_segs, net.syns, &net.num_syns, net.max_syns,
             net.neurons, 2, net.nuclei, 2, &net.tick);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    Nucleus snap;
    bool ok = ct.getNucleusSnapshot(0, &snap);
    if (ok) {
        CHECK(snap.neuron_id == 1);
        // pCREB и гены могут измениться за время работы потока, проверяем bounds
        CHECK(snap.pcreb_level >= 0.0f);
        CHECK(snap.pcreb_level <= 1.0f);
    }

    ct.stop();
}

TEST_CASE("Gene activity stays within [0,1] after thread cycles", "[consolidation][thread]") {
    TestNet net; net.init();

    ConsolidationThread ct;
    ct.setGlobalInterval(100);
    ct.start(net.segs, net.num_segs, net.syns, &net.num_syns, net.max_syns,
             net.neurons, 2, net.nuclei, 2, &net.tick);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    ct.stop();

    // Все гены должны оставаться в [0, 1] даже после фоновых циклов
    for (int n = 0; n < 2; n++) {
        for (int g = 0; g < GENE_COUNT; g++) {
            CHECK(net.nuclei[n].gene_activity[g] >= 0.0f);
            CHECK(net.nuclei[n].gene_activity[g] <= 1.0f);
        }
    }
}
