// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_cacc.cpp — верификация MOD_CACC (146) CaCC/ANO2 (TMEM16B).
// Ca²⁺-активируемый Cl⁻-канал: Hill n=2, EC50=4 µM (Cenedese 2011), γ=2 пС (структурная оценка).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double cacc_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V, float Ca_i_mM) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_CA] = Ca_i_mM;
    s.extra_ions[ION_CA] = 2.0f;     // 2 мМ
    s.intra_ions[ION_CL] = 30.0f;    // ORN: высокий [Cl⁻]ᵢ → деполяризующий Cl⁻-эффлюкс
    s.extra_ions[ION_CL] = 120.0f;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K]  = 4.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_CACC;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

// ── Константы (γ, EC50, Hill, τ) ───────────────────────────────────────
TEST_CASE("CaCC: литературные константы", "[cacc]") {
    REQUIRE(CACC_GAMMA_PS == Approx(2.0f));   // структурная оценка
    REQUIRE(CACC_EC50_UM == Approx(4.0f));    // Cenedese 2011
    REQUIRE(CACC_HILL == Approx(2.0f));       // Hill 1–2
    REQUIRE(CACC_TAU_MS == Approx(2.0f));
}

// ── Hill-гейтинг по Ca²⁺ (EC50=4 µM) ───────────────────────────────────
TEST_CASE("CaCC: Hill по Ca²⁺ — EC50=4 µM", "[cacc]") {
    REQUIRE(cacc_m_inf(4.0f) == Approx(0.5f).margin(1e-3f));  // полумаксимум при EC50
    REQUIRE(cacc_m_inf(0.1f) < 0.01f);                          // низкий Ca → закрыт
    REQUIRE(cacc_m_inf(100.0f) > 0.99f);                        // высокий Ca → открыт
}

// ── Gate-релаксация к m_inf (τ=2 мс) ───────────────────────────────────
TEST_CASE("CaCC: gate-релаксация к m_inf (τ=2 мс)", "[cacc]") {
    Segment s = make_seg(-45.0f, 0.100f);  // 100 µM Ca → m_inf≈1
    s.modifiers[0].state[0] = 0.0f;
    for (int t = 0; t < 200; t++) cacc_current(&s.modifiers[0], &s, 0.1f);  // 20 мс = 10·τ
    float m_inf = cacc_m_inf(100.0f);
    INFO("gate=" << s.modifiers[0].state[0] << " m_inf=" << m_inf);
    REQUIRE(s.modifiers[0].state[0] == Approx(m_inf).margin(0.01f));
}

// ── Ток линеен по density ──────────────────────────────────────────────
TEST_CASE("CaCC: ток линеен по density", "[cacc]") {
    Segment s1 = make_seg(-45.0f, 0.100f);  // высокий Ca → gate≈1
    Segment s2 = make_seg(-45.0f, 0.100f);
    s1.modifiers[0].state[0] = 1.0f;  // gate=1 (зафиксировать)
    s2.modifiers[0].state[0] = 1.0f;
    s2.modifiers[0].density = 2.0f;
    float i1 = fabsf(cacc_current(&s1.modifiers[0], &s1, 0.1f));
    float i2 = fabsf(cacc_current(&s2.modifiers[0], &s2, 0.1f));
    REQUIRE(i2 == Approx(2.0f * i1).margin(1e-3f * i1 + 1e-6f));
}

// ── Ca²⁺-активация: высокий Ca → gate выше (Hill) ──────────────────────
TEST_CASE("CaCC: Ca²⁺ активирует gate (высокий Ca → gate выше)", "[cacc]") {
    Segment s_hi = make_seg(-45.0f, 0.100f);  // 100 µM → m_inf≈1
    Segment s_lo = make_seg(-45.0f, 0.001f);  // 1 µM → m_inf≈0.06 (Hill n=2, EC50=4)
    for (int t = 0; t < 200; t++) {  // релаксация gate к m_inf(Ca)
        cacc_current(&s_hi.modifiers[0], &s_hi, 0.1f);
        cacc_current(&s_lo.modifiers[0], &s_lo, 0.1f);
    }
    REQUIRE(s_hi.modifiers[0].state[0] > s_lo.modifiers[0].state[0]);
}
