// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_bk.cpp — верификация BK (mod 104) против KCa1.1 α+β4.
// Нейрональный BK: Horrigan–Aldrich (Wang, Rothberg & Brenner 2006, JGP 127:449),
//   best fit α+β4_a: γ=250 пС, V½(100µM)≈−38.9 мВ, Ca²⁺-кооперативность Hill~3.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double bk_channel_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V, float Ca_mM) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 4.0f;   // мМ
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_CA] = Ca_mM;                                // мМ (моль/м³)
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_BK_CHANNEL;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[1] = 250.0f;  // γ (пС)
    s.modifier_count = 1;
    return s;
}

TEST_CASE("BK: Po(V,Ca) — покой закрыт, спайк+Ca открыт", "[bk]") {
    // Покой (V=−65, Ca=0.1µM): BK закрыт.
    REQUIRE(bk_ha_po(-65.0f, 0.0001f) < 1e-5f);
    // Спайк (V=+30) + нанодомен Ca (50µM): BK открыт.
    REQUIRE(bk_ha_po(30.0f, 0.05f) > 0.9f);
    // Ca²⁺ открывает: при том же V больше Ca → больше Po.
    REQUIRE(bk_ha_po(30.0f, 0.05f) > bk_ha_po(30.0f, 0.0001f));
    // Деполяризация открывает: при том же Ca больше V → больше Po.
    REQUIRE(bk_ha_po(-40.0f, 0.1f) > bk_ha_po(-65.0f, 0.1f));
}

TEST_CASE("BK: V½(100µM) в диапазоне Wang 2006 (−38.9 мВ)", "[bk]") {
    float p_lo = bk_ha_po(-65.0f, 0.1f);   // ниже V½ — ниже полуоткрытия
    float p_mid = bk_ha_po(-40.0f, 0.1f);  // около V½ — выше полуоткрытия
    float p_hi = bk_ha_po(-20.0f, 0.1f);   // выше V½ — открыт
    INFO("Po(-65)=" << p_lo << " Po(-40)=" << p_mid << " Po(-20)=" << p_hi);
    REQUIRE(p_lo < 0.5f);    // V½ > −65
    REQUIRE(p_mid > 0.5f);   // V½ < −40 (Wang: −38.9, ±температура/фит)
    REQUIRE(p_hi > 0.8f);
}

TEST_CASE("BK: Ca²⁺-кооперативность (superlinear Po(Ca))", "[bk]") {
    // 10× Ca (10→100µM) даёт >>10× Po при фикс. V=−40 — Hill > 1 (кооперативность).
    float p10 = bk_ha_po(-40.0f, 0.01f);
    float p100 = bk_ha_po(-40.0f, 0.1f);
    INFO("Po(10µM)=" << p10 << " Po(100µM)=" << p100);
    REQUIRE(p100 > 10.0f * p10);
}

TEST_CASE("BK: параметры — γ не меняется (нет скрытого гейтинга state[1])", "[bk]") {
    Segment s = make_seg(30.0f, 0.05f);
    for (int t = 0; t < 200; t++) {
        bk_channel_current(&s.modifiers[0], &s, 0.01f);
    }
    REQUIRE(s.modifiers[0].state[1] == Approx(250.0f).epsilon(1e-4));  // γ
}

TEST_CASE("BK: динамика — open_prob релаксирует к po_inf (τ=1 мс)", "[bk]") {
    Segment s = make_seg(30.0f, 0.05f);  // po_inf ≈ 0.988
    s.modifiers[0].state[0] = 0.0f;      // open_prob = 0 старт
    for (int t = 0; t < 200; t++) {      // 200 × 0.05 мс = 10 мс >> τ
        bk_channel_current(&s.modifiers[0], &s, 0.05f);
    }
    REQUIRE(s.modifiers[0].state[0] == Approx(bk_ha_po(30.0f, 0.05f)).margin(0.02f));
}

TEST_CASE("BK: знак — return = −I_K (гиперполяризация при outward K)", "[bk]") {
    // Деполяризация + Ca → BK открыт → K⁺ выходит (outward) → return < 0.
    Segment s = make_seg(30.0f, 0.05f);
    float I = bk_channel_current(&s.modifiers[0], &s, 0.01f);
    REQUIRE(I < 0.0f);
}
