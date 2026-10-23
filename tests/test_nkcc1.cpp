// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_nkcc1.cpp — верификация NKCC1 (MOD_NKCC1, 122) против Na⁺-K⁺-2Cl⁻ котранспортёра.
// Delpire & Gagnon (2011): электронейтральный 1 Na⁺ : 1 K⁺ : 2 Cl⁻ внутрь, Cl⁻-загрузчик.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double nkcc1_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float Cl_i, float K_o) {
    Segment s = {};
    s.V_m = -70.0f;
    s.intra_ions[ION_NA] = 15.0f;  s.extra_ions[ION_NA] = 140.0f;   // мМ
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K] = K_o;
    s.intra_ions[ION_CL] = Cl_i;   s.extra_ions[ION_CL] = 130.0f;
    s.intra_ions[ION_CA] = 0.0001f; s.extra_ions[ION_CA] = 2.0f;
    s.ATP_level = 1.0f;
    s.radius = 5.0f; s.length = 10.0f; s.segment_type = 1;   // сома
    s.modifiers[0].type = MOD_NKCC1;
    s.modifiers[0].density = 0.8f;
    s.modifier_count = 1;
    return s;
}

TEST_CASE("NKCC1: электронейтральность — return 0", "[nkcc1]") {
    Segment s = make_seg(25.0f, 4.0f);
    float I = nkcc1_current(&s.modifiers[0], &s, 1.0f);
    REQUIRE(I == 0.0f);
}

TEST_CASE("NKCC1: транспорт 1 Na⁺ : 1 K⁺ : 2 Cl⁻ внутрь", "[nkcc1]") {
    Segment s = make_seg(25.0f, 4.0f);
    float Na_i0 = s.intra_ions[ION_NA];
    float K_i0  = s.intra_ions[ION_K];
    float Cl_i0 = s.intra_ions[ION_CL];

    nkcc1_current(&s.modifiers[0], &s, 100.0f);

    float dNa_i = s.intra_ions[ION_NA] - Na_i0;   // > 0 (Na внутрь)
    float dK_i  = s.intra_ions[ION_K]  - K_i0;    // > 0
    float dCl_i = s.intra_ions[ION_CL] - Cl_i0;   // > 0
    INFO("dNa=" << dNa_i << " dK=" << dK_i << " dCl=" << dCl_i);
    REQUIRE(dNa_i > 0.0f);
    REQUIRE(dK_i  > 0.0f);
    REQUIRE(dCl_i > 0.0f);
    // 1:1:2 → |ΔNa| = |ΔK| = |ΔCl|/2
    REQUIRE(dNa_i == Approx(dK_i).margin(1e-4f));
    REQUIRE(dCl_i == Approx(2.0f * dNa_i).margin(1e-4f));
}

TEST_CASE("NKCC1: K_m(Cl⁻_i)=50мМ — ниже сродство чем KCC2", "[nkcc1]") {
    Segment s_lo = make_seg(5.0f, 4.0f);
    Segment s_hi = make_seg(50.0f, 4.0f);
    float Cl0_lo = s_lo.intra_ions[ION_CL];
    float Cl0_hi = s_hi.intra_ions[ION_CL];

    nkcc1_current(&s_lo.modifiers[0], &s_lo, 100.0f);
    nkcc1_current(&s_hi.modifiers[0], &s_hi, 100.0f);

    float d_lo = s_lo.intra_ions[ION_CL] - Cl0_lo;
    float d_hi = s_hi.intra_ions[ION_CL] - Cl0_hi;
    INFO("dCl(Cl=5)=" << d_lo << " dCl(Cl=50)=" << d_hi);
    REQUIRE(d_hi > d_lo);   // выше Cl_i → больше загрузка
}

TEST_CASE("NKCC1: транс-ингибирование внешним K⁺", "[nkcc1]") {
    Segment s_lowKo = make_seg(25.0f, 1.0f);
    Segment s_hiKo  = make_seg(25.0f, 10.0f);
    float Cl0_low = s_lowKo.intra_ions[ION_CL];
    float Cl0_hi  = s_hiKo.intra_ions[ION_CL];

    nkcc1_current(&s_lowKo.modifiers[0], &s_lowKo, 100.0f);
    nkcc1_current(&s_hiKo.modifiers[0],  &s_hiKo,  100.0f);

    float d_low = s_lowKo.intra_ions[ION_CL] - Cl0_low;
    float d_hi  = s_hiKo.intra_ions[ION_CL]  - Cl0_hi;
    INFO("dCl(K_o=1)=" << d_low << " dCl(K_o=10)=" << d_hi);
    REQUIRE(d_low > d_hi);   // высокий K_o ингибирует
}
