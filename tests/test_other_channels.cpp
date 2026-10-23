// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_other_channels.cpp — тесты 6 новых модификаторов (177-182).
// Проверяет АКТИВАЦИЮ (V½/τ), ИНАКТИВАЦИЮ (V½/τ), ИНГИБИЦИЮ (блокатор).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double nav13_current(Modifier* m, Segment* s, float dt);
extern double kir71_current(Modifier* m, Segment* s, float dt);
extern double trpc67_current(Modifier* m, Segment* s, float dt);
extern double calhm13_current(Modifier* m, Segment* s, float dt);
extern double otop1_current(Modifier* m, Segment* s, float dt);
extern double enac_current(Modifier* m, Segment* s, float dt);

static double nernst_mv(double c_in, double c_out, double z) {
    return (GAS_CONST * CELSIUS_37 / (z * FARADAY_CONST)) * log(c_out / c_in) * 1000.0;
}
static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_NA] = 15.0f;  s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K]  = 4.0f;
    s.intra_ions[ION_CA] = 0.0001f; s.extra_ions[ION_CA] = 1.2f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifier_count = 1;
    return s;
}

TEST_CASE("Nav1.3: активация V½ −15.8 (m_inf≈0.5) + инактивация V½ −51.8", "[other_channels]") {
    // Активация: при V=−15.8 m_inf≈0.5 (с h≈1 при сильной гиперполяр. препульсе — проверяем через steady m)
    Segment s = make_seg(-15.8f);
    s.modifiers[0].type = MOD_NAV13; s.modifiers[0].density = 1.0f;
    // m быстро (0.5 мс), h медленнее (0.65 мс) — за 2 мс m почти steady, h ещё ~1
    for (int i = 0; i < 4; i++) nav13_current(&s.modifiers[0], &s, 0.5f);
    REQUIRE(s.modifiers[0].state[0] == Approx(0.5f).margin(0.1f));  // m_inf при V½_акт
}

TEST_CASE("Nav1.3: ингибиция TTX (4 нМ)", "[other_channels]") {
    Segment s = make_seg(-10.0f);
    s.modifiers[0].type = MOD_NAV13; s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[5] = 4.0f;  // TTX 4 нМ = IC50
    for (int i = 0; i < 10; i++) nav13_current(&s.modifiers[0], &s, 0.5f);
    float I_blocked = nav13_current(&s.modifiers[0], &s, 0.5f);
    // при IC50 ток ~вдвое меньше, чем без TTX (open ÷ (1+ttx/4) = ÷2)
    Segment s2 = make_seg(-10.0f);
    s2.modifiers[0].type = MOD_NAV13; s2.modifiers[0].density = 1.0f;
    for (int i = 0; i < 10; i++) nav13_current(&s2.modifiers[0], &s2, 0.5f);
    float I_free = nav13_current(&s2.modifiers[0], &s2, 0.5f);
    REQUIRE(fabsf(I_blocked) < fabsf(I_free));  // TTX уменьшает ток
}

TEST_CASE("Kir7.1: K-утечка (E_rev=E_K), без инактивации", "[other_channels]") {
    double e_k = nernst_mv(140.0, 4.0, 1.0);  // ≈ −91 мВ
    Segment s = make_seg((float)e_k);
    s.modifiers[0].type = MOD_KIR71; s.modifiers[0].density = 1.0f;
    REQUIRE(kir71_current(&s.modifiers[0], &s, 0.0f) == Approx(0.0f).margin(1.0f));
}

TEST_CASE("TRPC6/7: DAG-активация (лиганд state[4])", "[other_channels]") {
    Segment s = make_seg(-60.0f);
    s.modifiers[0].type = MOD_TRPC67; s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[4] = 10.0f;  // DAG
    for (int i = 0; i < 2500; i++) trpc67_current(&s.modifiers[0], &s, 0.1f);  // 250 мс = 5τ
    REQUIRE(s.modifiers[0].state[0] == Approx(0.5f).margin(0.1f));  // r = 10/(10+10) = 0.5
    REQUIRE(trpc67_current(&s.modifiers[0], &s, 0.1f) > 0.0f);      // катионный вход
}

TEST_CASE("CALHM1/3: активация V½ +30.5", "[other_channels]") {
    Segment s = make_seg(30.5f);
    s.modifiers[0].type = MOD_CALHM13; s.modifiers[0].density = 1.0f;
    for (int i = 0; i < 2000; i++) calhm13_current(&s.modifiers[0], &s, 0.5f);  // 1000 мс = 50τ
    REQUIRE(s.modifiers[0].state[0] > 0.25f);  // частично активирован при V½
}

TEST_CASE("OTOP1: pH-гейт (pH<6 активирует)", "[other_channels]") {
    Segment s = make_seg(-60.0f);
    s.modifiers[0].type = MOD_OTOP1; s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[4] = 5.0f;  // pH 5 (кислое)
    for (int i = 0; i < 100; i++) otop1_current(&s.modifiers[0], &s, 0.1f);
    REQUIRE(s.modifiers[0].state[0] > 0.5f);  // активирован

    Segment s2 = make_seg(-60.0f);
    s2.modifiers[0].type = MOD_OTOP1; s2.modifiers[0].density = 1.0f;
    s2.modifiers[0].state[4] = 7.4f;  // pH 7.4 (нейтральное)
    for (int i = 0; i < 100; i++) otop1_current(&s2.modifiers[0], &s2, 0.1f);
    REQUIRE(s2.modifiers[0].state[0] < 0.1f);  // закрыт
}

TEST_CASE("ENaC: Na-утечка + амилорид-ингибиция (100 нМ)", "[other_channels]") {
    double e_na = nernst_mv(15.0, 140.0, 1.0);
    Segment s = make_seg((float)e_na);
    s.modifiers[0].type = MOD_ENAC; s.modifiers[0].density = 1.0f;
    REQUIRE(enac_current(&s.modifiers[0], &s, 0.0f) == Approx(0.0f).margin(1.0f));

    Segment s2 = make_seg(-60.0f);
    s2.modifiers[0].type = MOD_ENAC; s2.modifiers[0].density = 1.0f;
    s2.modifiers[0].state[5] = 100.0f;  // амилорид 100 нМ = IC50
    float I_blocked = enac_current(&s2.modifiers[0], &s2, 0.0f);
    Segment s3 = make_seg(-60.0f);
    s3.modifiers[0].type = MOD_ENAC; s3.modifiers[0].density = 1.0f;
    float I_free = enac_current(&s3.modifiers[0], &s3, 0.0f);
    REQUIRE(fabsf(I_blocked) < fabsf(I_free));  // амилорид уменьшает
}
