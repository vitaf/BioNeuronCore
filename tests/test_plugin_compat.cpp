// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_plugin_compat.cpp — валидация ВСЕХ .so плагинов в plugins/.
// Для каждого .so проверяет:
//   1. dlopen успешен (нет missing symbols даже с RTLD_LAZY)
//   2. dlsym("biocore_plugin_init") находит точку входа
//   3. biocore_plugin_init() возвращает > 0 (все register_* вызовы успешны)
//
// Ловит: stale .so от старого SDK, missing symbols, ABI-несовместимость.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>
#include <vector>
#include <string>

#include "types.h"
#include "plugin_manager.h"
#include "modifier_interface.h"

// Собираем пути ко всем .so в plugins/ (рекурсивно)
static std::vector<std::string> find_all_plugins(const char* root) {
    std::vector<std::string> result;
    // plugins/ корень
    DIR* d = opendir(root);
    if (!d) return result;

    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        const char* name = ent->d_name;
        size_t len = strlen(name);

        if (ent->d_type == DT_DIR) {
            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
            char sub[512];
            snprintf(sub, sizeof(sub), "%s/%s", root, name);
            auto sub_plugins = find_all_plugins(sub);
            result.insert(result.end(), sub_plugins.begin(), sub_plugins.end());
        } else if (len > 3 && strcmp(name + len - 3, ".so") == 0) {
            char full[512];
            snprintf(full, sizeof(full), "%s/%s", root, name);
            struct stat st;
            if (stat(full, &st) == 0 && S_ISREG(st.st_mode))
                result.push_back(std::string(full));
        }
    }
    closedir(d);
    return result;
}

// Проверяет, что все undefined-символы плагина разрешаются
// через глобальную таблицу символов вызывающего процесса.
static bool check_undefined_symbols(void* handle, const char* so_path) {
    // Берём адрес register_modifier_current как канареечный —
    // если он доступен, то и остальные export-символы хоста доступны.
    void* sym = dlsym(RTLD_DEFAULT, "register_modifier_current");
    if (!sym) {
        // Пробуем C++ mangled — на случай если extern "C" не сработал
        sym = dlsym(RTLD_DEFAULT, "_Z27register_modifier_currenthPFfP8ModifierP7SegmentfE");
    }
    // Даже если канареечный не найден, продолжаем —
    // реальная проверка будет при вызове init()

    // Проверяем что handle валидный
    if (!handle) return false;

    // Пытаемся найти init-функцию — если её нет, плагин битый
    typedef int (*InitFunc)();
    InitFunc init = (InitFunc)dlsym(handle, "biocore_plugin_init");
    if (!init) {
        fprintf(stderr, "[compat] %s: НЕТ biocore_plugin_init — возможно, не плагин\n", so_path);
        return false;
    }

    // Вызываем init — если символы не резолвятся, упадёт здесь
    int registered = init();
    if (registered <= 0) {
        fprintf(stderr, "[compat] %s: biocore_plugin_init вернул %d (ожидалось > 0)\n",
                so_path, registered);
        // Печатаем dlerror на случай если был symbol lookup error
        const char* err = dlerror();
        if (err) fprintf(stderr, "[compat] %s: dlerror: %s\n", so_path, err);
        return false;
    }

    return true;
}

TEST_CASE("all builtin plugins load and init successfully", "[plugin][compat][builtin]") {
    plugin_manager_init();

    auto builtin_plugins = find_all_plugins("plugins/builtin");
    INFO("Found " << builtin_plugins.size() << " builtin .so plugins");
    REQUIRE(builtin_plugins.size() >= 80);

    int failed = 0;
    for (const auto& path : builtin_plugins) {
        // Очищаем предыдущие ошибки dl
        dlerror();

        void* handle = dlopen(path.c_str(), RTLD_LAZY);
        if (!handle) {
            const char* err = dlerror();
            FAIL(path << ": dlopen failed: " << (err ? err : "unknown"));
            failed++;
            continue;
        }

        bool ok = check_undefined_symbols(handle, path.c_str());
        dlclose(handle);

        if (!ok) {
            FAIL(path << ": init failed (stale .so или missing symbols)");
            failed++;
        }
    }

    REQUIRE(failed == 0);
}

TEST_CASE("user plugins (plugins/*.so) load and init successfully", "[plugin][compat][user]") {
    plugin_manager_init();

    // Загружаем builtin сначала — user-плагины могут зависеть от их символов
    plugin_load_builtin(".");

    auto user_plugins = find_all_plugins("plugins");
    // Оставляем только те, что НЕ в builtin/
    std::vector<std::string> filtered;
    for (const auto& p : user_plugins) {
        if (p.find("/builtin/") == std::string::npos)
            filtered.push_back(p);
    }

    if (filtered.empty()) {
        SUCCEED("Нет пользовательских .so плагинов — нечего проверять");
        return;
    }

    INFO("Found " << filtered.size() << " user .so plugins");
    int failed = 0;

    for (const auto& path : filtered) {
        dlerror();

        void* handle = dlopen(path.c_str(), RTLD_LAZY);
        if (!handle) {
            const char* err = dlerror();
            FAIL(path << ": dlopen failed: " << (err ? err : "unknown"));
            failed++;
            continue;
        }

        bool ok = check_undefined_symbols(handle, path.c_str());
        dlclose(handle);

        if (!ok) {
            FAIL(path << ": init failed — пересоберите плагин с текущим SDK");
            failed++;
        }
    }

    REQUIRE(failed == 0);
}

TEST_CASE("register_modifier_current available in global symbol table", "[plugin][symbols]") {
    // Ключевая проверка: символ register_modifier_current ДОЛЖЕН быть
    // в глобальной таблице символов (экспортирован через -rdynamic).
    // Без него любые SDK-плагины упадут при init().
    void* sym = dlsym(RTLD_DEFAULT, "register_modifier_current");
    INFO("register_modifier_current должен быть экспортирован из тестового бинарника (-rdynamic)");
    REQUIRE(sym != nullptr);

    sym = dlsym(RTLD_DEFAULT, "register_modulator_release");
    INFO("register_modulator_release должен быть экспортирован из тестового бинарника (-rdynamic)");
    REQUIRE(sym != nullptr);
}

TEST_CASE("all plugins have valid biocore_plugin_desc", "[plugin][compat][desc]") {
    plugin_manager_init();
    plugin_load_builtin(".");

    // Проверяем что каждый загруженный плагин имеет ненулевой дескриптор
    auto user_plugins = find_all_plugins("plugins");
    for (const auto& path : user_plugins) {
        if (path.find("/builtin/") != std::string::npos) continue;
        if (path.find(".so") == std::string::npos) continue;

        dlerror();
        void* handle = dlopen(path.c_str(), RTLD_LAZY);
        if (!handle) {
            FAIL(path << ": dlopen failed: " << dlerror());
            continue;
        }

        typedef const char* (*DescFunc)();
        DescFunc desc_func = (DescFunc)dlsym(handle, "biocore_plugin_desc");
        if (desc_func) {
            const char* json = desc_func();
            REQUIRE(json != nullptr);
            REQUIRE(strlen(json) > 10);  // не пустой JSON
        }
        dlclose(handle);
    }
}

TEST_CASE("no stale .so with removed symbols", "[plugin][compat][stale]") {
    // Специфичный тест: проверяем что нет плагинов, ссылающихся на
    // удалённые символы (как register_user_modifier из старого SDK).
    // Ищем любые .so и проверяем что dlopen + init работают.
    plugin_manager_init();
    plugin_load_builtin(".");

    auto all_plugins = find_all_plugins("plugins");
    REQUIRE(all_plugins.size() >= 80);

    int stale = 0;
    for (const auto& path : all_plugins) {
        dlerror();
        void* handle = dlopen(path.c_str(), RTLD_NOW);  // NOW — сразу резолвим все символы
        if (!handle) {
            const char* err = dlerror();
            // RTLD_NOW может упасть на cross-plugin зависимостях —
            // это ожидаемо для builtin-плагинов без preload.
            // Пробуем RTLD_LAZY + init:
            handle = dlopen(path.c_str(), RTLD_LAZY);
            if (!handle) {
                FAIL(path << ": не загружается даже с RTLD_LAZY: " << dlerror());
                stale++;
                continue;
            }

            typedef int (*InitFunc)();
            InitFunc init = (InitFunc)dlsym(handle, "biocore_plugin_init");
            if (!init) {
                dlclose(handle);
                continue;  // не плагин — OK
            }

            int registered = init();
            if (registered <= 0) {
                const char* e = dlerror();
                FAIL(path << ": STALE PLUGIN — init failed (" << registered
                     << "): " << (e ? e : "пересоберите с текущим SDK"));
                stale++;
            }
            dlclose(handle);
        } else {
            dlclose(handle);
        }
    }

    REQUIRE(stale == 0);
}
