// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// mod_136_plc_ip3_dag.cpp — Gq→PLC→IP3+DAG каскад (общий пул на сегмент)
// Тип: MOD_PLC_IP3_DAG (136)
//
// Gq-рецепторы (5-HT2A/2B/2C, α1A/α1B, M1/M3/M5, mGluR1/5, H1) стимулируют PLC.
// PLC гидролизует PIP2 → IP3 + DAG (1:1). IP3→IP3R (Ca²⁺ из ЭПР), DAG→PKC.
// state[0]=IP3 (мМ), [1]=DAG (норм.), [2]=desens, [3..4]=overrides (Vmax IP3/DAG).

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double plc_ip3_dag_current_impl(Modifier* m, Segment* s, float dt) {
    return plc_ip3_dag_step(m, s, dt);  // единая реализация — modifier_helpers.h
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_PLC_IP3_DAG, plc_ip3_dag_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"PLC→IP3+DAG каскад","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":136,"name":"PLC→IP3+DAG (Gq)","category":"Сигнальные каскады","description":"Общий пул IP3+DAG. Gq→PLC→IP3+DAG (1:1). IP3 открывает IP3R (Ca²⁺ из ЭПР), DAG активирует PKC. state[0]=IP3,[1]=DAG,[2]=desens.","release":false}]})BIOCORE";
}
