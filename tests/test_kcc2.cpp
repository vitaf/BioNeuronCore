// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_kcc2.cpp — верификация KCC2 (MOD_KCC2, 100) против K⁺-Cl⁻ котранспортёра.
// Payne (1997), Rivera et al. (1999): электронейтральный 1 K⁺ : 1 Cl⁻ наружу,
// K_m(K⁺_i)≈6.5мМ, K_m(Cl⁻_i)≈6.5мМ, транс-ингибирование внешним K⁺.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double kcc2_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float Cl_i, float K_o) {
    Segment s = {};
    s.V_m = -70.0f;
    s.intra_ions[ION_K]  = 140.0f;  s.extra_ions[ION_K] = K_o;   // мМ
    s.intra_ions[ION_CL] = Cl_i;    s.extra_ions[ION_CL] = 130.0f;
    s.intra_ions[ION_NA] = 15.0f;   s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_CA] = 0.0001f; s.extra_ions[ION_CA] = 2.0f;
    s.ATP_level = 1.0f;
    s.radius = 5.0f; s.length = 10.0f; s.segment_type = 1;   // сома
    s.modifiers[0].type = MOD_KCC2;
    s.modifiers[0].density = 0.8f;
    s.modifier_count = 1;
    return s;
}

TEST_CASE("KCC2: электронейтральность — return 0 (нет тока)", "[kcc2]") {
    Segment s = make_seg(5.0f, 4.0f);
    float I = kcc2_current(&s.modifiers[0], &s, 1.0f);
    REQUIRE(I == 0.0f);   // 1 K⁺ : 1 Cl⁻, нетто заряд 0
}

TEST_CASE("KCC2: транспорт 1 K⁺ : 1 Cl⁻ наружу", "[kcc2]") {
    Segment s = make_seg(5.0f, 4.0f);
    float K_i0  = s.intra_ions[ION_K];
    float Cl_i0 = s.intra_ions[ION_CL];
    float K_o0  = s.extra_ions[ION_K];

    kcc2_current(&s.modifiers[0], &s, 100.0f);

    float dK_i  = s.intra_ions[ION_K]  - K_i0;   // < 0 (K наружу)
    float dCl_i = s.intra_ions[ION_CL] - Cl_i0;  // < 0 (Cl наружу)
    float dK_o  = s.extra_ions[ION_K]  - K_o0;   // > 0
    INFO("dK_i=" << dK_i << " dCl_i=" << dCl_i << " dK_o=" << dK_o);
    REQUIRE(dK_i  < 0.0f);
    REQUIRE(dCl_i < 0.0f);
    REQUIRE(dK_o  > 0.0f);
    // Электронейтральный 1:1 → |ΔK| = |ΔCl|
    REQUIRE(fabsf(dK_i) == Approx(fabsf(dCl_i)).margin(1e-4f));
}

TEST_CASE("KCC2: термодинамика — ниже равновесия вводит Cl, выше — выводит", "[kcc2]") {
    // Равновесие электронейтрального 1:1 котранспорта: [K]i[Cl]i = [K]o[Cl]o
    // → Cl_i_eq = 4·130/140 = 3.71 мМ.
    Segment s_lo = make_seg(2.0f, 4.0f);   // Cl_i=2 < 3.71 → вводит Cl (двигается к равновесию)
    Segment s_hi = make_seg(15.0f, 4.0f);  // Cl_i=15 > 3.71 → выводит Cl
    float Cl0_lo = s_lo.intra_ions[ION_CL];
    float Cl0_hi = s_hi.intra_ions[ION_CL];

    kcc2_current(&s_lo.modifiers[0], &s_lo, 100.0f);
    kcc2_current(&s_hi.modifiers[0], &s_hi, 100.0f);

    float d_lo = s_lo.intra_ions[ION_CL] - Cl0_lo;  // >0 (вводит)
    float d_hi = s_hi.intra_ions[ION_CL] - Cl0_hi;  // <0 (выводит)
    INFO("dCl(Cl=2)=" << d_lo << " dCl(Cl=15)=" << d_hi);
    REQUIRE(d_lo > 0.0f);
    REQUIRE(d_hi < 0.0f);
}

TEST_CASE("KCC2: высокий внеклеточный K⁺ тормозит выброс (термодинамика)", "[kcc2]") {
    Segment s_lowKo = make_seg(10.0f, 1.0f);    // низкий K_o → движущая сила больше
    Segment s_hiKo  = make_seg(10.0f, 10.0f);   // высокий K_o → движущая сила меньше
    float Cl0_low = s_lowKo.intra_ions[ION_CL];
    float Cl0_hi  = s_hiKo.intra_ions[ION_CL];

    kcc2_current(&s_lowKo.modifiers[0], &s_lowKo, 100.0f);
    kcc2_current(&s_hiKo.modifiers[0],  &s_hiKo,  100.0f);

    float d_low = Cl0_low - s_lowKo.intra_ions[ION_CL];
    float d_hi  = Cl0_hi  - s_hiKo.intra_ions[ION_CL];
    INFO("dCl(K_o=1)=" << d_low << " dCl(K_o=10)=" << d_hi);
    REQUIRE(d_low > d_hi);   // высокий K_o → меньше выброс
}
