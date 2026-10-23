// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// plugin_manager.cpp — загрузчик .so плагинов.
// Vitis HLS: фиксированные массивы, без std::vector, без new/malloc.
#include "plugin_manager.h"
#include "daemon_log.h"
#include <dlfcn.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <sys/stat.h>

// Глобальные таблицы функций (CPU-путь)
ModifierFunc user_library[MOD_USER_COUNT];
ReleaseModulatorFunc user_modulators[MOD_USER_COUNT];

// Основные таблицы для диспетчеризации (используются kernel_dispatch.h)
ModifierFunc modifier_current_table[MOD_TABLE_SIZE];
ReleaseModulatorFunc modulator_release_table[MOD_TABLE_SIZE];

// Фиксированный массив загруженных плагинов
static PluginInfo loaded_plugins[MAX_PLUGINS];
static int loaded_count = 0;

void plugin_manager_init() {
    memset(user_library, 0, sizeof(user_library));
    memset(user_modulators, 0, sizeof(user_modulators));
    memset(modifier_current_table, 0, sizeof(modifier_current_table));
    memset(modulator_release_table, 0, sizeof(modulator_release_table));
    memset(loaded_plugins, 0, sizeof(loaded_plugins));
    loaded_count = 0;
}

extern "C" void register_modifier_current(uint8_t type, ModifierFunc func) {
    if (type < MOD_TABLE_SIZE) {
        modifier_current_table[type] = func;
    }
}

extern "C" void register_modulator_release(uint8_t type, ReleaseModulatorFunc func) {
    if (type < MOD_TABLE_SIZE) {
        modulator_release_table[type] = func;
    }
}

void register_user_modifier(uint8_t type, ModifierFunc func) {
    if (type <= MOD_USER_MAX) {
        user_library[type] = func;
        // Дублируем в основную таблицу для унифицированного CPU-пути
        if (type < MOD_TABLE_SIZE) {
            modifier_current_table[type] = func;
        }
        // Регистрируем в arbitrary_library для смешанного диспетча (get_modifier_func)
        if (type < MOD_TYPE_COUNT) {
            arbitrary_library[type] = func;
        }
    }
}

void register_user_modulator(uint8_t type, ReleaseModulatorFunc func) {
    if (type <= MOD_USER_MAX) {
        user_modulators[type] = func;
        // Дублируем в основную таблицу для унифицированного CPU-пути
        if (type < MOD_TABLE_SIZE) {
            modulator_release_table[type] = func;
        }
        // Регистрируем в arbitrary_modulators для смешанного диспетча (get_modulator_func)
        if (type < MOD_TYPE_COUNT) {
            arbitrary_modulators[type] = func;
        }
    }
}

int plugin_get_count() {
    return loaded_count;
}

const PluginInfo* plugin_get(int idx) {
    if (idx < 0 || idx >= loaded_count) return nullptr;
    return &loaded_plugins[idx];
}

int plugin_get_all(PluginInfo* out_buf, int buf_capacity) {
    if (!out_buf || buf_capacity <= 0) return 0;
    int n = (loaded_count < buf_capacity) ? loaded_count : buf_capacity;
    memcpy(out_buf, loaded_plugins, n * sizeof(PluginInfo));
    return n;
}

const ModifierDescriptor* plugin_find_modifier(uint8_t id) {
    for (int i = 0; i < loaded_count; i++) {
        const PluginInfo& p = loaded_plugins[i];
        for (int j = 0; j < p.modifier_count; j++) {
            if (p.modifiers[j].id == id) return &p.modifiers[j];
        }
    }
    return nullptr;
}

const char* plugin_modifier_name(uint8_t id) {
    const ModifierDescriptor* d = plugin_find_modifier(id);
    return d ? d->name : nullptr;
}

const char* plugin_modifier_category(uint8_t id) {
    const ModifierDescriptor* d = plugin_find_modifier(id);
    return d ? d->category : nullptr;
}

const char* plugin_modifier_description(uint8_t id) {
    const ModifierDescriptor* d = plugin_find_modifier(id);
    return d ? d->description : nullptr;
}

// Простой парсер JSON-строки дескриптора плагина.
// Ожидаемый формат:
// {"name":"...","version":"...","author":"...","modifiers":[
//   {"id":64,"name":"...","category":"...","description":"...","release":false},
//   ...
// ]}
// Возвращает количество распарсенных модификаторов.
static int parse_plugin_descriptor(const char* json, PluginInfo* info) {
    // Ищет строковое значение по ключу: "key":"value" → извлекает value
    auto find_str = [](const char* haystack, const char* key, char* out, size_t out_sz) -> const char* {
        const char* pos = strstr(haystack, key);
        if (!pos) return nullptr;
        // Пропускаем ключ и двоеточие до открывающей кавычки значения
        pos = strchr(pos + strlen(key), ':');
        if (!pos) return nullptr;
        pos = strchr(pos, '"');
        if (!pos) return nullptr;
        pos++; // пропускаем открывающую кавычку
        size_t i = 0;
        while (*pos && *pos != '"' && i < out_sz - 1) out[i++] = *pos++;
        out[i] = '\0';
        return pos;
    };

    auto find_int = [](const char* haystack, const char* key, int* out) -> const char* {
        const char* pos = strstr(haystack, key);
        if (!pos) return nullptr;
        pos += strlen(key);
        pos = strchr(pos, ':');
        if (!pos) return nullptr;
        *out = (int)strtol(pos + 1, nullptr, 10);
        return pos;
    };

    auto find_bool = [](const char* haystack, const char* key, bool* out) -> const char* {
        const char* pos = strstr(haystack, key);
        if (!pos) return nullptr;
        pos += strlen(key);
        pos = strchr(pos, ':');
        if (!pos) return nullptr;
        while (*pos && (*pos == ':' || *pos == ' ' || *pos == '\t')) pos++;
        *out = (strncmp(pos, "true", 4) == 0);
        return pos;
    };

    find_str(json, "\"name\"", info->name, sizeof(info->name));
    find_str(json, "\"version\"", info->version, sizeof(info->version));
    find_str(json, "\"author\"", info->author, sizeof(info->author));

    const char* mods_start = strstr(json, "\"modifiers\"");
    if (!mods_start) { info->modifier_count = 0; return 0; }
    mods_start = strchr(mods_start, '[');
    if (!mods_start) { info->modifier_count = 0; return 0; }

    // Считаем количество объектов в массиве по "id":
    int count = 0;
    const char* scan = mods_start;
    const char* end = strchr(mods_start, ']');
    while ((scan = strstr(scan, "\"id\"")) != nullptr && scan < end) {
        count++;
        scan++;
    }
    if (count == 0) { info->modifier_count = 0; return 0; }
    if (count > MAX_MODIFIERS_PER_PLUGIN) count = MAX_MODIFIERS_PER_PLUGIN;

    // Парсим каждый модификатор в фиксированный массив
    const char* cur = mods_start;
    for (int i = 0; i < count; i++) {
        ModifierDescriptor& d = info->modifiers[i];
        cur = strstr(cur, "{");
        if (!cur) break;

        int id_int = 0;
        find_int(cur, "\"id\"", &id_int);
        d.id = (uint8_t)id_int;
        find_str(cur, "\"name\"", d.name, sizeof(d.name));
        find_str(cur, "\"category\"", d.category, sizeof(d.category));
        find_str(cur, "\"description\"", d.description, sizeof(d.description));
        bool rel = false;
        find_bool(cur, "\"release\"", &rel);
        d.is_release_modulator = rel ? 1 : 0;

        cur = strchr(cur, '}');
        if (cur) cur++;
    }

    info->modifier_count = count;
    return count;
}

int plugin_load(const char* so_path) {
    if (loaded_count >= MAX_PLUGINS) {
        DLOG_ERROR("[plugin] максимальное количество плагинов (%d)", MAX_PLUGINS);
        return -1;
    }

    void* handle = dlopen(so_path, RTLD_LAZY | RTLD_GLOBAL);  // GLOBAL: cross-plugin symbols visible
    if (!handle) {
        DLOG_ERROR("[plugin] dlopen: %s", dlerror());
        return -1;
    }

    // Функция инициализации плагина
    typedef int (*InitFunc)();
    InitFunc init = (InitFunc)dlsym(handle, "biocore_plugin_init");
    if (!init) {
        DLOG_ERROR("[plugin] %s: biocore_plugin_init не найден", so_path);
        dlclose(handle);
        return -2;
    }

    // Дескриптор плагина
    typedef const char* (*DescFunc)();
    DescFunc desc_func = (DescFunc)dlsym(handle, "biocore_plugin_desc");

    PluginInfo& info = loaded_plugins[loaded_count];
    memset(&info, 0, sizeof(PluginInfo));
    strncpy(info.filename, so_path, sizeof(info.filename) - 1);

    if (desc_func) {
        const char* json = desc_func();
        parse_plugin_descriptor(json, &info);
    }

    // Вызываем инициализацию — плагин регистрирует свои функции
    int registered = init();
    if (registered <= 0) {
        DLOG_ERROR("[plugin] %s: init вернул %d", so_path, registered);
        dlclose(handle);
        return -3;
    }

    if (info.modifier_count == 0) {
        info.modifier_count = (registered > MAX_MODIFIERS_PER_PLUGIN)
                              ? MAX_MODIFIERS_PER_PLUGIN : registered;
    }

    loaded_count++;
    // handle утекает намеренно — плагин должен оставаться загруженным
    return registered;
}

int plugin_load_builtin(const char* core_base_path) {
    // Если путь не указан, пробуем стандартные расположения
    if (!core_base_path || !core_base_path[0]) {
        // Пробуем plugins/builtin/ относительно текущей директории
        int total = plugin_load_directory("plugins/builtin");
        if (total > 0) return total;
        // Пробуем ../plugins/builtin/ (если запущены из build/)
        return plugin_load_directory("../plugins/builtin");
    }
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/plugins/builtin", core_base_path);
    return plugin_load_directory(full_path);
}

int plugin_load_directory(const char* dir_path) {
    // Убираем завершающие слеши для корректного snprintf ниже
    char clean_path[512];
    strncpy(clean_path, dir_path, sizeof(clean_path) - 1);
    clean_path[sizeof(clean_path) - 1] = '\0';
    size_t plen = strlen(clean_path);
    while (plen > 0 && clean_path[plen - 1] == '/') {
        clean_path[--plen] = '\0';
    }

    DIR* dir = opendir(clean_path);
    if (!dir) {
        DLOG_ERROR("[plugin] нет доступа к директории: %s", clean_path);
        return 0;
    }

    int total = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        const char* name = entry->d_name;
        size_t len = strlen(name);

        // Пропускаем не-.so файлы
        if (len < 4 || strcmp(name + len - 3, ".so") != 0) continue;

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", clean_path, name);

        struct stat st;
        if (stat(full_path, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        int n = plugin_load(full_path);
        if (n > 0) {
            DLOG_INFO("[plugin] загружен %s: %d модификаторов", name, n);
            total += n;
        }
    }
    closedir(dir);
    return total;
}

// ══════════════════════════════════════════════════════════════════════════════
// .info справочные файлы: загрузка и кэширование JSON
// ══════════════════════════════════════════════════════════════════════════════

#define INFO_CACHE_SIZE  256      // MOD_TYPE_COUNT=193, с запасом
#define INFO_MAX_BYTES   8192     // максимум байт на один .info файл
#define INFO_NAME_MAX    96       // имя модификатора (UTF-8, три языка)

static char  info_cache[INFO_CACHE_SIZE][INFO_MAX_BYTES];
static bool  info_loaded[INFO_CACHE_SIZE];  // был ли загружен .info для этого ID
static char  info_name[INFO_CACHE_SIZE][INFO_NAME_MAX];            // русское имя (name)
static char  info_name_en[INFO_CACHE_SIZE][INFO_NAME_MAX];         // английское (name_en)
static char  info_name_zh[INFO_CACHE_SIZE][INFO_NAME_MAX];         // китайское (name_zh)
static char  info_category[INFO_CACHE_SIZE][MAX_MOD_CATEGORY];     // категория
static char  info_description[INFO_CACHE_SIZE][MAX_MOD_DESCRIPTION]; // описание (рус.)
static char  info_description_en[INFO_CACHE_SIZE][MAX_MOD_DESCRIPTION]; // английское
static char  info_description_zh[INFO_CACHE_SIZE][MAX_MOD_DESCRIPTION]; // китайское

// Извлекает строковое значение по ключу из плоского JSON: "key":"value"
static void info_extract_str(const char* json, const char* key, char* out, size_t out_sz) {
    if (!json || !out || out_sz == 0) return;
    out[0] = '\0';
    const char* pos = strstr(json, key);
    if (!pos) return;
    pos = strchr(pos + strlen(key), ':');
    if (!pos) return;
    pos = strchr(pos, '"');
    if (!pos) return;
    pos++;
    size_t i = 0;
    while (*pos && *pos != '"' && i < out_sz - 1) out[i++] = *pos++;
    out[i] = '\0';
}

// Рекурсивный обход директории для поиска .info файлов.
// Принимает флаг recurse — если true, также сканирует поддиректории (builtin/ и др.).
static int load_info_from_dir(const char* dir_path, int* out_loaded) {
    DIR* dir = opendir(dir_path);
    if (!dir) return 0;

    int found = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        const char* name = entry->d_name;
        size_t len = strlen(name);

        // Рекурсивный вход в поддиректории (но не . и ..)
        if (entry->d_type == DT_DIR) {
            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
            char sub_path[640];
            snprintf(sub_path, sizeof(sub_path), "%s/%s", dir_path, name);
            found += load_info_from_dir(sub_path, out_loaded);
            continue;
        }

        // Ищем файлы вида mod_NN.info
        if (len < 9) continue;
        if (strncmp(name, "mod_", 4) != 0) continue;
        const char* ext = name + len - 5;
        if (strcmp(ext, ".info") != 0) continue;

        int tid = atoi(name + 4);
        if (tid < 0 || tid >= INFO_CACHE_SIZE) continue;

        char full_path[640];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, name);

        struct stat st;
        if (stat(full_path, &st) != 0 || !S_ISREG(st.st_mode)) continue;
        if (st.st_size >= INFO_MAX_BYTES) {
            DLOG_WARN("[info] %s слишком большой (%ld байт)", name, (long)st.st_size);
            continue;
        }

        FILE* f = fopen(full_path, "r");
        if (!f) continue;

        size_t nread = fread(info_cache[tid], 1, INFO_MAX_BYTES - 1, f);
        fclose(f);

        info_cache[tid][nread] = '\0';
        info_loaded[tid] = true;

        // Извлекаем имя/категорию/описание на трёх языках
        info_extract_str(info_cache[tid], "\"name\"",       info_name[tid],        INFO_NAME_MAX);
        info_extract_str(info_cache[tid], "\"name_en\"",    info_name_en[tid],     INFO_NAME_MAX);
        info_extract_str(info_cache[tid], "\"name_zh\"",    info_name_zh[tid],     INFO_NAME_MAX);
        info_extract_str(info_cache[tid], "\"category\"",   info_category[tid],    MAX_MOD_CATEGORY);
        info_extract_str(info_cache[tid], "\"description\"",info_description[tid], MAX_MOD_DESCRIPTION);
        info_extract_str(info_cache[tid], "\"description_en\"", info_description_en[tid], MAX_MOD_DESCRIPTION);
        info_extract_str(info_cache[tid], "\"description_zh\"", info_description_zh[tid], MAX_MOD_DESCRIPTION);

        (*out_loaded)++;
        found++;
        DLOG_INFO("[info] загружен %s (%zu байт)", name, nread);
    }
    closedir(dir);
    return found;
}

int plugin_load_info_directory(const char* dir_path) {
    // Очищаем кэш
    memset(info_cache, 0, sizeof(info_cache));
    memset(info_loaded, 0, sizeof(info_loaded));

    char clean_path[512];
    strncpy(clean_path, dir_path, sizeof(clean_path) - 1);
    clean_path[sizeof(clean_path) - 1] = '\0';

    // Убираем trailing slash
    size_t plen = strlen(clean_path);
    while (plen > 0 && (clean_path[plen - 1] == '/' || clean_path[plen - 1] == '\\')) {
        clean_path[--plen] = '\0';
    }

    int loaded = 0;
    load_info_from_dir(clean_path, &loaded);
    DLOG_INFO("[info] всего загружено %d справочных файлов", loaded);
    return loaded;
}

const char* plugin_get_info_json(uint8_t id) {
    if (id >= INFO_CACHE_SIZE || !info_loaded[id]) return nullptr;
    return info_cache[id];
}

// Имя/категория/описание модификатора по ID из .info справочника (три языка)
const char* plugin_info_name(uint8_t id) {
    if (id >= INFO_CACHE_SIZE || !info_loaded[id]) return nullptr;
    return info_name[id][0] ? info_name[id] : nullptr;
}

const char* plugin_info_name_en(uint8_t id) {
    if (id >= INFO_CACHE_SIZE || !info_loaded[id]) return nullptr;
    return info_name_en[id][0] ? info_name_en[id] : nullptr;
}

const char* plugin_info_name_zh(uint8_t id) {
    if (id >= INFO_CACHE_SIZE || !info_loaded[id]) return nullptr;
    return info_name_zh[id][0] ? info_name_zh[id] : nullptr;
}

const char* plugin_info_category(uint8_t id) {
    if (id >= INFO_CACHE_SIZE || !info_loaded[id]) return nullptr;
    return info_category[id][0] ? info_category[id] : nullptr;
}

const char* plugin_info_description(uint8_t id) {
    if (id >= INFO_CACHE_SIZE || !info_loaded[id]) return nullptr;
    return info_description[id][0] ? info_description[id] : nullptr;
}

const char* plugin_info_description_en(uint8_t id) {
    if (id >= INFO_CACHE_SIZE || !info_loaded[id]) return nullptr;
    return info_description_en[id][0] ? info_description_en[id] : nullptr;
}

const char* plugin_info_description_zh(uint8_t id) {
    if (id >= INFO_CACHE_SIZE || !info_loaded[id]) return nullptr;
    return info_description_zh[id][0] ? info_description_zh[id] : nullptr;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Гены (genes.info) — 46 генов консолидации на трёх языках.
// Плоский JSON: {"gene_count":46, "gN_name":.., "gN_name_en":.., "gN_description":.., ...}
// ═══════════════════════════════════════════════════════════════════════════════
#define GENE_COUNT_MAX  64
#define GENE_NAME_MAX   64
#define GENE_DESC_MAX   256

static char gene_name_ru[GENE_COUNT_MAX][GENE_NAME_MAX];
static char gene_name_en[GENE_COUNT_MAX][GENE_NAME_MAX];
static char gene_name_zh[GENE_COUNT_MAX][GENE_NAME_MAX];
static char gene_desc_ru[GENE_COUNT_MAX][GENE_DESC_MAX];
static char gene_desc_en[GENE_COUNT_MAX][GENE_DESC_MAX];
static char gene_desc_zh[GENE_COUNT_MAX][GENE_DESC_MAX];
static char gene_cat_ru[GENE_COUNT_MAX][GENE_NAME_MAX];
static char gene_cat_en[GENE_COUNT_MAX][GENE_NAME_MAX];
static char gene_cat_zh[GENE_COUNT_MAX][GENE_NAME_MAX];
static int  gene_loaded_count = 0;

int plugin_load_genes_info(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    static char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';
    gene_loaded_count = 0;
    char key[40];
    for (int i = 0; i < GENE_COUNT_MAX; i++) {
        snprintf(key, sizeof(key), "\"g%d_name_ru\"", i);
        if (!strstr(buf, key)) break;
        snprintf(key, sizeof(key), "\"g%d_name_ru\"", i);      info_extract_str(buf, key, gene_name_ru[i], GENE_NAME_MAX);
        snprintf(key, sizeof(key), "\"g%d_name_en\"", i);      info_extract_str(buf, key, gene_name_en[i], GENE_NAME_MAX);
        snprintf(key, sizeof(key), "\"g%d_name_zh\"", i);      info_extract_str(buf, key, gene_name_zh[i], GENE_NAME_MAX);
        snprintf(key, sizeof(key), "\"g%d_description\"", i);  info_extract_str(buf, key, gene_desc_ru[i], GENE_DESC_MAX);
        snprintf(key, sizeof(key), "\"g%d_description_en\"", i); info_extract_str(buf, key, gene_desc_en[i], GENE_DESC_MAX);
        snprintf(key, sizeof(key), "\"g%d_description_zh\"", i); info_extract_str(buf, key, gene_desc_zh[i], GENE_DESC_MAX);
        snprintf(key, sizeof(key), "\"g%d_category\"", i);    info_extract_str(buf, key, gene_cat_ru[i], GENE_NAME_MAX);
        snprintf(key, sizeof(key), "\"g%d_category_en\"", i); info_extract_str(buf, key, gene_cat_en[i], GENE_NAME_MAX);
        snprintf(key, sizeof(key), "\"g%d_category_zh\"", i); info_extract_str(buf, key, gene_cat_zh[i], GENE_NAME_MAX);
        gene_loaded_count++;
    }
    return gene_loaded_count;
}

const char* plugin_gene_name(uint8_t id, uint8_t lang) {
    if (id >= (uint8_t)gene_loaded_count) return nullptr;
    return lang == 1 ? gene_name_en[id] : (lang == 2 ? gene_name_zh[id] : gene_name_ru[id]);
}

const char* plugin_gene_description(uint8_t id, uint8_t lang) {
    if (id >= (uint8_t)gene_loaded_count) return nullptr;
    return lang == 1 ? gene_desc_en[id] : (lang == 2 ? gene_desc_zh[id] : gene_desc_ru[id]);
}

const char* plugin_gene_category(uint8_t id, uint8_t lang) {
    if (id >= (uint8_t)gene_loaded_count) return nullptr;
    return lang == 1 ? gene_cat_en[id] : (lang == 2 ? gene_cat_zh[id] : gene_cat_ru[id]);
}

int plugin_gene_count() { return gene_loaded_count; }
