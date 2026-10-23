// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_abi_segment_layout.cpp — проверка ABI-совместимости offset'ов Segment и BionData
// между GCC, nvcc и icpx. Критично для бинарной загрузки .bion и GPU↔CPU обмена.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstddef>
#include "types.h"
#include "bion_loader.h"

TEST_CASE("Segment ABI layout", "[abi]") {
    // v14: Modifier double + I_buffer/post_receptors 8-aligned, Segment 3208→3216
    CHECK(sizeof(Segment) == 3216);

    // Критичные поля — используются в raw-доступе (test_cpg_rhythm, kernels)
    CHECK(offsetof(Segment, V_m)            == 40);
    CHECK(offsetof(Segment, I_buffer)       == 3128);
    CHECK(offsetof(Segment, intra_ions)     == 48);
    CHECK(offsetof(Segment, extra_ions)     == 112);
    CHECK(offsetof(Segment, segment_type)   == 3113);
    CHECK(offsetof(Segment, modifier_count) == 3112);

    // Проверяем, что поля не пересекаются
    CHECK(offsetof(Segment, intra_ions) + sizeof(double)*ION_COUNT <= offsetof(Segment, extra_ions));
    CHECK(offsetof(Segment, extra_ions) + sizeof(double)*ION_COUNT <= offsetof(Segment, intra_nt));
    CHECK(offsetof(Segment, modifiers) + sizeof(Modifier)*MAX_MODIFIERS_PER_SEGMENT == offsetof(Segment, modifier_count));
}

TEST_CASE("BionData ABI layout", "[abi]") {
    // Указатели — зависят от платформы (8 байт на 64-bit)
    CHECK(offsetof(BionData, header)        == 0);
    CHECK(offsetof(BionData, segments)      == 112);
    CHECK(offsetof(BionData, num_segments)  == 148);

    // header должен быть первым (offset 0)
    CHECK(offsetof(BionData, header) == 0);
}

TEST_CASE("Modifier and other struct sizes", "[abi]") {
    CHECK(sizeof(Modifier)        == 88);
    CHECK(sizeof(Synapse)         == 1568);
    CHECK(sizeof(IntraConnection) == 16);
    CHECK(sizeof(Neuron)          == 24);
    CHECK(sizeof(Nucleus)         == 500);
    CHECK(sizeof(NetworkHeader)   == 100);  // v10: +file_sz_* fields
}

TEST_CASE("ION_COUNT and NT_COUNT", "[abi]") {
    // Критично для расчёта размеров intra_ions/extra_ions/intra_nt/neurotransmitter
    CHECK(ION_COUNT == 7);
    CHECK(NT_COUNT  == 15);
    CHECK(MAX_MODIFIERS_PER_SEGMENT == 32);
    CHECK(MAX_STATE_VARS == 8);
}
