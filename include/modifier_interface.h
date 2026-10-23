// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#pragma once
#include "types.h"

// Сигнатура функции модификатора: возвращает ток (положительный = деполяризация)
typedef double (*ModifierFunc)(Modifier* mod, Segment* seg, float dt);

// Сигнатура пресинаптического модулятора: модифицирует cleft_neurotransmitter
typedef float (*ReleaseModulatorFunc)(Modifier* mod, float* cleft_nt, float dt);

// ── Таблицы функций для CPU-пути (function pointers) ──────────────────
// Заполняются плагинами при загрузке. GPU/SYCL использует switch-диспетчер.
#define MOD_TABLE_SIZE   128   // полный диапазон ID (0-127)

// Таблица токов модификаторов (заполняется плагинами)
extern ModifierFunc modifier_current_table[MOD_TABLE_SIZE];

// Таблица пресинаптических модуляторов (заполняется плагинами)
extern ReleaseModulatorFunc modulator_release_table[MOD_TABLE_SIZE];

// ── Встроенные библиотеки (скомпилированы в ядро для GPU/SYCL) ─────────
// Используются GPU-путём через switch-диспетчер
extern ModifierFunc biological_library[MOD_TYPE_COUNT];
extern ModifierFunc arbitrary_library[MOD_TYPE_COUNT];
extern ReleaseModulatorFunc biological_modulators[MOD_TYPE_COUNT];
extern ReleaseModulatorFunc arbitrary_modulators[MOD_TYPE_COUNT];

// Активные библиотеки (переключаются BIO/ARB)
extern ModifierFunc* active_library;
extern ReleaseModulatorFunc* active_modulators;

// ── Регистрация функций (вызывается плагинами при biocore_plugin_init) ─
// CPU-путь: регистрирует в modifier_current_table / modulator_release_table
// extern "C" — обязательно для dlopen: плагины ищут эти символы в хосте
extern "C" void register_modifier_current(uint8_t type, ModifierFunc func);
extern "C" void register_modulator_release(uint8_t type, ReleaseModulatorFunc func);

// ── Старые API для обратной совместимости (GPU/SYCL) ───────────────────
#define MOD_USER_BASE     0     // теперь весь диапазон плагинный
#define MOD_USER_MAX      127
#define MOD_USER_COUNT    (MOD_USER_MAX - MOD_USER_BASE + 1)

// Таблицы пользовательских функций (CPU-only, заполняются плагинами)
extern ModifierFunc user_library[MOD_USER_COUNT];
extern ReleaseModulatorFunc user_modulators[MOD_USER_COUNT];

// ── Инициализация ──────────────────────────────────────────────────────
void init_biological_library();
void init_arbitrary_library();

// Установка пользовательских функций для ARB-режима (GPU/SYCL-совместимо)
void set_arbitrary_modulator(int type, ModifierFunc func);
void set_arbitrary_modulator_release(int type, ReleaseModulatorFunc func);
