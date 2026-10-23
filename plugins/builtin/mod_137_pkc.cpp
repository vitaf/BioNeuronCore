// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_137_pkc.cpp — PKC киназа (DAG + Ca²⁺)
// Тип: MOD_PKC (137)
//
// Классическая PKC (α/β/γ): DAG (C1-домен) + Ca²⁺ (C2-домен) → активность.
// Фосфорилирует мишени (напр., Kv7/KCNQ M-ток → подавление).
// state[0]=PKC-активность (0-1).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double pkc_current_impl(Modifier* m, Segment* s, float dt) {
    return pkc_step(m, s, dt);  // единая реализация — modifier_helpers.h
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_PKC, pkc_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"PKC киназа","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":137,"name":"PKC (DAG+Ca²⁺)","category":"Сигнальные каскады","description":"Классическая PKC: DAG (C1) + Ca²⁺ (C2) → активность. Фосфорилирует Kv7/KCNQ → подавление M-тока. state[0]=активность.","release":false}]})BIOCORE";
}
