// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#pragma once
#include "types.h"
#include "modifier_interface.h"

// ── Лимиты плагинной системы ───────────────────────────────────────────
#define MAX_PLUGINS             256   // максимум загруженных .so плагинов
#define MAX_MODIFIERS_PER_PLUGIN 16    // максимум модификаторов в одном плагине
#define MAX_PLUGIN_FILENAME     256
#define MAX_PLUGIN_NAME         128
#define MAX_PLUGIN_VERSION      32
#define MAX_PLUGIN_AUTHOR       128
#define MAX_MOD_DESCRIPTION     256
#define MAX_MOD_CATEGORY        64    // UTF-8 русский текст требует больше байт
#define MAX_MOD_NAME            64

// Описание одного модификатора из плагина
// Vitis HLS: фиксированные массивы, без динамической памяти
struct ModifierDescriptor {
    uint8_t  id;            // 0–127
    char     name[MAX_MOD_NAME];
    char     category[MAX_MOD_CATEGORY];
    char     description[MAX_MOD_DESCRIPTION];
    uint8_t  is_release_modulator;  // 1 = пресинаптический модулятор
    uint8_t  _reserved[3];
};

// Информация о загруженном плагине
// Vitis HLS: фиксированные массивы, без std::string
struct PluginInfo {
    char     filename[MAX_PLUGIN_FILENAME];
    char     name[MAX_PLUGIN_NAME];
    char     version[MAX_PLUGIN_VERSION];
    char     author[MAX_PLUGIN_AUTHOR];
    int32_t  modifier_count;
    ModifierDescriptor modifiers[MAX_MODIFIERS_PER_PLUGIN];  // фикс. массив
    uint32_t _reserved[4];
};

// Инициализация менеджера плагинов (заполняет таблицы нулями)
void plugin_manager_init();

// Загрузка одного плагина из .so файла
// Возвращает количество зарегистрированных модификаторов, <0 при ошибке
int  plugin_load(const char* so_path);

// Загрузка всех плагинов из директории
int  plugin_load_directory(const char* dir_path);

// Загрузка встроенных плагинов из директории (обычно plugins/builtin/)
int  plugin_load_builtin(const char* core_base_path);

// Получить количество загруженных плагинов
int  plugin_get_count();

// Получить плагин по индексу (возвращает nullptr если выход за границы)
const PluginInfo* plugin_get(int idx);

// Получить список загруженных плагинов (копирует в буфер)
int  plugin_get_all(PluginInfo* out_buf, int buf_capacity);

// Поиск модификатора по ID среди загруженных плагинов
const ModifierDescriptor* plugin_find_modifier(uint8_t id);

// Получить описание модификатора по ID (возвращает nullptr если не найден)
const char* plugin_modifier_name(uint8_t id);
const char* plugin_modifier_category(uint8_t id);
const char* plugin_modifier_description(uint8_t id);

// ── .info справочные файлы ────────────────────────────────────────────
// Загрузка всех .info JSON-файлов из директории (обычно plugins/builtin/)
int  plugin_load_info_directory(const char* dir_path);

// Получить сырой JSON .info файла для модификатора по ID
// Возвращает nullptr если не загружен
const char* plugin_get_info_json(uint8_t id);

// Имя/категория/описание модификатора по ID из .info справочника (три языка)
const char* plugin_info_name(uint8_t id);
const char* plugin_info_name_en(uint8_t id);
const char* plugin_info_name_zh(uint8_t id);
const char* plugin_info_category(uint8_t id);
const char* plugin_info_description(uint8_t id);
const char* plugin_info_description_en(uint8_t id);
const char* plugin_info_description_zh(uint8_t id);

// Гены (genes.info) — трёхъязычный справочник
int         plugin_load_genes_info(const char* path);
const char* plugin_gene_name(uint8_t id, uint8_t lang);        // lang: 0=ru,1=en,2=zh
const char* plugin_gene_description(uint8_t id, uint8_t lang);
const char* plugin_gene_category(uint8_t id, uint8_t lang);
int         plugin_gene_count();

// Регистрация пользовательских функций (вызывается из плагинов)
void register_user_modifier(uint8_t type, ModifierFunc func);
void register_user_modulator(uint8_t type, ReleaseModulatorFunc func);
