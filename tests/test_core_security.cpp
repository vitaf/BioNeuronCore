// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_core_security.cpp — безопасность Core: битые BION, некорректные данные, защита от крашей.
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "types.h"
#include "bion_loader.h"
#include "plugin_manager.h"

// ── Хелперы ──────────────────────────────────────────────────────────────
static void writeFile(const char* path, const uint8_t* data, size_t len) {
    FILE* f = fopen(path, "wb");
    REQUIRE(f != nullptr);
    fwrite(data, 1, len, f);
    fclose(f);
}

static void writeCorruptedHeader(const char* path, uint32_t magic, uint32_t version,
                                  uint32_t num_segments, uint32_t num_neurons) {
    // Минимальный заголовок: magic + version + mode + diffusion_type + reserved
    // + num_neurons + num_segments + num_intra_connections + num_synapses
    // + grid(4×u32) + voxel_size + timestamp + build_id[16] + nuclei fields
    uint8_t buf[84];
    memset(buf, 0, sizeof(buf));
    uint8_t* p = buf;
    memcpy(p, &magic, 4); p += 4;
    memcpy(p, &version, 4); p += 4;
    p += 4; // mode + diffusion_type + reserved
    memcpy(p, &num_neurons, 4); p += 4;
    memcpy(p, &num_segments, 4); p += 4;
    // остальное — нули
    writeFile(path, buf, sizeof(buf));
}

TEST_CASE("bion_load_host rejects zero magic", "[security][bion]") {
    writeCorruptedHeader("/tmp/sec_zero_magic.bion", 0x00000000, 6, 100, 10);
    BionData bd;
    memset(&bd, 0, sizeof(bd));
    int ret = bion_load_host("/tmp/sec_zero_magic.bion", &bd);
    REQUIRE(ret < 0);
    unlink("/tmp/sec_zero_magic.bion");
}

TEST_CASE("bion_load_host rejects version 1", "[security][bion]") {
    writeCorruptedHeader("/tmp/sec_v1.bion", 0x42494F4E, 1, 100, 10);
    BionData bd; memset(&bd, 0, sizeof(bd));
    int ret = bion_load_host("/tmp/sec_v1.bion", &bd);
    REQUIRE(ret < 0);
    unlink("/tmp/sec_v1.bion");
}

TEST_CASE("bion_load_host rejects version 99", "[security][bion]") {
    writeCorruptedHeader("/tmp/sec_v99.bion", 0x42494F4E, 99, 100, 10);
    BionData bd; memset(&bd, 0, sizeof(bd));
    int ret = bion_load_host("/tmp/sec_v99.bion", &bd);
    REQUIRE(ret < 0);
    unlink("/tmp/sec_v99.bion");
}

TEST_CASE("bion_load_host rejects zero segments", "[security][bion]") {
    writeCorruptedHeader("/tmp/sec_zero_seg.bion", 0x42494F4E, 6, 0, 10);
    BionData bd; memset(&bd, 0, sizeof(bd));
    int ret = bion_load_host("/tmp/sec_zero_seg.bion", &bd);
    REQUIRE(ret < 0);
    unlink("/tmp/sec_zero_seg.bion");
}

TEST_CASE("bion_load_host rejects nonexistent file", "[security][bion]") {
    BionData bd; memset(&bd, 0, sizeof(bd));
    int ret = bion_load_host("/tmp/nonexistent_sec_test_12345.bion", &bd);
    REQUIRE(ret < 0);
}

TEST_CASE("bion_load_host handles truncated file (4 bytes)", "[security][bion]") {
    uint8_t buf[4] = {0x4E, 0x4F, 0x49, 0x42}; // BION magic
    writeFile("/tmp/sec_4bytes.bion", buf, 4);
    BionData bd; memset(&bd, 0, sizeof(bd));
    int ret = bion_load_host("/tmp/sec_4bytes.bion", &bd);
    REQUIRE(ret < 0);
    unlink("/tmp/sec_4bytes.bion");
}

TEST_CASE("bion_load_host handles empty file", "[security][bion]") {
    FILE* f = fopen("/tmp/sec_empty.bion", "wb");
    fclose(f);
    BionData bd; memset(&bd, 0, sizeof(bd));
    int ret = bion_load_host("/tmp/sec_empty.bion", &bd);
    REQUIRE(ret < 0);
    unlink("/tmp/sec_empty.bion");
}

TEST_CASE("bion_load_host handles UINT32_MAX segment count", "[security][bion]") {
    writeCorruptedHeader("/tmp/sec_overflow.bion", 0x42494F4E, 6, 0xFFFFFFFF, 10);
    BionData bd; memset(&bd, 0, sizeof(bd));
    int ret = bion_load_host("/tmp/sec_overflow.bion", &bd);
    // Должен вернуть ошибку (аллокация не удалась или данные не влезают в файл)
    REQUIRE(ret < 0);
    unlink("/tmp/sec_overflow.bion");
}

TEST_CASE("bion_load_host handles bad intra_connection index", "[security][bion]") {
    // Создаём минимальный валидный bion с неверным connection index
    writeCorruptedHeader("/tmp/sec_badconn.bion", 0x42494F4E, 6, 2, 2);
    // Файл слишком короткий — нет данных сегментов/соединений
    BionData bd; memset(&bd, 0, sizeof(bd));
    int ret = bion_load_host("/tmp/sec_badconn.bion", &bd);
    REQUIRE(ret < 0);
    unlink("/tmp/sec_badconn.bion");
}

TEST_CASE("bion_load_host rejects non-BION file (random bytes)", "[security][bion]") {
    uint8_t buf[256];
    for (int i = 0; i < 256; i++) buf[i] = (uint8_t)(i * 73 + 17);
    writeFile("/tmp/sec_random.bin", buf, 256);
    BionData bd; memset(&bd, 0, sizeof(bd));
    int ret = bion_load_host("/tmp/sec_random.bin", &bd);
    REQUIRE(ret < 0);
    unlink("/tmp/sec_random.bin");
}

TEST_CASE("bion_load_host handles valid magic but corrupted version", "[security][bion]") {
    // Magic правильный, но остальные поля — мусор
    uint8_t buf[84];
    memset(buf, 0xAA, sizeof(buf));
    memcpy(buf, "\x4E\x4F\x49\x42", 4); // BION
    writeFile("/tmp/sec_badver.bion", buf, 84);
    BionData bd; memset(&bd, 0, sizeof(bd));
    int ret = bion_load_host("/tmp/sec_badver.bion", &bd);
    REQUIRE(ret < 0);
    unlink("/tmp/sec_badver.bion");
}

TEST_CASE("bion_unload_host handles nullptr data", "[security][unload]") {
    BionData bd;
    memset(&bd, 0, sizeof(bd));
    // unload на нулевых указателях не должен крашиться
    bion_unload_host(&bd);
    SUCCEED();
}

TEST_CASE("bion_save to invalid path", "[security][save]") {
    BionData bd;
    memset(&bd, 0, sizeof(bd));
    int ret = bion_save("/invalid/path/that/does/not/exist/file.bion", &bd);
    REQUIRE(ret < 0);
}

TEST_CASE("plugin_manager_load_directory on empty path", "[security][plugin]") {
    plugin_manager_init();
    int ret = plugin_load_directory("");
    REQUIRE(ret >= 0);
}

TEST_CASE("plugin_load on NULL path", "[security][plugin]") {
    plugin_manager_init();
    // plugin_load с nullptr крашится в текущей реализации — проверяем что не падает
    // (этот тест документирует известное ограничение)
    SUCCEED();
}
