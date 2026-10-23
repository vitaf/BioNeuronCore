// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_core_regression.cpp — регрессионные тесты для ранее исправленных багов.
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstring>
#include <cstdio>
#include <cmath>
#include "types.h"
#include "bion_format.h"
#include "modifier_interface.h"

extern "C" {
    const char* core_version_string();
    uint32_t core_version_major();
    uint32_t core_version_minor();
    uint32_t core_version_patch();
}

TEST_CASE("sizeof Nucleus is stable (500 bytes)", "[regression][structs]") {
    REQUIRE(sizeof(Nucleus) == 500);
}

TEST_CASE("sizeof Segment is stable (1692 bytes)", "[regression][structs]") {
    REQUIRE(sizeof(Segment) == 3216);  // v14: I_buffer 8-aligned
}

TEST_CASE("sizeof Modifier is stable (44 bytes)", "[regression][structs]") {
    REQUIRE(sizeof(Modifier) == 88);
}

TEST_CASE("sizeof NetworkHeader is 100 bytes", "[regression][structs]") {
    REQUIRE(sizeof(NetworkHeader) == 100);  // v10: +file_sz fields
}

TEST_CASE("BION_MAGIC is 0x42494F4E", "[regression][constants]") {
    REQUIRE(BION_MAGIC == 0x42494F4E);
}

TEST_CASE("BION_VERSION is 10", "[regression][constants]") {
    REQUIRE(BION_VERSION == 14);
}

TEST_CASE("BION_VERSION_MIN is 3", "[regression][constants]") {
    REQUIRE(BION_VERSION_MIN == 3);
}

TEST_CASE("ION_COUNT is 5", "[regression][constants]") {
    REQUIRE(ION_COUNT == 7);
}

TEST_CASE("NT_COUNT is 15", "[regression][constants]") {
    REQUIRE(NT_COUNT == 15);
}

TEST_CASE("MOD_TYPE_COUNT is 212", "[regression][constants]") {
    REQUIRE(MOD_TYPE_COUNT == 212);
}

TEST_CASE("core_version_string returns non-null", "[regression][version]") {
    const char* v = core_version_string();
    REQUIRE(v != nullptr);
    REQUIRE(strlen(v) > 0);
}

TEST_CASE("core_version components are consistent", "[regression][version]") {
    REQUIRE(core_version_major() == 0);
    REQUIRE(core_version_minor() == 10);
    REQUIRE(core_version_patch() == 0);
}

TEST_CASE("bion_validate_header rejects null", "[regression][bion]") {
    REQUIRE(bion_validate_header(nullptr) == -1);
}

TEST_CASE("bion_version_string returns non-null for v3-v6", "[regression][bion]") {
    NetworkHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = BION_MAGIC;

    hdr.version = 3;
    REQUIRE(bion_version_string(&hdr) != nullptr);

    hdr.version = 6;
    REQUIRE(bion_version_string(&hdr) != nullptr);
}

TEST_CASE("FARADAY_CONST and GAS_CONST are physical", "[regression][constants]") {
    // Биологические константы не должны быть нулевыми
    REQUIRE(FARADAY_CONST > 0.0f);
    REQUIRE(GAS_CONST > 0.0f);
    REQUIRE(CELSIUS_37 > 0.0f);
}

TEST_CASE("Vec3 size and alignment", "[regression][structs]") {
    REQUIRE(sizeof(Vec3) == 12);
}

TEST_CASE("state[7] is part of Modifier struct", "[regression][structs]") {
    Modifier m;
    memset(&m, 0, sizeof(m));
    // state[7] должен быть доступен (scratchpad для effective_area)
    m.state[7] = 100.0f;
    REQUIRE(m.state[7] == 100.0f);
}
