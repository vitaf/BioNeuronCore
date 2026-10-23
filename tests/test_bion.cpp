// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include "types.h"
#include "bion_loader.h"
#include <sycl/sycl.hpp>
#include <cstdio>
#include <cstring>

// Вспомогательная: создание минимального .bion в памяти
static void create_minimal_bion(const char* filename) {
    NetworkHeader hdr = {};
    hdr.magic = BION_MAGIC;
    hdr.version = BION_VERSION;
    hdr.mode = MODE_BIOLOGICAL;
    hdr.num_neurons = 1;
    hdr.num_segments = 2;
    hdr.num_intra_connections = 1;
    hdr.num_synapses = 0;
    hdr.timestamp = 1234567890;
    strncpy(hdr.build_id, "test", 16);

    Neuron neuron = { 0, 0, 2, 0, -40.0f, 0, {} };
    Segment segs[2] = {};
    segs[0].id = 0; segs[0].neuron_id = 0;
    segs[0].length = 100; segs[0].radius = 10;
    segs[1].id = 1; segs[1].neuron_id = 0;
    segs[1].length = 100; segs[1].radius = 5;
    IntraConnection conn = { 0, 1, 1 };

    FILE* f = fopen(filename, "wb");
    fwrite(&hdr, sizeof(NetworkHeader), 1, f);
    fwrite(&neuron, sizeof(Neuron), 1, f);
    fwrite(segs, sizeof(Segment), 2, f);
    fwrite(&conn, sizeof(IntraConnection), 1, f);
    fclose(f);
}

TEST_CASE("BION: Load valid file", "[bion]") {
    create_minimal_bion("test_valid.bion");
    sycl::queue q(sycl::cpu_selector_v);
    BionData data;
    int ret = bion_load_usm("test_valid.bion", &data, q);
    REQUIRE(ret == 0);
    REQUIRE(data.header.num_segments == 2);
    REQUIRE(data.header.num_neurons == 1);
    REQUIRE(data.header.num_intra_connections == 1);
    REQUIRE(data.header.mode == MODE_BIOLOGICAL);
    REQUIRE(data.segments[0].length == 100.0f);
    REQUIRE(data.connections[0].is_intracellular == 1);
    bion_unload_usm(&data, q);
    remove("test_valid.bion");
}

TEST_CASE("BION: Reject bad magic", "[bion]") {
    NetworkHeader hdr = {};
    hdr.magic = 0xDEADBEEF;
    hdr.version = BION_VERSION;
    hdr.num_segments = 1;
    FILE* f = fopen("test_bad.bin", "wb");
    fwrite(&hdr, sizeof(NetworkHeader), 1, f);
    fclose(f);
    sycl::queue q(sycl::cpu_selector_v);
    BionData data;
    int ret = bion_load_usm("test_bad.bin", &data, q);
    REQUIRE(ret != 0);
    remove("test_bad.bin");
}

TEST_CASE("BION: Reject unsupported version", "[bion]") {
    NetworkHeader hdr = {};
    hdr.magic = BION_MAGIC;
    hdr.version = 99;
    hdr.num_segments = 1;
    FILE* f = fopen("test_ver.bin", "wb");
    fwrite(&hdr, sizeof(NetworkHeader), 1, f);
    fclose(f);
    sycl::queue q(sycl::cpu_selector_v);
    BionData data;
    int ret = bion_load_usm("test_ver.bin", &data, q);
    REQUIRE(ret != 0);
    remove("test_ver.bin");
}

TEST_CASE("BION: Validate segment connections", "[bion]") {
    NetworkHeader hdr = {};
    hdr.magic = BION_MAGIC;
    hdr.version = BION_VERSION;
    hdr.num_segments = 2;
    hdr.num_intra_connections = 1;
    Segment segs[2] = {};
    IntraConnection conn = { 0, 999, 1 }; // невалидный индекс segment_b
    FILE* f = fopen("test_bad_conn.bin", "wb");
    fwrite(&hdr, sizeof(NetworkHeader), 1, f);
    fwrite(segs, sizeof(Segment), 2, f);
    fwrite(&conn, sizeof(IntraConnection), 1, f);
    fclose(f);
    sycl::queue q(sycl::cpu_selector_v);
    BionData data;
    int ret = bion_load_usm("test_bad_conn.bin", &data, q);
    REQUIRE(ret != 0);
    remove("test_bad_conn.bin");
}

TEST_CASE("BION: Load synapse with pre/post receptors", "[bion]") {
    NetworkHeader hdr = {};
    hdr.magic = BION_MAGIC;
    hdr.version = BION_VERSION;
    hdr.mode = MODE_BIOLOGICAL;
    hdr.num_segments = 2;
    hdr.num_synapses = 1;
    hdr.num_intra_connections = 0;
    hdr.num_neurons = 0;

    Segment segs[2] = {};
    segs[0].id = 0; segs[0].length = 100; segs[0].radius = 10;
    segs[1].id = 1; segs[1].length = 100; segs[1].radius = 10;

    Synapse syn = {};
    syn.id = 0;
    syn.pre_segment = 0;
    syn.post_segment = 1;
    syn.type = 0;
    syn.pre_autoreceptors[0] = { MOD_NT_RECEPTOR_5HT1A, {0,0,0}, 0.3f, {}, 0, 0 };
    syn.pre_autoreceptor_count = 1;
    syn.post_receptors[0] = { MOD_NT_RECEPTOR_AMPA, {0,0,0}, 0.5f, {}, 0, 0 };
    syn.post_receptors[1] = { MOD_NT_RECEPTOR_NMDA, {0,0,0}, 0.2f, {}, 0, 0 };
    syn.post_receptor_count = 2;

    FILE* f = fopen("test_syn.bin", "wb");
    fwrite(&hdr, sizeof(NetworkHeader), 1, f);
    fwrite(segs, sizeof(Segment), 2, f);
    fwrite(&syn, sizeof(Synapse), 1, f);
    fclose(f);

    sycl::queue q(sycl::cpu_selector_v);
    BionData data;
    int ret = bion_load_usm("test_syn.bin", &data, q);
    REQUIRE(ret == 0);
    REQUIRE(data.synapses[0].pre_autoreceptor_count == 1);
    REQUIRE(data.synapses[0].pre_autoreceptors[0].type == MOD_NT_RECEPTOR_5HT1A);
    REQUIRE(data.synapses[0].post_receptor_count == 2);
    REQUIRE(data.synapses[0].post_receptors[0].type == MOD_NT_RECEPTOR_AMPA);
    REQUIRE(data.synapses[0].post_receptors[1].type == MOD_NT_RECEPTOR_NMDA);

    bion_unload_usm(&data, q);
    remove("test_syn.bin");
}

TEST_CASE("BION: Save and reload produces identical data", "[bion]") {
    create_minimal_bion("test_original.bion");
    sycl::queue q(sycl::cpu_selector_v);
    BionData data1, data2;
    int r1 = bion_load_usm("test_original.bion", &data1, q);
    REQUIRE(r1 == 0);
    int r2 = bion_save("test_copy.bion", &data1);
    REQUIRE(r2 == 0);
    int r3 = bion_load_usm("test_copy.bion", &data2, q);
    REQUIRE(r3 == 0);

    REQUIRE(data1.header.num_segments == data2.header.num_segments);
    REQUIRE(data1.segments[0].length == data2.segments[0].length);
    REQUIRE(data1.connections[0].is_intracellular == data2.connections[0].is_intracellular);

    bion_unload_usm(&data1, q);
    bion_unload_usm(&data2, q);
    remove("test_original.bion");
    remove("test_copy.bion");
}

TEST_CASE("BION: Reject zero segments", "[bion]") {
    NetworkHeader hdr = {};
    hdr.magic = BION_MAGIC;
    hdr.version = BION_VERSION;
    hdr.num_segments = 0;
    FILE* f = fopen("test_zero.bin", "wb");
    fwrite(&hdr, sizeof(NetworkHeader), 1, f);
    fclose(f);
    sycl::queue q(sycl::cpu_selector_v);
    BionData data;
    int ret = bion_load_usm("test_zero.bin", &data, q);
    REQUIRE(ret != 0);
    remove("test_zero.bin");
}
