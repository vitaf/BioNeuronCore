// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_plugin_manager.cpp — тесты plugin_manager: load_directory (пустая/нет),
// plugin_get_info_json (незагруженный), plugin_find_modifier (неизвестный),
// plugin_get/gat_all (граничные случаи).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstring>
#include <cstdio>
#include <sys/stat.h>
#include <sys/types.h>
#include "types.h"
#include "plugin_manager.h"

TEST_CASE("plugin_get_count returns 0 initially", "[plugin_manager]") {
    plugin_manager_init();
    REQUIRE(plugin_get_count() == 0);
}

TEST_CASE("plugin_get with negative index returns nullptr", "[plugin_manager]") {
    plugin_manager_init();
    REQUIRE(plugin_get(-1) == nullptr);
    REQUIRE(plugin_get(-100) == nullptr);
}

TEST_CASE("plugin_get beyond loaded count returns nullptr", "[plugin_manager]") {
    plugin_manager_init();
    REQUIRE(plugin_get(0) == nullptr);
    REQUIRE(plugin_get(1) == nullptr);
    REQUIRE(plugin_get(999) == nullptr);
}

TEST_CASE("plugin_get_all with nullptr buffer returns 0", "[plugin_manager]") {
    plugin_manager_init();
    REQUIRE(plugin_get_all(nullptr, 10) == 0);
}

TEST_CASE("plugin_get_all with zero capacity returns 0", "[plugin_manager]") {
    plugin_manager_init();
    PluginInfo buf[4];
    REQUIRE(plugin_get_all(buf, 0) == 0);
}

TEST_CASE("plugin_get_all with negative capacity returns 0", "[plugin_manager]") {
    plugin_manager_init();
    PluginInfo buf[4];
    REQUIRE(plugin_get_all(buf, -1) == 0);
}

TEST_CASE("plugin_find_modifier for unregistered ID returns nullptr", "[plugin_manager]") {
    plugin_manager_init();
    REQUIRE(plugin_find_modifier(0) == nullptr);
    REQUIRE(plugin_find_modifier(64) == nullptr);
    REQUIRE(plugin_find_modifier(127) == nullptr);
}

TEST_CASE("plugin_modifier_name for unregistered ID returns nullptr", "[plugin_manager]") {
    plugin_manager_init();
    REQUIRE(plugin_modifier_name(0) == nullptr);
    REQUIRE(plugin_modifier_name(100) == nullptr);
}

TEST_CASE("plugin_modifier_category for unregistered ID returns nullptr", "[plugin_manager]") {
    plugin_manager_init();
    REQUIRE(plugin_modifier_category(0) == nullptr);
}

TEST_CASE("plugin_modifier_description for unregistered ID returns nullptr", "[plugin_manager]") {
    plugin_manager_init();
    REQUIRE(plugin_modifier_description(0) == nullptr);
}

TEST_CASE("plugin_load_directory with nonexistent directory returns 0", "[plugin_manager]") {
    plugin_manager_init();
    int n = plugin_load_directory("/tmp/nonexistent_plugin_dir_xyz12345");
    REQUIRE(n == 0);
}

TEST_CASE("plugin_load_directory with empty directory returns 0", "[plugin_manager]") {
    plugin_manager_init();

    const char* empty_dir = "/tmp/empty_plugin_test_dir";
    mkdir(empty_dir, 0755);

    int n = plugin_load_directory(empty_dir);
    REQUIRE(n == 0);

    rmdir(empty_dir);
}

TEST_CASE("plugin_load_directory with directory_of_non_so_files returns 0", "[plugin_manager]") {
    plugin_manager_init();

    const char* test_dir = "/tmp/test_plugin_non_so";
    mkdir(test_dir, 0755);

    // Создаём файлы не-.so
    FILE* f1 = fopen("/tmp/test_plugin_non_so/readme.txt", "w");
    if (f1) { fprintf(f1, "hello"); fclose(f1); }
    FILE* f2 = fopen("/tmp/test_plugin_non_so/plugin.so.bak", "w");
    if (f2) { fprintf(f2, "backup"); fclose(f2); }

    // .so без прав на выполнение — stat проверяет только S_ISREG, не права
    // Просто .so без валидного ELF — dlopen вернёт ошибку
    FILE* f3 = fopen("/tmp/test_plugin_non_so/bad.so", "w");
    if (f3) { fprintf(f3, "not an elf"); fclose(f3); }

    // plugin_load_directory пытается загрузить bad.so через dlopen → ошибка
    // Функция возвращает количество ЗАРЕГИСТРИРОВАННЫХ модификаторов
    // При ошибке dlopen возвращает -1, что суммируется как n>0? нет, n=-1, условие n>0 ложно
    // Итог: total = 0
    int n = plugin_load_directory(test_dir);
    REQUIRE(n == 0);

    // Очистка
    remove("/tmp/test_plugin_non_so/readme.txt");
    remove("/tmp/test_plugin_non_so/plugin.so.bak");
    remove("/tmp/test_plugin_non_so/bad.so");
    rmdir(test_dir);
}

TEST_CASE("plugin_load with nonexistent file returns negative", "[plugin_manager]") {
    plugin_manager_init();
    int n = plugin_load("/tmp/nonexistent_plugin_xyz12345.so");
    REQUIRE(n < 0);
}

TEST_CASE("plugin_get_info_json for unloaded modifier returns nullptr", "[plugin_manager]") {
    plugin_manager_init();
    REQUIRE(plugin_get_info_json(0) == nullptr);
    REQUIRE(plugin_get_info_json(10) == nullptr);
    REQUIRE(plugin_get_info_json(86) == nullptr);
}

TEST_CASE("plugin_load_info_directory with nonexistent dir returns 0", "[plugin_manager]") {
    plugin_manager_init();
    int n = plugin_load_info_directory("/tmp/nonexistent_info_dir_xyz12345");
    REQUIRE(n == 0);
}

TEST_CASE("plugin_load_info_directory with empty dir returns 0", "[plugin_manager]") {
    plugin_manager_init();

    const char* empty_dir = "/tmp/empty_info_test_dir";
    mkdir(empty_dir, 0755);

    int n = plugin_load_info_directory(empty_dir);
    REQUIRE(n == 0);

    rmdir(empty_dir);
}

TEST_CASE("plugin_manager_init resets loaded count", "[plugin_manager]") {
    plugin_manager_init();

    // После init счётчик плагинов = 0
    REQUIRE(plugin_get_count() == 0);

    // После попытки загрузить несуществующий плагин всё ещё 0
    plugin_load("/tmp/nonexistent.so");
    REQUIRE(plugin_get_count() == 0);

    // Повторный init не должен крашить
    plugin_manager_init();
    REQUIRE(plugin_get_count() == 0);
}
