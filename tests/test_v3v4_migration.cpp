// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_v3v4_migration.cpp — тесты обратной совместимости .bion форматов.
// Проверяет v3→v4→v5 миграцию при загрузке старых файлов.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "types.h"
#include "bion_loader.h"
#include "bion_format.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

// ═══════════════════════════════════════════════════════════════════
// Хелперы для записи v3/v4 файлов
// ═══════════════════════════════════════════════════════════════════

// v3 Segment (без direction, segment_type, intra_nt, Mg²⁺)
#pragma pack(push, 1)
struct V3Modifier {
    uint8_t  type;
    float    density;
    float    state[8];
    uint8_t  active_vars;
    uint8_t  flags;
    uint8_t  _reserved[2];
};

struct V3Segment {
    uint32_t id;
    uint32_t neuron_id;
    Vec3     position;
    float    length;
    float    radius;
    float    V_m;
    float    intra_ions[4];
    float    extra_ions[4];
    float    neurotransmitter[12];
    V3Modifier modifiers[16];
    uint8_t  modifier_count;
    uint8_t  _reserved[3];
    float    ATP_level;
    float    temperature;
    float    I_buffer;
    uint32_t intra_connection_ids[16];
    uint8_t  intra_connection_count;
    uint8_t  _reserved2[3];
};

// v3 header (68 байт, без num_nuclei/nucleus_version/_reserved_hdr)
struct V3Header {
    uint32_t magic;
    uint32_t version;
    uint8_t  mode;
    uint8_t  diffusion_type;
    uint16_t reserved;
    uint32_t num_neurons;
    uint32_t num_segments;
    uint32_t num_intra_connections;
    uint32_t num_synapses;
    uint32_t grid_dim_x;
    uint32_t grid_dim_y;
    uint32_t grid_dim_z;
    float    voxel_size;
    uint64_t timestamp;
    char     build_id[16];
};
#pragma pack(pop)

// v4 Segment (v4-v6: MAX_MODIFIERS=16, sizeof=988) — зеркало bion_loader_host.cpp
#pragma pack(push, 1)
struct V4Segment {
    uint32_t id; uint32_t neuron_id;
    Vec3 position; Vec3 direction;
    float length; float radius; float V_m;
    float intra_ions[5]; float extra_ions[5];
    float intra_nt[14]; float neurotransmitter[14];
    Modifier modifiers[16];
    uint8_t modifier_count; uint8_t segment_type; uint8_t _reserved1[2];
    float ATP_level; float temperature; float I_buffer; float transcription_signal;
    uint32_t intra_connection_ids[16]; uint8_t intra_connection_count; uint8_t _reserved2[3];
};
#pragma pack(pop)

// v8 IntraConnection (12 байт, без _pad) — зеркало bion_loader_host.cpp OldIntraConnection
struct OldIntraConnection {
    uint32_t segment_a;
    uint32_t segment_b;
    float    coupling_resistance;
};

// v6 Synapse (824 байта, без RRP полей)
struct SynapseV6 { uint8_t raw[824]; };

static void write_v3_test_file(const char* path) {
    FILE* f = fopen(path, "wb");
    REQUIRE(f != nullptr);

    // Заголовок v3 (68 байт)
    V3Header hdr = {};
    hdr.magic = BION_MAGIC;
    hdr.version = 3;
    hdr.mode = 0;
    hdr.num_neurons = 1;
    hdr.num_segments = 2;
    hdr.num_intra_connections = 1;
    hdr.num_synapses = 1;
    hdr.grid_dim_x = 1; hdr.grid_dim_y = 1; hdr.grid_dim_z = 1;
    hdr.voxel_size = 1.0f;
    hdr.timestamp = 12345;
    memset(hdr.build_id, 0, 16);
    fwrite(&hdr, sizeof(V3Header), 1, f);

    // Нейроны
    Neuron neurons[1] = {};
    neurons[0].id = 1;
    neurons[0].segment_start = 0;
    neurons[0].segment_count = 2;
    neurons[0].soma_index = 0;
    neurons[0].threshold = -20.0f;
    neurons[0].type = 0;
    fwrite(neurons, sizeof(Neuron), 1, f);

    // v3 сегменты с узнаваемыми значениями
    V3Segment segs[2] = {};
    for (int i = 0; i < 2; i++) {
        segs[i].id = 100 + i;
        segs[i].neuron_id = 1;
        segs[i].position = {float(i * 20), 0.0f, 0.0f};
        segs[i].length = 15.0f;
        segs[i].radius = 2.5f;
        segs[i].V_m = -65.0f;
        // 4 иона (Mg²⁺ нет в v3)
        segs[i].intra_ions[0] = 10.0f;  // Na
        segs[i].intra_ions[1] = 140.0f; // K
        segs[i].intra_ions[2] = 0.0001f;// Ca
        segs[i].intra_ions[3] = 5.0f;   // Cl
        segs[i].extra_ions[0] = 140.0f;
        segs[i].extra_ions[1] = 4.0f;
        segs[i].extra_ions[2] = 2.0f;
        segs[i].extra_ions[3] = 120.0f;
        // 12 нейромедиаторов (NT_MAGNESIUM=12, NT_NICOTINE=13 нет в v3)
        for (int j = 0; j < 12; j++) segs[i].neurotransmitter[j] = 0.0f;
        // Модификатор
        segs[i].modifiers[0].type = 142;  // MOD_AXON_NAV12 (Nav1.2) — вместо удалённого IONOTROPIC_NAK(0)
        segs[i].modifiers[0].density = 0.8f;
        for (int k = 0; k < 8; k++) segs[i].modifiers[0].state[k] = float(k + 1);
        segs[i].modifiers[0].active_vars = 3;
        segs[i].modifiers[0].flags = 0;
        segs[i].modifier_count = 1;
        segs[i].ATP_level = 1.0f;
        segs[i].temperature = 310.15f;
        segs[i].I_buffer = -0.5f;
        segs[i].intra_connection_count = (i == 0) ? 1 : 0;
        if (i == 0) segs[i].intra_connection_ids[0] = 1;
    }
    fwrite(segs, sizeof(V3Segment), 2, f);

    // IntraConnection (v3-v8: 12 байт, без _pad; segment_a/segment_b — индексы)
    OldIntraConnection conn = {0, 1, 5.0f};
    fwrite(&conn, sizeof(OldIntraConnection), 1, f);

    // Synapse (v3-v6: 824 байта, без RRP полей; pre_segment/post_segment — индексы)
    SynapseV6 syn6 = {};
    Synapse syn = {};
    syn.id = 200;
    syn.pre_segment = 0;
    syn.post_segment = 1;
    syn.weight = 1.0f;
    syn.type = 0;
    memcpy(syn6.raw, &syn, 824);  // первые 824 байта v6==v7 (RRP добавлены в конец)
    fwrite(&syn6, sizeof(SynapseV6), 1, f);

    fclose(f);
}

static void write_v4_test_file(const char* path) {
    // v4: header 68 байт + v4 Segments (988 байт)
    FILE* f = fopen(path, "wb");
    REQUIRE(f != nullptr);

    V3Header hdr = {};
    hdr.magic = BION_MAGIC;
    hdr.version = 4;
    hdr.num_neurons = 1;
    hdr.num_segments = 1;
    hdr.num_intra_connections = 0;
    hdr.num_synapses = 0;
    hdr.grid_dim_x = 1; hdr.grid_dim_y = 1; hdr.grid_dim_z = 1;
    hdr.voxel_size = 1.0f;
    hdr.timestamp = 12345;
    fwrite(&hdr, sizeof(V3Header), 1, f);

    Neuron n = {};
    n.id = 1;
    n.segment_start = 0;
    n.segment_count = 1;
    n.soma_index = 0;
    n.threshold = -20.0f;
    fwrite(&n, sizeof(Neuron), 1, f);

    V4Segment seg = {};
    seg.id = 1;
    seg.neuron_id = 1;
    seg.position = {0, 0, 0};
    seg.direction = {1, 0, 0};
    seg.length = 10.0f;
    seg.radius = 3.0f;
    seg.V_m = -60.0f;
    seg.segment_type = 1;  // SOMA
    fwrite(&seg, sizeof(V4Segment), 1, f);

    fclose(f);
}

// ═══════════════════════════════════════════════════════════════════
// Тесты
// ═══════════════════════════════════════════════════════════════════

TEST_CASE("v3 segment struct size matches expectation", "[v3v4]") {
    // V3Segment должен иметь известный размер для корректной загрузки
    size_t mod_sz = sizeof(V3Modifier);
    size_t seg_sz = sizeof(V3Segment);
    REQUIRE(mod_sz == 41);
    // V3Segment = 4+4+12+4+4+4+16+16+48+(16*41)+1+3+4+4+4+64+1+3 = 852
    REQUIRE(seg_sz == 852);
}

TEST_CASE("v3 header size is 68 bytes", "[v3v4]") {
    REQUIRE(sizeof(V3Header) == 68);
}

TEST_CASE("v5 header size is 84 bytes", "[v3v4]") {
    REQUIRE(sizeof(NetworkHeader) == 100);  // v10: +file_sz_* fields
}

TEST_CASE("v3 file loads and migrates to v5", "[v3v4]") {
    const char* path = "/tmp/test_v3_migration.bion";
    write_v3_test_file(path);

    BionData data;
    int rc = bion_load_host(path, &data);
    REQUIRE(rc == 0);

    // Проверяем мигрированные сегменты
    REQUIRE(data.num_segments == 2);
    REQUIRE(data.num_neurons == 1);
    REQUIRE(data.num_connections == 1);
    REQUIRE(data.num_synapses == 1);

    // Сегмент 0: поля идентичности
    Segment* s = &data.segments[0];
    REQUIRE(s->id == 100);
    REQUIRE(s->neuron_id == 1);
    REQUIRE(s->position.x == 0.0f);
    REQUIRE(s->length == 15.0f);
    REQUIRE(s->radius == 2.5f);
    REQUIRE(s->V_m == -65.0f);

    // v4-добавленное: direction по умолчанию
    REQUIRE(s->direction.x == 0.0f);
    REQUIRE(s->direction.y == 1.0f);
    REQUIRE(s->direction.z == 0.0f);

    // v4-добавленное: segment_type по умолчанию
    REQUIRE(s->segment_type == 0);  // DENDRITE по умолчанию

    // Ионы: v3 имел 4 → v5 имеет 5, Mg²⁺=0
    REQUIRE(s->intra_ions[0] == 10.0f);
    REQUIRE(s->intra_ions[1] == 140.0f);
    REQUIRE(s->intra_ions[2] == 0.0001f);
    REQUIRE(s->intra_ions[3] == 5.0f);
    REQUIRE(s->intra_ions[4] == 0.0f);  // Mg²⁺ не было в v3

    REQUIRE(s->extra_ions[0] == 140.0f);
    REQUIRE(s->extra_ions[1] == 4.0f);
    REQUIRE(s->extra_ions[2] == 2.0f);
    REQUIRE(s->extra_ions[3] == 120.0f);
    REQUIRE(s->extra_ions[4] == 0.0f);

    // Нейромедиаторы: v3 имел 12 → v5 имеет 14
    for (int j = 0; j < 12; j++) REQUIRE(s->neurotransmitter[j] == 0.0f);
    REQUIRE(s->neurotransmitter[12] == 0.0f);  // NT_MAGNESIUM
    REQUIRE(s->neurotransmitter[13] == 0.0f);  // NT_NICOTINE

    // intra_nt: v3 не имел → v5 нули
    for (int j = 0; j < NT_COUNT; j++) REQUIRE(s->intra_nt[j] == 0.0f);

    // Модификатор мигрирован
    REQUIRE(s->modifier_count == 1);
    REQUIRE(s->modifiers[0].type == 142);
    REQUIRE(s->modifiers[0].density == 0.8f);
    for (int k = 0; k < 8; k++)
        REQUIRE(s->modifiers[0].state[k] == float(k + 1));
    REQUIRE(s->modifiers[0].active_vars == 3);
    REQUIRE(s->modifiers[0].flags == 0);

    // ATP/temperature/I_buffer
    REQUIRE(s->ATP_level == 1.0f);
    REQUIRE(s->temperature == 310.15f);
    REQUIRE(s->I_buffer == -0.5f);

    // IntraConnection IDs (перестраиваются загрузчиком из connections[] массива)
    REQUIRE(s->intra_connection_count == 1);
    CHECK(s->intra_connection_ids[0] == 0);

    // Проверяем Connections
    REQUIRE(data.connections[0].segment_a == 0);
    REQUIRE(data.connections[0].segment_b == 1);

    // Сегмент 1: тоже имеет ссылку на connection (связь двусторонняя)
    Segment* s1 = &data.segments[1];
    REQUIRE(s1->intra_connection_count == 1);

    // v5: default Nucleus создан
    REQUIRE(data.num_nuclei == 1);
    REQUIRE(data.nuclei != nullptr);
    REQUIRE(data.nuclei[0].neuron_id == 1);
    REQUIRE(data.nuclei[0].enabled == 1);
    REQUIRE(data.nuclei[0].consolidation_mode == 3);
    REQUIRE(data.nuclei[0].amino_acid_level == 0.5f);

    bion_unload_host(&data);
    remove(path);
}

TEST_CASE("v4 file loads and creates default nuclei", "[v3v4]") {
    const char* path = "/tmp/test_v4_migration.bion";
    write_v4_test_file(path);

    BionData data;
    int rc = bion_load_host(path, &data);
    REQUIRE(rc == 0);

    REQUIRE(data.num_segments == 1);
    REQUIRE(data.num_neurons == 1);

    // v4 Segment загружается напрямую (без миграции)
    Segment* s = &data.segments[0];
    REQUIRE(s->id == 1);
    REQUIRE(s->direction.x == 1.0f);   // v4: direction сохранён
    REQUIRE(s->direction.y == 0.0f);
    REQUIRE(s->segment_type == 1);      // v4: SOMA сохранён

    // v5: default Nucleus (header.version=4 < 5)
    REQUIRE(data.num_nuclei == 1);
    REQUIRE(data.nuclei[0].enabled == 1);
    REQUIRE(data.nuclei[0].amino_acid_level == 0.5f);

    bion_unload_host(&data);
    remove(path);
}

TEST_CASE("v3 migration zeroes intra_nt array", "[v3v4]") {
    const char* path = "/tmp/test_v3_intrant.bion";
    write_v3_test_file(path);

    BionData data;
    bion_load_host(path, &data);
    REQUIRE(data.num_segments == 2);

    // intra_nt должно быть полностью занулено после миграции
    for (uint32_t i = 0; i < data.num_segments; i++) {
        for (int j = 0; j < NT_COUNT; j++) {
            REQUIRE(data.segments[i].intra_nt[j] == 0.0f);
        }
    }

    bion_unload_host(&data);
    remove(path);
}

TEST_CASE("v3 migration preserves transcription_signal as zero", "[v3v4]") {
    const char* path = "/tmp/test_v3_transcription.bion";
    write_v3_test_file(path);

    BionData data;
    bion_load_host(path, &data);

    // transcription_signal — новое v5-поле, должно быть 0 после миграции
    for (uint32_t i = 0; i < data.num_segments; i++) {
        REQUIRE(data.segments[i].transcription_signal == 0.0f);
    }

    bion_unload_host(&data);
    remove(path);
}

TEST_CASE("bion_load_host rejects unsupported version", "[v3v4]") {
    const char* path = "/tmp/test_v99.bion";
    FILE* f = fopen(path, "wb");
    REQUIRE(f != nullptr);

    V3Header hdr = {};
    hdr.magic = BION_MAGIC;
    hdr.version = 99;
    hdr.num_segments = 1;
    fwrite(&hdr, sizeof(V3Header), 1, f);
    fclose(f);

    BionData data;
    int rc = bion_load_host(path, &data);
    REQUIRE(rc < 0);  // Должен отвергнуть

    remove(path);
}

TEST_CASE("bion_load_host rejects bad magic", "[v3v4]") {
    const char* path = "/tmp/test_badmagic.bion";
    FILE* f = fopen(path, "wb");
    REQUIRE(f != nullptr);

    V3Header hdr = {};
    hdr.magic = 0xDEADBEEF;
    hdr.version = 3;
    hdr.num_segments = 1;
    fwrite(&hdr, sizeof(V3Header), 1, f);
    fclose(f);

    BionData data;
    int rc = bion_load_host(path, &data);
    REQUIRE(rc < 0);

    remove(path);
}

TEST_CASE("bion_load_host rejects zero segments", "[v3v4]") {
    const char* path = "/tmp/test_zerosegs.bion";
    FILE* f = fopen(path, "wb");
    REQUIRE(f != nullptr);

    V3Header hdr = {};
    hdr.magic = BION_MAGIC;
    hdr.version = 3;
    hdr.num_segments = 0;  // ноль сегментов
    fwrite(&hdr, sizeof(V3Header), 1, f);
    fclose(f);

    BionData data;
    int rc = bion_load_host(path, &data);
    REQUIRE(rc < 0);

    remove(path);
}

// ═══════════════════════════════════════════════════════════════════
// v10 size-based миграция Synapse: 840 (до bouton_Ca) → 844 (с bouton_Ca)
// bouton_Ca добавлен в Synapse БЕЗ поднятия версии (осталась v10), поэтому
// размер синапса берётся из file_sz_synapse заголовка, а не из sizeof(Synapse).
// ═══════════════════════════════════════════════════════════════════

// Пишет v10 .bion с одним синапсом. on_disk_syn_sz = сколько байт синапса реально
// записано на диск (840 — старые файлы без bouton_Ca, 844 — текущие).
static void write_v10_synapse_file(const char* path, uint32_t file_sz_synapse,
                                   uint32_t on_disk_syn_sz) {
    FILE* f = fopen(path, "wb");
    REQUIRE(f != nullptr);

    NetworkHeader hdr = {};
    hdr.magic = BION_MAGIC;
    hdr.version = 10;
    hdr.mode = 0;
    hdr.num_neurons = 1;
    hdr.num_segments = 2;
    hdr.num_intra_connections = 1;
    hdr.num_synapses = 1;
    hdr.num_nuclei = 0;
    hdr.file_sz_segment = sizeof(Segment);
    hdr.file_sz_synapse = file_sz_synapse;
    hdr.file_sz_nucleus = sizeof(Nucleus);
    hdr.file_sz_intraconnection = sizeof(IntraConnection);
    hdr.file_sz_neuron = sizeof(Neuron);
    hdr.file_sz_modifier = sizeof(Modifier);
    fwrite(&hdr, sizeof(NetworkHeader), 1, f);

    Neuron n = {};
    n.id = 0; n.segment_start = 0; n.segment_count = 2; n.threshold = -20.0f;
    fwrite(&n, sizeof(Neuron), 1, f);

    Segment segs[2] = {};
    segs[0].id = 0; segs[0].neuron_id = 0; segs[0].length = 100.0f; segs[0].radius = 10.0f;
    segs[1].id = 1; segs[1].neuron_id = 0; segs[1].length = 100.0f; segs[1].radius = 10.0f;
    fwrite(segs, sizeof(Segment), 2, f);

    IntraConnection conn = {};
    conn.segment_a = 0; conn.segment_b = 1; conn.is_intracellular = 0; // 0 = bypass геометрии
    fwrite(&conn, sizeof(IntraConnection), 1, f);

    Synapse syn = {};
    syn.id = 5;
    syn.pre_segment = 0;
    syn.post_segment = 1;
    syn.type = SYNAPSE_TYPE_CHEMICAL;
    syn.weight = 2.5f;
    syn.release_probability = 0.7f;
    syn.RRP = 42.0f;
    syn.RRP_max = 100.0f;
    syn.RRP_recovery = 0.05f;
    syn.bouton_Ca = 123.456f;
    REQUIRE(on_disk_syn_sz <= sizeof(Synapse));
    fwrite(&syn, on_disk_syn_sz, 1, f);  // 840 отсекает bouton_Ca (последние 4 байта)

    fclose(f);
}

TEST_CASE("v10 file with 840-byte synapses (pre-bouton_Ca) migrates", "[v3v4][synapse_migration]") {
    const char* path = "/tmp/test_v10_syn840.bion";
    write_v10_synapse_file(path, 840, 840);

    BionData data;
    int rc = bion_load_host(path, &data);
    REQUIRE(rc == 0);
    REQUIRE(data.num_synapses == 1);

    Synapse* s = &data.synapses[0];
    REQUIRE(s->id == 5);
    REQUIRE(s->pre_segment == 0);
    REQUIRE(s->post_segment == 1);
    REQUIRE(s->weight == 2.5f);
    REQUIRE(s->release_probability == 0.7f);
    REQUIRE(s->RRP == 42.0f);
    REQUIRE(s->RRP_max == 100.0f);
    REQUIRE(s->RRP_recovery == 0.05f);
    REQUIRE(s->bouton_Ca == 0.0f);  // поля не было на диске → 0 после миграции

    bion_unload_host(&data);
    remove(path);
}

TEST_CASE("v10 file with 844-byte synapses preserves bouton_Ca", "[v3v4][synapse_migration]") {
    const char* path = "/tmp/test_v10_syn844.bion";
    write_v10_synapse_file(path, 844, 844);

    BionData data;
    int rc = bion_load_host(path, &data);
    REQUIRE(rc == 0);
    REQUIRE(data.num_synapses == 1);

    Synapse* s = &data.synapses[0];
    REQUIRE(s->weight == 2.5f);
    REQUIRE(s->bouton_Ca == 123.456f);

    bion_unload_host(&data);
    remove(path);
}

// ═══════════════════════════════════════════════════════════════════
// Миграция Segment v7-v9 (1692 байта: NT_COUNT=14, без ER_Ca2) → текущий 1704.
// NT_COUNT 14→15 сдвигает modifiers+хвост в СЕРЕДИНЕ структуры → полный remap.
// ═══════════════════════════════════════════════════════════════════
static void write_v9_segment_file(const char* path) {
    FILE* f = fopen(path, "wb");
    REQUIRE(f != nullptr);

    NetworkHeader hdr = {};
    hdr.magic = BION_MAGIC;
    hdr.version = 9;
    hdr.mode = 0;
    hdr.num_neurons = 1;
    hdr.num_segments = 2;
    hdr.num_intra_connections = 1;
    hdr.num_synapses = 0;
    hdr.num_nuclei = 1;
    hdr.nucleus_version = 1;
    fwrite(&hdr, 84, 1, f);  // v9 header = 84 байта (без file_sz_*)

    Neuron n = {};
    n.id = 0; n.segment_start = 0; n.segment_count = 2; n.threshold = -20.0f;
    fwrite(&n, sizeof(Neuron), 1, f);

    SegmentV7 segs[2] = {};
    for (int i = 0; i < 2; i++) {
        segs[i].id = 100 + i;
        segs[i].neuron_id = 0;
        segs[i].position = {float(i * 20), 0.0f, 0.0f};
        segs[i].direction = {0.0f, 1.0f, 0.0f};
        segs[i].length = 15.0f;
        segs[i].radius = 2.5f;
        segs[i].V_m = -65.0f;
        for (int j = 0; j < 5; j++) { segs[i].intra_ions[j] = float(j + 1); segs[i].extra_ions[j] = float(j + 10); }
        for (int j = 0; j < 14; j++) { segs[i].intra_nt[j] = float(j); segs[i].neurotransmitter[j] = float(j + 100); }
        segs[i].modifiers[0].type = 0;
        segs[i].modifiers[0].density = 0.8f;
        segs[i].modifier_count = 1;
        segs[i].segment_type = 1;
        segs[i].ATP_level = 1.0f;
        segs[i].temperature = 310.15f;
        segs[i].I_buffer = -0.5f;
        segs[i].transcription_signal = 0.25f;
    }
    fwrite(segs, sizeof(SegmentV7), 2, f);

    IntraConnection conn = {};
    conn.segment_a = 0; conn.segment_b = 1; conn.is_intracellular = 0;
    fwrite(&conn, sizeof(IntraConnection), 1, f);

    Nucleus nuc = {};
    nuc.neuron_id = 0;
    nuc.enabled = 1;
    fwrite(&nuc, sizeof(Nucleus), 1, f);

    fclose(f);
}

TEST_CASE("v9 file with 1692-byte segments migrates to current", "[v3v4][segment_migration]") {
    const char* path = "/tmp/test_v9_seg1692.bion";
    write_v9_segment_file(path);

    BionData data;
    int rc = bion_load_host(path, &data);
    REQUIRE(rc == 0);
    REQUIRE(data.num_segments == 2);

    Segment* s = &data.segments[0];
    REQUIRE(s->id == 100);
    REQUIRE(s->neuron_id == 0);
    REQUIRE(s->position.x == 0.0f);
    REQUIRE(s->direction.y == 1.0f);
    REQUIRE(s->length == 15.0f);
    REQUIRE(s->radius == 2.5f);
    REQUIRE(s->V_m == -65.0f);
    REQUIRE(s->intra_ions[0] == 1.0f);
    REQUIRE(s->intra_ions[4] == 5.0f);
    REQUIRE(s->extra_ions[0] == 10.0f);
    REQUIRE(s->intra_nt[0] == 0.0f);
    REQUIRE(s->intra_nt[13] == 13.0f);
    REQUIRE(s->intra_nt[14] == 0.0f);       // капсаицин — новое поле
    REQUIRE(s->neurotransmitter[13] == 113.0f);
    REQUIRE(s->neurotransmitter[14] == 0.0f);
    REQUIRE(s->modifier_count == 1);
    REQUIRE(s->modifiers[0].density == 0.8f);
    REQUIRE(s->segment_type == 1);
    REQUIRE(s->ATP_level == 1.0f);
    REQUIRE(s->temperature == 310.15f);
    REQUIRE(s->I_buffer == -0.5f);
    REQUIRE(s->transcription_signal == 0.25f);
    REQUIRE(s->ER_Ca2 == 0.0f);  // поля не было

    Segment* s1 = &data.segments[1];
    REQUIRE(s1->id == 101);
    REQUIRE(s1->position.x == 20.0f);

    bion_unload_host(&data);
    remove(path);
}

// v9 Synapse = 836 байт (NT_COUNT=14 + RRP, без bouton_Ca) → полный remap
static void write_v9_synapse_file(const char* path) {
    FILE* f = fopen(path, "wb");
    REQUIRE(f != nullptr);

    NetworkHeader hdr = {};
    hdr.magic = BION_MAGIC;
    hdr.version = 9;
    hdr.mode = 0;
    hdr.num_neurons = 1;
    hdr.num_segments = 2;
    hdr.num_intra_connections = 1;
    hdr.num_synapses = 1;
    hdr.num_nuclei = 1;
    hdr.nucleus_version = 1;
    fwrite(&hdr, 84, 1, f);

    Neuron n = {};
    n.id = 0; n.segment_start = 0; n.segment_count = 2; n.threshold = -20.0f;
    fwrite(&n, sizeof(Neuron), 1, f);

    SegmentV7 segs[2] = {};
    segs[0].id = 0; segs[0].neuron_id = 0; segs[0].length = 15.0f; segs[0].radius = 2.5f;
    segs[1].id = 1; segs[1].neuron_id = 0; segs[1].length = 15.0f; segs[1].radius = 2.5f;
    fwrite(segs, sizeof(SegmentV7), 2, f);

    IntraConnection conn = {};
    conn.segment_a = 0; conn.segment_b = 1; conn.is_intracellular = 0;
    fwrite(&conn, sizeof(IntraConnection), 1, f);

    SynapseV7 syn = {};
    syn.id = 7;
    syn.pre_segment = 0;
    syn.post_segment = 1;
    syn.type = SYNAPSE_TYPE_CHEMICAL;
    syn.weight = 2.5f;
    syn.release_probability = 0.7f;
    for (int j = 0; j < 14; j++) syn.cleft_neurotransmitter[j] = float(j);
    syn.facilitation = 0.3f;
    syn.depression = 0.2f;
    syn.RRP = 42.0f;
    syn.RRP_max = 100.0f;
    syn.RRP_recovery = 0.05f;
    fwrite(&syn, sizeof(SynapseV7), 1, f);

    Nucleus nuc = {};
    nuc.neuron_id = 0; nuc.enabled = 1;
    fwrite(&nuc, sizeof(Nucleus), 1, f);

    fclose(f);
}

TEST_CASE("v9 file with 836-byte synapses migrates to current", "[v3v4][synapse_migration]") {
    const char* path = "/tmp/test_v9_syn836.bion";
    write_v9_synapse_file(path);

    BionData data;
    int rc = bion_load_host(path, &data);
    REQUIRE(rc == 0);
    REQUIRE(data.num_synapses == 1);

    Synapse* s = &data.synapses[0];
    REQUIRE(s->id == 7);
    REQUIRE(s->pre_segment == 0);
    REQUIRE(s->post_segment == 1);
    REQUIRE(s->weight == 2.5f);
    REQUIRE(s->release_probability == 0.7f);
    REQUIRE(s->cleft_neurotransmitter[0] == 0.0f);
    REQUIRE(s->cleft_neurotransmitter[13] == 13.0f);
    REQUIRE(s->cleft_neurotransmitter[14] == 0.0f);  // капсаицин — новое поле
    REQUIRE(s->facilitation == 0.3f);
    REQUIRE(s->depression == 0.2f);
    REQUIRE(s->RRP == 42.0f);
    REQUIRE(s->RRP_max == 100.0f);
    REQUIRE(s->RRP_recovery == 0.05f);
    REQUIRE(s->bouton_Ca == 0.0f);  // поля не было

    bion_unload_host(&data);
    remove(path);
}
