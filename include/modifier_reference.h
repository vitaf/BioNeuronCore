// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#pragma once
#include "types.h"

// ── Подробный справочник модификатора (биологический + технический) ──────
// Используется для хелп-панели в Studio и как документация разработчика.
// Vitis HLS: фиксированные массивы, без динамической памяти.

#define MAX_REF_NAME            64
#define MAX_REF_CATEGORY        64
#define MAX_REF_VERSION         16
#define MAX_REF_MECHANISM       48
#define MAX_REF_SIGNALING       256
#define MAX_REF_INPUTS          192
#define MAX_REF_OUTPUTS         192
#define MAX_REF_NEURON_TYPES    192
#define MAX_REF_SUBCELLULAR     192
#define MAX_REF_KINETICS        384
#define MAX_REF_FUNCTION        384
#define MAX_REF_REFERENCES      256

struct ModifierReference {
    uint8_t  id;
    char     name[MAX_REF_NAME];
    char     category[MAX_REF_CATEGORY];
    char     version[MAX_REF_VERSION];
    char     mechanism_type[MAX_REF_MECHANISM];  // Ионотропный / Метаботропный Gi/o / Модулятор / Сома
    char     signaling[MAX_REF_SIGNALING];       // G-белок, ионный поток, ферментативный каскад
    char     inputs[MAX_REF_INPUTS];             // Что читает: V_m, ионы, нейромедиаторы, ATP
    char     outputs[MAX_REF_OUTPUTS];           // Что пишет: ток, модуляция, тепло, транскрипция
    char     neuron_types[MAX_REF_NEURON_TYPES]; // В каких нейронах встречается
    char     subcellular[MAX_REF_SUBCELLULAR];   // Сома, дендрит, шипик, аксон, пре/постсинапс
    char     kinetics[MAX_REF_KINETICS];         // ODE, состояния, константы
    char     function[MAX_REF_FUNCTION];         // Биологическая роль
    char     references[MAX_REF_REFERENCES];     // DOI, публикации
};

// Получить справочную информацию о модификаторе по ID (0..79)
const ModifierReference* modifier_reference_get(uint8_t id);

// Количество записей в справочнике
int modifier_reference_count();
