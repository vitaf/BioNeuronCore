// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_plugin_lifecycle.cpp — полный lifecycle .so плагинов.
// Тестирует plugin_load_builtin, plugin_load_directory, plugin_get_info_json,
// plugin_modifier_name/category/description для реальных builtin-плагинов.
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstring>
#include <cstdio>
#include <sys/stat.h>
#include "types.h"
#include "plugin_manager.h"
#include "modifier_interface.h"

static const char* BUILTIN_DIR = ".";  // plugin_load_builtin добавляет /plugins/builtin

TEST_CASE("plugin_load_builtin loads all plugins", "[plugin][builtin]") {
    plugin_manager_init();

    int loaded = plugin_load_builtin(BUILTIN_DIR);
    INFO("Loaded " << loaded << " builtin plugins");
    REQUIRE(loaded > 0);

    int count = plugin_get_count();
    // Плагины могут регистрировать несколько модификаторов каждый
    REQUIRE(count >= 80);     // не меньше 80 типов модификаторов

    // Все модификаторы должны иметь имена
    for (int i = 0; i < count; i++) {
        const PluginInfo* info = plugin_get(i);
        REQUIRE(info != nullptr);
        REQUIRE(info->modifier_count > 0);
    }
}

TEST_CASE("plugin_load_directory on builtin dir", "[plugin][directory]") {
    plugin_manager_init();

    int loaded = plugin_load_directory("plugins/builtin");
    INFO("plugin_load_directory loaded " << loaded << " plugins");
    REQUIRE(loaded > 0);

    int count = plugin_get_count();
    REQUIRE(count >= 80);
}

TEST_CASE("plugin_modifier_name for every builtin type", "[plugin][names]") {
    plugin_manager_init();
    REQUIRE(plugin_load_builtin(BUILTIN_DIR) > 0);

    int count = plugin_get_count();
    for (int i = 0; i < count; i++) {
        const PluginInfo* info = plugin_get(i);
        REQUIRE(info != nullptr);
        for (uint32_t j = 0; j < info->modifier_count; j++) {
            uint8_t type = info->modifiers[j].id;
            const char* name = plugin_modifier_name(type);
            REQUIRE(name != nullptr);
            REQUIRE(strlen(name) > 0);
        }
    }
}

TEST_CASE("plugin_modifier_category for builtin types", "[plugin][category]") {
    plugin_manager_init();
    REQUIRE(plugin_load_builtin(BUILTIN_DIR) > 0);

    // Проверяем ключевые категории
    const char* cat_nak = plugin_modifier_category(0);
    REQUIRE(cat_nak != nullptr);
    REQUIRE(strlen(cat_nak) > 0);

    const char* cat_ampa = plugin_modifier_category(8);
    REQUIRE(cat_ampa != nullptr);
    REQUIRE(strlen(cat_ampa) > 0);

    const char* cat_gaba = plugin_modifier_category(11);
    REQUIRE(cat_gaba != nullptr);
    REQUIRE(strlen(cat_gaba) > 0);
}

TEST_CASE("plugin_modifier_description returns non-empty", "[plugin][desc]") {
    plugin_manager_init();
    REQUIRE(plugin_load_builtin(BUILTIN_DIR) > 0);

    for (int i = 0; i < 5; i++) {
        const char* desc = plugin_modifier_description((uint8_t)i);
        REQUIRE(desc != nullptr);
    }
}

TEST_CASE("plugin_get_info_json returns content for loaded modifiers", "[plugin][info]") {
    plugin_manager_init();
    REQUIRE(plugin_load_builtin(BUILTIN_DIR) > 0);

    // Загружаем .info файлы отдельно (plugin_load_builtin их не загружает)
    int info_loaded = plugin_load_info_directory("plugins/builtin");
    INFO("Loaded " << info_loaded << " .info files");

    int info_count = 0;
    int count = plugin_get_count();
    for (int i = 0; i < count; i++) {
        const PluginInfo* inf = plugin_get(i);
        for (uint32_t j = 0; j < inf->modifier_count; j++) {
            const char* json = plugin_get_info_json(inf->modifiers[j].id);
            if (json != nullptr) info_count++;
        }
    }
    INFO("Found " << info_count << " modifiers with .info data");
    REQUIRE(info_count >= 1); // хотя бы один должен быть
}

TEST_CASE("plugin_double_load does not crash", "[plugin][dedup]") {
    plugin_manager_init();

    int first = plugin_load_builtin(BUILTIN_DIR);
    REQUIRE(first > 0);

    int second = plugin_load_builtin(BUILTIN_DIR);
    // Двойная загрузка не должна крашиться
    REQUIRE(second >= 0);
}

TEST_CASE("plugin_find_modifier with registered modifiers", "[plugin][find]") {
    plugin_manager_init();
    REQUIRE(plugin_load_builtin(BUILTIN_DIR) > 0);

    const ModifierDescriptor* desc = plugin_find_modifier(107); // K2P_LEAK (реальный тип — 0 удалён в #69)
    REQUIRE(desc != nullptr);

    const ModifierDescriptor* desc_bad = plugin_find_modifier(255);
    REQUIRE(desc_bad == nullptr);
}

TEST_CASE("plugin_manager_init resets state", "[plugin][reset]") {
    plugin_manager_init();
    REQUIRE(plugin_load_builtin(BUILTIN_DIR) > 0);
    REQUIRE(plugin_get_count() > 0);

    plugin_manager_init();
    REQUIRE(plugin_get_count() == 0);

    // После сброса загрузка снова работает
    REQUIRE(plugin_load_builtin(BUILTIN_DIR) > 0);
    REQUIRE(plugin_get_count() > 0);
}

TEST_CASE("plugin_load nonexistent .so", "[plugin][error]") {
    plugin_manager_init();
    int ret = plugin_load("/tmp/nonexistent_plugin.so");
    REQUIRE(ret < 0); // отрицательный код ошибки
}

TEST_CASE("plugin_load_builtin nonexistent directory", "[plugin][error]") {
    plugin_manager_init();
    int ret = plugin_load_builtin("/tmp/no_such_plugin_dir/");
    REQUIRE(ret >= 0); // не крашится, возвращает 0 или положительное
}

TEST_CASE("plugin_load_directory with non-.so files", "[plugin][directory]") {
    plugin_manager_init();
    // /tmp содержит множество файлов, но не .so плагинов
    int ret = plugin_load_directory("/tmp");
    REQUIRE(ret >= 0); // не крашится
}

TEST_CASE("plugin_get negative/beyond bounds", "[plugin][bounds]") {
    plugin_manager_init();
    REQUIRE(plugin_load_builtin(BUILTIN_DIR) > 0);

    REQUIRE(plugin_get(-1) == nullptr);
    REQUIRE(plugin_get(99999) == nullptr);
}

TEST_CASE("plugin_get_info_json on unloaded type", "[plugin][info]") {
    plugin_manager_init();
    const char* json = plugin_get_info_json(255);
    REQUIRE(json == nullptr);
}

TEST_CASE("plugin_modifier_name unregistered type", "[plugin][unreg]") {
    plugin_manager_init();
    const char* name = plugin_modifier_name(255);
    // Для незарегистрированного типа может вернуть nullptr или fallback
    // Главное — не крашится
    (void)name;
    SUCCEED();
}

TEST_CASE("plugin_get_all with valid buffer", "[plugin][all]") {
    plugin_manager_init();
    REQUIRE(plugin_load_builtin(BUILTIN_DIR) > 0);
    int count = plugin_get_count();
    REQUIRE(count > 0);

    std::vector<PluginInfo> buf(count);
    int got = plugin_get_all(buf.data(), count);
    REQUIRE(got == count);

    for (int i = 0; i < got; i++) {
        REQUIRE(buf[i].modifier_count > 0);
        REQUIRE(strlen(buf[i].name) > 0);
    }
}
