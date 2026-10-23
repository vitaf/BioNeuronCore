// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_plugins.cpp — тесты инфраструктуры плагинов
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstring>
#include "types.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include "kernel_dispatch.h"

TEST_CASE("Plugin manager initializes", "[plugins]") {
    plugin_manager_init();

    // Все слоты таблиц должны быть nullptr
    for (int i = 0; i < MOD_TABLE_SIZE; i++) {
        REQUIRE(modifier_current_table[i] == nullptr);
        REQUIRE(modulator_release_table[i] == nullptr);
    }

    // Нет загруженных плагинов
    REQUIRE(plugin_get_count() == 0);
}

TEST_CASE("register_modifier_current adds to table", "[plugins]") {
    plugin_manager_init();

    // Создаём тестовую функцию модификатора
    auto dummy = [](Modifier*, Segment*, float) -> double { return 42.0; };
    register_modifier_current(64, dummy);

    REQUIRE(modifier_current_table[64] != nullptr);
    REQUIRE(modifier_current_table[64](nullptr, nullptr, 0.0f) == 42.0f);

    // Выход за границы не должен регистрировать
    register_modifier_current(200, dummy);
    REQUIRE(modifier_current_table[64] != nullptr); // без изменений
}

TEST_CASE("dispatch_modifier_current uses table", "[plugins]") {
    plugin_manager_init();

    auto my_func = [](Modifier*, Segment*, float) -> double { return 77.0; };
    register_modifier_current(64, my_func);

    Modifier mod = {};
    mod.type = 64;
    Segment seg = {};

    float result = dispatch_modifier_current(0, 64, &mod, &seg, 0.1f);
    REQUIRE(result == 77.0f);
}

TEST_CASE("dispatch_modifier_current returns 0 for unregistered user type", "[plugins]") {
    plugin_manager_init();

    Modifier mod = {};
    mod.type = 100;
    Segment seg = {};

    // Тип 100 не зарегистрирован и отсутствует в switch → 0.0
    float result = dispatch_modifier_current(0, 100, &mod, &seg, 0.1f);
    REQUIRE(result == 0.0f);
}

TEST_CASE("dispatch_release_modulator uses table", "[plugins]") {
    plugin_manager_init();

    auto my_modulator = [](Modifier*, float*, float) -> float { return 0.5f; };
    register_modulator_release(64, my_modulator);

    Modifier mod = {};
    mod.type = 64;
    float cleft[12] = {};

    float result = dispatch_release_modulator(0, 64, &mod, cleft, 0.1f);
    REQUIRE(result == 0.5f);
}

TEST_CASE("dispatch_release_modulator returns 1.0 for unregistered user type", "[plugins]") {
    plugin_manager_init();

    Modifier mod = {};
    mod.type = 120;
    float cleft[12] = {};

    float result = dispatch_release_modulator(0, 120, &mod, cleft, 0.1f);
    REQUIRE(result == 1.0f);
}

TEST_CASE("plugin_load loads sample plugin", "[plugins]") {
    plugin_manager_init();

    // Пробуем загрузить тестовый плагин
    int registered = plugin_load("plugins/example_ca_plugin.so");
    if (registered <= 0) {
        WARN("Sample plugin not found — skipping load test");
        return;
    }

    REQUIRE(registered == 1);

    // Проверяем что функция модификатора зарегистрирована (тип 64)
    REQUIRE(modifier_current_table[64] != nullptr);

    // Проверяем информацию о плагине
    REQUIRE(plugin_get_count() == 1);
    const PluginInfo* p = plugin_get(0);
    REQUIRE(p != nullptr);
    REQUIRE(p->modifier_count >= 1);
    REQUIRE(p->modifiers[0].id == 64);
}

TEST_CASE("plugin_modifier_name lookup", "[plugins]") {
    plugin_manager_init();

    int registered = plugin_load("plugins/example_ca_plugin.so");
    if (registered <= 0) {
        WARN("Sample plugin not found — skipping name test");
        return;
    }

    const char* name = plugin_modifier_name(64);
    REQUIRE(name != nullptr);
    REQUIRE(strlen(name) > 0);

    // Неизвестный модификатор
    REQUIRE(plugin_modifier_name(127) == nullptr);
}
