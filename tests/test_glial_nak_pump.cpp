// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_glial_nak_pump.cpp — верификация глиальной Na/K-АТФазы (MOD_GLIAL_NAK_PUMP, 110).
// Тот же цикл Поста–Альберса, что α3 (нейрональный), но α2-параметры:
// тот же k_cat=75 с⁻¹, K_m(K⁺_o)=1.0 мМ (выше сродство). Различие α2/α3 — плотность (клетка).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double glial_nak_pump_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V, float Na_i, float K_o, float ATP) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_NA] = Na_i;  s.extra_ions[ION_NA] = 140.0f;   // мМ
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K] = K_o;
    s.intra_ions[ION_CA] = 0.0001f; s.extra_ions[ION_CA] = 2.0f;
    s.intra_ions[ION_CL] = 5.0f; s.extra_ions[ION_CL] = 130.0f;
    s.ATP_level = ATP;
    s.radius = 5.0f; s.length = 10.0f; s.segment_type = 1;   // сома
    s.modifiers[0].type = MOD_GLIAL_NAK_PUMP;
    s.modifiers[0].density = 1000.0f;   // молекул/µm²
    s.modifier_count = 1;
    return s;
}

TEST_CASE("GLIAL_NAK_PUMP: ток гиперполяризующий (return < 0)", "[glial_pump]") {
    Segment s = make_seg(-70.0f, 15.0f, 5.0f, 1.0f);
    float I = glial_nak_pump_current(&s.modifiers[0], &s, 0.1f);
    INFO("I_glial=" << I);
    REQUIRE(I < 0.0f);   // электрогенный +1 наружу → гиперполяризация
}

TEST_CASE("GLIAL_NAK_PUMP: стехиометрия 3 Na⁺ : 2 K⁺", "[glial_pump]") {
    Segment s = make_seg(-70.0f, 15.0f, 5.0f, 1.0f);
    float Na_i0 = s.intra_ions[ION_NA];
    float K_i0  = s.intra_ions[ION_K];

    glial_nak_pump_current(&s.modifiers[0], &s, 1000.0f);

    float dNa_i = s.intra_ions[ION_NA] - Na_i0;   // < 0 (Na наружу)
    float dK_i  = s.intra_ions[ION_K]  - K_i0;    // > 0 (K внутрь)
    INFO("dNa_i=" << dNa_i << " dK_i=" << dK_i);
    REQUIRE(dNa_i < 0.0f);
    REQUIRE(dK_i  > 0.0f);
    REQUIRE(fabsf(dNa_i) == Approx(1.5f * fabsf(dK_i)).margin(1e-4f));   // 3:2
}

TEST_CASE("GLIAL_NAK_PUMP: V-зависимость (деполяризация ускоряет)", "[glial_pump]") {
    float v_lo  = nak_pump_vnorm(-120.0f, 15.0f, 5.0f, 1.0f, NA_K_PUMP_K_NA_MM, NA_K_PUMP_GLIAL_K_K_MM);
    float v_hi  = nak_pump_vnorm( 50.0f, 15.0f, 5.0f, 1.0f, NA_K_PUMP_K_NA_MM, NA_K_PUMP_GLIAL_K_K_MM);
    INFO("v(-120)=" << v_lo << " v(+50)=" << v_hi);
    REQUIRE(v_hi > v_lo);
}

TEST_CASE("GLIAL_NAK_PUMP: K_m(K⁺_o)=1.0 выше сродство, чем α3", "[glial_pump]") {
    // При K_o=1.0 мМ (равном K_m) активность ~ половина от насыщения.
    float v_at_km = nak_pump_vnorm(-70.0f, 15.0f, 1.0f, 1.0f, NA_K_PUMP_K_NA_MM, NA_K_PUMP_GLIAL_K_K_MM);
    float v_sat   = nak_pump_vnorm(-70.0f, 15.0f, 10.0f, 1.0f, NA_K_PUMP_K_NA_MM, NA_K_PUMP_GLIAL_K_K_MM);
    INFO("v(K=1)=" << v_at_km << " v(K=10)=" << v_sat);
    REQUIRE(v_sat > v_at_km);
}
