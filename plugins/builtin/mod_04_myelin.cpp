// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// myelin.cpp — Миелиновая оболочка
// Тип: MOD_MYELIN (4)
// Категория: Базовые модификаторы
// Автогенерация: generate_builtin_plugins.py

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double myelin_current_impl(Modifier* m, Segment* s, float dt) {
    (void)m; (void)s; (void)dt;
    return 0.0f;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_MYELIN, myelin_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"Миелиновая оболочка","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":4,"name":"Миелиновая оболочка","category":"Базовые модификаторы","description":"Пассивный миелиновый модулятор, блокирует ионотропные токи на сегменте","release":false}]})BIOCORE";
}
