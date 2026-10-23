// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include "types.h"
#include "bion_loader.h"
#include <cstdio>
#include <cstring>

// Вспомогательная: создание минимального .bion файла
static void create_minimal_bion(const char* filename, uint32_t num_segs = 2,
                                 uint32_t num_conn = 1, uint32_t num_syn = 0,
                                 uint32_t num_neurons = 1) {
    NetworkHeader hdr = {};
    hdr.magic = BION_MAGIC;
    hdr.version = BION_VERSION;
    hdr.mode = MODE_BIOLOGICAL;
    hdr.num_neurons = num_neurons;
    hdr.num_segments = num_segs;
    hdr.num_intra_connections = num_conn;
    hdr.num_synapses = num_syn;
    hdr.timestamp = 1234567890;
    strncpy(hdr.build_id, "test", 16);

    FILE* f = fopen(filename, "wb");
    REQUIRE(f != nullptr);

    fwrite(&hdr, sizeof(NetworkHeader), 1, f);

    if (num_neurons > 0) {
        Neuron neuron = {0, 0, num_segs, 0, -40.0f, 0, {}};
        for (uint32_t i = 0; i < num_neurons; i++) {
            neuron.segment_start = 0;
            neuron.segment_count = num_segs;
            fwrite(&neuron, sizeof(Neuron), 1, f);
        }
    }

    for (uint32_t i = 0; i < num_segs; i++) {
        Segment seg = {};
        seg.id = i;
        seg.neuron_id = 0;
        seg.length = 100.0f;
        seg.radius = 10.0f;
        fwrite(&seg, sizeof(Segment), 1, f);
    }

    for (uint32_t i = 0; i < num_conn; i++) {
        IntraConnection conn = {0, 1, 0}; // 0 = не пересчитывать (EXTRA_ONLY)
        fwrite(&conn, sizeof(IntraConnection), 1, f);
    }

    for (uint32_t i = 0; i < num_syn; i++) {
        Synapse syn = {};
        syn.id = i;
        syn.pre_segment = 0;
        syn.post_segment = 1;
        syn.type = SYNAPSE_TYPE_CHEMICAL;
        fwrite(&syn, sizeof(Synapse), 1, f);
    }

    fclose(f);
}

// ═══════════════════════════════════════════════════════════════════
// Загрузка валидного файла (host-путь)
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("BION_HOST: Load valid file", "[bion_loader_host]") {
    create_minimal_bion("/tmp/test_bion_host_valid.bion");
    BionData data;
    int ret = bion_load_host("/tmp/test_bion_host_valid.bion", &data);
    REQUIRE(ret == 0);
    REQUIRE(data.header.num_segments == 2);
    REQUIRE(data.header.num_neurons == 1);
    REQUIRE(data.header.num_intra_connections == 1);
    REQUIRE(data.header.mode == MODE_BIOLOGICAL);
    REQUIRE(data.segments[0].length == 100.0f);
    REQUIRE(data.segments[0].radius == 10.0f);
    REQUIRE(data.connections[0].is_intracellular == 0); // 0 = bypass геометрического пересчёта
    bion_unload_host(&data);
    remove("/tmp/test_bion_host_valid.bion");
}

// ═══════════════════════════════════════════════════════════════════
// Несуществующий файл
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("BION_HOST: File not found", "[bion_loader_host]") {
    BionData data;
    int ret = bion_load_host("/tmp/__nonexistent_bion_file__.bion", &data);
    REQUIRE(ret == -1);
}

// ═══════════════════════════════════════════════════════════════════
// Неверная magic-сигнатура
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("BION_HOST: Bad magic", "[bion_loader_host]") {
    NetworkHeader hdr = {};
    hdr.magic = 0xDEADBEEF;
    hdr.version = BION_VERSION;
    hdr.num_segments = 1;
    FILE* f = fopen("/tmp/test_bion_host_bad_magic.bin", "wb");
    fwrite(&hdr, sizeof(NetworkHeader), 1, f);
    fclose(f);

    BionData data;
    int ret = bion_load_host("/tmp/test_bion_host_bad_magic.bin", &data);
    REQUIRE(ret == -2);
    remove("/tmp/test_bion_host_bad_magic.bin");
}

// ═══════════════════════════════════════════════════════════════════
// Неподдерживаемая версия
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("BION_HOST: Bad version", "[bion_loader_host]") {
    NetworkHeader hdr = {};
    hdr.magic = BION_MAGIC;
    hdr.version = 99;
    hdr.num_segments = 1;
    FILE* f = fopen("/tmp/test_bion_host_bad_ver.bin", "wb");
    fwrite(&hdr, sizeof(NetworkHeader), 1, f);
    fclose(f);

    BionData data;
    int ret = bion_load_host("/tmp/test_bion_host_bad_ver.bin", &data);
    REQUIRE(ret == -3);
    remove("/tmp/test_bion_host_bad_ver.bin");
}

// ═══════════════════════════════════════════════════════════════════
// Нулевое число сегментов
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("BION_HOST: Zero segments", "[bion_loader_host]") {
    NetworkHeader hdr = {};
    hdr.magic = BION_MAGIC;
    hdr.version = BION_VERSION;
    hdr.num_segments = 0;
    FILE* f = fopen("/tmp/test_bion_host_zero.bin", "wb");
    fwrite(&hdr, sizeof(NetworkHeader), 1, f);
    fclose(f);

    BionData data;
    int ret = bion_load_host("/tmp/test_bion_host_zero.bin", &data);
    REQUIRE(ret == -4);
    remove("/tmp/test_bion_host_zero.bin");
}

// ═══════════════════════════════════════════════════════════════════
// Невалидный индекс соединения
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("BION_HOST: Bad connection index", "[bion_loader_host]") {
    NetworkHeader hdr = {};
    hdr.magic = BION_MAGIC;
    hdr.version = BION_VERSION;
    hdr.num_neurons = 0;
    hdr.num_segments = 2;
    hdr.num_intra_connections = 1;

    Segment segs[2] = {};
    segs[0].id = 0; segs[1].id = 1;
    IntraConnection conn = {0, 999, 1};  // segment_b вне диапазона

    FILE* f = fopen("/tmp/test_bion_host_bad_conn.bin", "wb");
    fwrite(&hdr, sizeof(NetworkHeader), 1, f);
    fwrite(segs, sizeof(Segment), 2, f);
    fwrite(&conn, sizeof(IntraConnection), 1, f);
    fclose(f);

    BionData data;
    int ret = bion_load_host("/tmp/test_bion_host_bad_conn.bin", &data);
    REQUIRE(ret == -6);
    remove("/tmp/test_bion_host_bad_conn.bin");
}

// ═══════════════════════════════════════════════════════════════════
// Невалидный индекс синапса
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("BION_HOST: Bad synapse index", "[bion_loader_host]") {
    NetworkHeader hdr = {};
    hdr.magic = BION_MAGIC;
    hdr.version = BION_VERSION;
    hdr.num_neurons = 0;
    hdr.num_segments = 2;
    hdr.num_intra_connections = 0;
    hdr.num_synapses = 1;

    Segment segs[2] = {};
    segs[0].id = 0; segs[1].id = 1;
    Synapse syn = {};
    syn.pre_segment = 0;
    syn.post_segment = 999;  // вне диапазона
    syn.type = SYNAPSE_TYPE_CHEMICAL;

    FILE* f = fopen("/tmp/test_bion_host_bad_syn.bin", "wb");
    fwrite(&hdr, sizeof(NetworkHeader), 1, f);
    fwrite(segs, sizeof(Segment), 2, f);
    fwrite(&syn, sizeof(Synapse), 1, f);
    fclose(f);

    BionData data;
    int ret = bion_load_host("/tmp/test_bion_host_bad_syn.bin", &data);
    REQUIRE(ret == -7);
    remove("/tmp/test_bion_host_bad_syn.bin");
}

// ═══════════════════════════════════════════════════════════════════
// Невалидный диапазон нейрона
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("BION_HOST: Bad neuron range", "[bion_loader_host]") {
    NetworkHeader hdr = {};
    hdr.magic = BION_MAGIC;
    hdr.version = BION_VERSION;
    hdr.num_neurons = 1;
    hdr.num_segments = 2;

    Neuron neuron = {0, 0, 99,  // segment_count=99 выходит за num_segments=2
                     0, -40.0f, 0, {}};
    Segment segs[2] = {};
    segs[0].id = 0; segs[1].id = 1;

    FILE* f = fopen("/tmp/test_bion_host_bad_neuron.bin", "wb");
    fwrite(&hdr, sizeof(NetworkHeader), 1, f);
    fwrite(&neuron, sizeof(Neuron), 1, f);
    fwrite(segs, sizeof(Segment), 2, f);
    fclose(f);

    BionData data;
    int ret = bion_load_host("/tmp/test_bion_host_bad_neuron.bin", &data);
    REQUIRE(ret == -8);
    remove("/tmp/test_bion_host_bad_neuron.bin");
}

// ═══════════════════════════════════════════════════════════════════
// Слишком много модификаторов на сегменте
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("BION_HOST: Too many modifiers", "[bion_loader_host]") {
    NetworkHeader hdr = {};
    hdr.magic = BION_MAGIC;
    hdr.version = BION_VERSION;
    hdr.num_neurons = 0;
    hdr.num_segments = 1;

    Segment seg = {};
    seg.id = 0;
    seg.modifier_count = 99;  // > MAX_MODIFIERS_PER_SEGMENT (16)

    FILE* f = fopen("/tmp/test_bion_host_modcount.bin", "wb");
    fwrite(&hdr, sizeof(NetworkHeader), 1, f);
    fwrite(&seg, sizeof(Segment), 1, f);
    fclose(f);

    BionData data;
    int ret = bion_load_host("/tmp/test_bion_host_modcount.bin", &data);
    REQUIRE(ret == -9);
    remove("/tmp/test_bion_host_modcount.bin");
}

// ═══════════════════════════════════════════════════════════════════
// Слишком много рецепторов на синапсе
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("BION_HOST: Too many autoreceptors", "[bion_loader_host]") {
    NetworkHeader hdr = {};
    hdr.magic = BION_MAGIC;
    hdr.version = BION_VERSION;
    hdr.num_neurons = 0;
    hdr.num_segments = 2;
    hdr.num_synapses = 1;

    Segment segs[2] = {};
    segs[0].id = 0; segs[1].id = 1;
    Synapse syn = {};
    syn.pre_segment = 0;
    syn.post_segment = 1;
    syn.type = SYNAPSE_TYPE_CHEMICAL;
    syn.pre_autoreceptor_count = 99;  // > MAX_PRE_AUTORECEPTORS (8)

    FILE* f = fopen("/tmp/test_bion_host_autorec.bin", "wb");
    fwrite(&hdr, sizeof(NetworkHeader), 1, f);
    fwrite(segs, sizeof(Segment), 2, f);
    fwrite(&syn, sizeof(Synapse), 1, f);
    fclose(f);

    BionData data;
    int ret = bion_load_host("/tmp/test_bion_host_autorec.bin", &data);
    REQUIRE(ret == -9);
    remove("/tmp/test_bion_host_autorec.bin");
}

// ═══════════════════════════════════════════════════════════════════
// Невалидный тип синапса
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("BION_HOST: Invalid synapse type", "[bion_loader_host]") {
    NetworkHeader hdr = {};
    hdr.magic = BION_MAGIC;
    hdr.version = BION_VERSION;
    hdr.num_neurons = 0;
    hdr.num_segments = 2;
    hdr.num_synapses = 1;

    Segment segs[2] = {};
    segs[0].id = 0; segs[1].id = 1;
    Synapse syn = {};
    syn.pre_segment = 0;
    syn.post_segment = 1;
    syn.type = 99;  // > SYNAPSE_TYPE_COUNT (3)

    FILE* f = fopen("/tmp/test_bion_host_syn_type.bin", "wb");
    fwrite(&hdr, sizeof(NetworkHeader), 1, f);
    fwrite(segs, sizeof(Segment), 2, f);
    fwrite(&syn, sizeof(Synapse), 1, f);
    fclose(f);

    BionData data;
    int ret = bion_load_host("/tmp/test_bion_host_syn_type.bin", &data);
    REQUIRE(ret == -7);
    remove("/tmp/test_bion_host_syn_type.bin");
}

// ═══════════════════════════════════════════════════════════════════
// Save и reload через host-путь (roundtrip)
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("BION_HOST: Save and reload roundtrip", "[bion_loader_host]") {
    create_minimal_bion("/tmp/test_bion_host_rt_orig.bion");
    BionData data;
    int r1 = bion_load_host("/tmp/test_bion_host_rt_orig.bion", &data);
    REQUIRE(r1 == 0);

    int r2 = bion_save("/tmp/test_bion_host_rt_copy.bion", &data);
    REQUIRE(r2 == 0);

    BionData data2;
    int r3 = bion_load_host("/tmp/test_bion_host_rt_copy.bion", &data2);
    REQUIRE(r3 == 0);

    REQUIRE(data.header.num_segments == data2.header.num_segments);
    REQUIRE(data.segments[0].length == data2.segments[0].length);
    REQUIRE(data.connections[0].is_intracellular == data2.connections[0].is_intracellular);

    bion_unload_host(&data);
    bion_unload_host(&data2);
    remove("/tmp/test_bion_host_rt_orig.bion");
    remove("/tmp/test_bion_host_rt_copy.bion");
}

// ═══════════════════════════════════════════════════════════════════
// Save в невалидный путь
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("BION_HOST: Save to invalid path", "[bion_loader_host]") {
    BionData data = {};
    int ret = bion_save("/nonexistent_dir/should_fail.bion", &data);
    REQUIRE(ret == -1);  // BION_ERR_FILE_OPEN (fopen возвращает NULL)
}

// ═══════════════════════════════════════════════════════════════════
// bion_unload_host очищает указатели
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("BION_HOST: Unload clears pointers", "[bion_loader_host]") {
    create_minimal_bion("/tmp/test_bion_host_unload.bion");
    BionData data;
    int ret = bion_load_host("/tmp/test_bion_host_unload.bion", &data);
    REQUIRE(ret == 0);
    REQUIRE(data.segments != nullptr);

    bion_unload_host(&data);
    REQUIRE(data.segments == nullptr);
    REQUIRE(data.neurons == nullptr);
    REQUIRE(data.connections == nullptr);
    REQUIRE(data.synapses == nullptr);
    REQUIRE(data.num_segments == 0);
    REQUIRE(data.num_neurons == 0);

    remove("/tmp/test_bion_host_unload.bion");
}

// ═══════════════════════════════════════════════════════════════════
// intra_connection_ids заполняются корректно
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("BION_HOST: Intra connection IDs populated", "[bion_loader_host]") {
    create_minimal_bion("/tmp/test_bion_host_connids.bion", 3, 2, 0, 0);
    // 3 сегмента, 2 соединения: (0,1) и (0,1) — оба соединяют seg 0 и seg 1
    // Перезаписываем соединения
    {
        FILE* f = fopen("/tmp/test_bion_host_connids.bion", "rb");
        REQUIRE(f != nullptr);
        NetworkHeader hdr;
        fread(&hdr, sizeof(NetworkHeader), 1, f);

        // Читаем сегменты
        Segment segs[3];
        fread(segs, sizeof(Segment), 3, f);

        // Закрываем и переоткрываем на запись
        fclose(f);
        f = fopen("/tmp/test_bion_host_connids.bion", "wb");
        hdr.num_segments = 3;
        hdr.num_intra_connections = 2;
        fwrite(&hdr, sizeof(NetworkHeader), 1, f);

        segs[0].id = 0; segs[1].id = 1; segs[2].id = 2;
        fwrite(segs, sizeof(Segment), 3, f);

        IntraConnection conn1 = {0, 2, 1};  // seg 0 ↔ seg 2
        IntraConnection conn2 = {1, 2, 1};  // seg 1 ↔ seg 2
        fwrite(&conn1, sizeof(IntraConnection), 1, f);
        fwrite(&conn2, sizeof(IntraConnection), 1, f);
        fclose(f);
    }

    BionData data;
    int ret = bion_load_host("/tmp/test_bion_host_connids.bion", &data);
    REQUIRE(ret == 0);

    // seg 0 соединён с seg 2 через conn 0 → 1 связь
    REQUIRE(data.segments[0].intra_connection_count == 1);
    REQUIRE(data.segments[0].intra_connection_ids[0] == 0);

    // seg 1 соединён с seg 2 через conn 1 → 1 связь
    REQUIRE(data.segments[1].intra_connection_count == 1);
    REQUIRE(data.segments[1].intra_connection_ids[0] == 1);

    // seg 2 соединён с seg 0 (conn 0) и seg 1 (conn 1) → 2 связи
    REQUIRE(data.segments[2].intra_connection_count == 2);

    bion_unload_host(&data);
    remove("/tmp/test_bion_host_connids.bion");
}

// ═══════════════════════════════════════════════════════════════════
// Загрузка синапса с рецепторами (host-путь)
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("BION_HOST: Load synapse with receptors", "[bion_loader_host]") {
    NetworkHeader hdr = {};
    hdr.magic = BION_MAGIC;
    hdr.version = BION_VERSION;
    hdr.num_neurons = 0;
    hdr.num_segments = 2;
    hdr.num_synapses = 1;

    Segment segs[2] = {};
    segs[0].id = 0; segs[0].length = 100; segs[0].radius = 10;
    segs[1].id = 1; segs[1].length = 100; segs[1].radius = 10;

    Synapse syn = {};
    syn.id = 0;
    syn.pre_segment = 0;
    syn.post_segment = 1;
    syn.type = SYNAPSE_TYPE_CHEMICAL;
    syn.pre_autoreceptors[0] = {MOD_NT_RECEPTOR_5HT1A, {0,0,0}, 0.3f, {}, 0, 0, {0,0}};
    syn.pre_autoreceptor_count = 1;
    syn.post_receptors[0] = {MOD_NT_RECEPTOR_AMPA, {0,0,0}, 0.5f, {}, 0, 0, {0,0}};
    syn.post_receptors[1] = {MOD_NT_RECEPTOR_NMDA, {0,0,0}, 0.2f, {}, 0, 0, {0,0}};
    syn.post_receptor_count = 2;

    FILE* f = fopen("/tmp/test_bion_host_syn.bin", "wb");
    fwrite(&hdr, sizeof(NetworkHeader), 1, f);
    fwrite(segs, sizeof(Segment), 2, f);
    fwrite(&syn, sizeof(Synapse), 1, f);
    fclose(f);

    BionData data;
    int ret = bion_load_host("/tmp/test_bion_host_syn.bin", &data);
    REQUIRE(ret == 0);
    REQUIRE(data.synapses[0].pre_autoreceptor_count == 1);
    REQUIRE(data.synapses[0].pre_autoreceptors[0].type == MOD_NT_RECEPTOR_5HT1A);
    REQUIRE(data.synapses[0].post_receptor_count == 2);
    REQUIRE(data.synapses[0].post_receptors[0].type == MOD_NT_RECEPTOR_AMPA);
    REQUIRE(data.synapses[0].post_receptors[1].type == MOD_NT_RECEPTOR_NMDA);

    bion_unload_host(&data);
    remove("/tmp/test_bion_host_syn.bin");
}

// ═══════════════════════════════════════════════════════════════════
// Структуры bion_format.h — проверка размеров
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("BION_HOST: Struct sizes", "[bion_loader_host]") {
    REQUIRE(sizeof(NetworkHeader) == 100u);  // v10: +file_sz fields
    REQUIRE(sizeof(Neuron) == 24u);
    REQUIRE(sizeof(Nucleus) == 500u);
    REQUIRE(sizeof(Segment) == 3216);  // v14: I_buffer 8-aligned
    REQUIRE(sizeof(Modifier) == 88);
    REQUIRE(sizeof(IntraConnection) == 16u);
    REQUIRE(sizeof(Synapse) == 1568);  // v14: post_receptors 8-aligned
    REQUIRE(sizeof(ConsolidationEvent) == 36u);
}

// ═══════════════════════════════════════════════════════════════════
// Константы BION
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("BION_HOST: Constants", "[bion_loader_host]") {
    REQUIRE(BION_MAGIC == 0x42494F4Eu);
    REQUIRE(BION_VERSION == 14u);
    REQUIRE(BION_VERSION_MIN == 3u);
    REQUIRE(BION_VERSION_MAX == 14u);
    REQUIRE(MODE_BIOLOGICAL == 0u);
    REQUIRE(MODE_ARBITRARY == 1u);
    REQUIRE(SYNAPSE_TYPE_CHEMICAL == 0u);
    REQUIRE(SYNAPSE_TYPE_ELECTRICAL == 1u);
    REQUIRE(SYNAPSE_TYPE_DENDRITIC_BRIDGE == 2u);
    REQUIRE(MAX_MODIFIERS_PER_SEGMENT == 32u);
    REQUIRE(MAX_PRE_AUTORECEPTORS == 8u);
    REQUIRE(MAX_POST_RECEPTORS == 8u);
}

// ═══════════════════════════════════════════════════════════════════
// Данные data.header после успешной загрузки
// ═══════════════════════════════════════════════════════════════════
TEST_CASE("BION_HOST: Header reflects file contents", "[bion_loader_host]") {
    create_minimal_bion("/tmp/test_bion_host_hdr.bion", 5, 2, 1, 1);
    BionData data;
    int ret = bion_load_host("/tmp/test_bion_host_hdr.bion", &data);
    REQUIRE(ret == 0);
    REQUIRE(data.num_segments == 5);
    REQUIRE(data.num_connections == 2);
    REQUIRE(data.num_synapses == 1);
    REQUIRE(data.num_neurons == 1);
    REQUIRE(data.header.magic == BION_MAGIC);
    REQUIRE(data.header.version == BION_VERSION);
    bion_unload_host(&data);
    remove("/tmp/test_bion_host_hdr.bion");
}

// ═══════════════════════════════════════════════════════════════════
// MAX_MODIFIERS_PER_SEGMENT: >16 modifier roundtrip (BION v8)
// ═══════════════════════════════════════════════════════════════════

// Хелпер: создаём .bion с сегментом, у которого N модификаторов
static void create_bion_n_mods(const char* filename, int n_mods) {
    NetworkHeader hdr = {};
    hdr.magic = BION_MAGIC;
    hdr.version = BION_VERSION;
    hdr.mode = MODE_BIOLOGICAL;
    hdr.num_neurons = 1;
    hdr.num_segments = 1;
    hdr.num_intra_connections = 0;
    hdr.num_synapses = 0;
    hdr.timestamp = 1234567890;
    strncpy(hdr.build_id, "test-nmods", 16);

    FILE* f = fopen(filename, "wb");
    REQUIRE(f != nullptr);
    fwrite(&hdr, sizeof(NetworkHeader), 1, f);

    Neuron neuron = {0, 0, 1, 0, -40.0f, 0, {}};
    fwrite(&neuron, sizeof(Neuron), 1, f);

    Segment seg = {};
    seg.id = 0;
    seg.neuron_id = 0;
    seg.length = 100.0f; seg.radius = 10.0f; seg.V_m = -65.0f;
    seg.modifier_count = (uint8_t)n_mods;
    // Заполняем модификаторы: тип = MOD_K2P_LEAK + i, density = 0.1f * (i+1)
    for (int i = 0; i < n_mods && i < MAX_MODIFIERS_PER_SEGMENT; i++) {
        seg.modifiers[i].type = (uint8_t)(MOD_K2P_LEAK + i);
        seg.modifiers[i].density = 0.1f * (i + 1);
    }
    fwrite(&seg, sizeof(Segment), 1, f);
    fclose(f);
}

TEST_CASE("BION_HOST: 20 modifiers roundtrip works", "[bion_loader_host][modifiers]") {
    constexpr int N_MODS = 20;
    REQUIRE(MAX_MODIFIERS_PER_SEGMENT >= N_MODS);
    create_bion_n_mods("/tmp/test_bion_nmods.bion", N_MODS);

    BionData data;
    int ret = bion_load_host("/tmp/test_bion_nmods.bion", &data);
    REQUIRE(ret == 0);
    REQUIRE(data.num_segments == 1);

    Segment* seg = &data.segments[0];
    // V_m и геометрия сохранены
    CHECK(seg->V_m == -65.0f);
    // Все 20 модификаторов на месте
    CHECK(seg->modifier_count == N_MODS);
    for (int i = 0; i < N_MODS; i++) {
        INFO("Modifier " << i);
        CHECK(seg->modifiers[i].type == (uint8_t)(MOD_K2P_LEAK + i));
        CHECK(seg->modifiers[i].density == 0.1f * (i + 1));
    }

    bion_unload_host(&data);
    remove("/tmp/test_bion_nmods.bion");
}

TEST_CASE("BION_HOST: MAX_MODIFIERS_PER_SEGMENT is at least 32", "[bion_loader_host][modifiers]") {
    // BION v8: увеличен с 16 до 32
    // dIN имеет 17 модификаторов — должно помещаться с запасом
    CHECK(MAX_MODIFIERS_PER_SEGMENT >= 32);
}
