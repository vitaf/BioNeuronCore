// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_cav14.cpp — верификация MOD_CAV14 (144) Cav1.4 + MOD_CABP4 (145).
// Cav1.4: γ=4 пС (Doering 2005), V½_bare=−15 мВ, БЕЗ CDI (ICDI), медленная VDI, m².
// CaBP4 (Shaltiel 2012, Park 2014): связывает IQ → снимает ингибицию ICDI/CTM → ΔV½=−10 мВ.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double cav14_current(Modifier* m, Segment* s, float dt);
extern double cabp4_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_CA] = 0.0001f;  // 100 нМ
    s.extra_ions[ION_CA] = 2.0f;     // 2 мМ
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K]  = 4.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_CAV14;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

// ── Константы (γ, V½, k, τ_m, ΔV½, τ_bind) ──────────────────────────────
TEST_CASE("Cav1.4: литературные константы", "[cav14]") {
    REQUIRE(CAV14_GAMMA_PS == Approx(4.0f));          // Doering 2005 (1–4 пС)
    REQUIRE(CAV14_VHALF_BARE == Approx(-15.0f));      // Ca²⁺, полноразмерный
    REQUIRE(CAV14_K_ACT == Approx(7.0f));
    REQUIRE(CAV14_TAU_M_MS == Approx(2.0f));
    REQUIRE(CAV14_CABP4_DVHALF == Approx(-10.0f));    // Shaltiel 2012
    REQUIRE(CABP4_TAU_BIND_MS == Approx(1.0f));
}

// ── m_inf гейтинг (V½_bare=−15) ─────────────────────────────────────────
TEST_CASE("Cav1.4: m_inf V½=−15 мВ (голый)", "[cav14]") {
    REQUIRE(cav14_m_inf(-15.0f, CAV14_VHALF_BARE) == Approx(0.5f).margin(1e-3f));
    REQUIRE(cav14_m_inf(30.0f, CAV14_VHALF_BARE) > 0.99f);    // деполяр. → открыт
    REQUIRE(cav14_m_inf(-60.0f, CAV14_VHALF_BARE) < 0.01f);   // гиперполяр. → закрыт
}

// ── CaBP4 сдвигает V½ на −10 мВ ────────────────────────────────────────
TEST_CASE("Cav1.4: CaBP4 сдвигает V½ на −10 мВ", "[cav14][cabp4]") {
    float vhalf_cabp4 = CAV14_VHALF_BARE + CAV14_CABP4_DVHALF;  // −25 мВ
    REQUIRE(vhalf_cabp4 == Approx(-25.0f));
    // При новом V½ (с CaBP4) полумаксимум достигается при −25 мВ
    REQUIRE(cav14_m_inf(-25.0f, vhalf_cabp4) == Approx(0.5f).margin(1e-3f));
    // При одном и том же V (−25) с CaBP4 открыт сильнее, чем без
    REQUIRE(cav14_m_inf(-25.0f, vhalf_cabp4) > cav14_m_inf(-25.0f, CAV14_VHALF_BARE));
}

// ── Нет h-гейта: БЕЗ CDI и БЕЗ VDI (отличие от Cav1.3/Cav1.2) ──────────
TEST_CASE("Cav1.4: нет h-гейта — state[1] не трогается (нет CDI/VDI)", "[cav14]") {
    Segment s = make_seg(30.0f);
    s.modifiers[0].state[0] = 1.0f;  // m=1
    s.modifiers[0].state[1] = 0.5f;  // произвольный h — НЕ должен меняться
    for (int t = 0; t < 100; t++) cav14_current(&s.modifiers[0], &s, 0.1f);
    REQUIRE(s.modifiers[0].state[1] == Approx(0.5f));  // h не трогается (нет инактивации)
}

// ── m-релаксация к m_inf (τ_m=2 мс) ────────────────────────────────────
TEST_CASE("Cav1.4: m-релаксация к m_inf (τ_m=2 мс)", "[cav14]") {
    Segment s = make_seg(30.0f);
    s.modifiers[0].state[0] = 0.0f;
    for (int t = 0; t < 200; t++) cav14_current(&s.modifiers[0], &s, 0.1f);  // 20 мс = 10·τ_m
    float m_inf = cav14_m_inf(30.0f, CAV14_VHALF_BARE);
    INFO("m=" << s.modifiers[0].state[0] << " m_inf=" << m_inf);
    REQUIRE(s.modifiers[0].state[0] == Approx(m_inf).margin(0.01f));
}

// ── Плотность: ток ∝ density ──────────────────────────────────────────
TEST_CASE("Cav1.4: ток линеен по density", "[cav14]") {
    Segment s1 = make_seg(-45.0f);
    Segment s2 = make_seg(-45.0f);
    s1.modifiers[0].state[0] = 1.0f;  // m=1 (зафиксировать гейтинг)
    s2.modifiers[0].state[0] = 1.0f;
    s2.modifiers[0].density = 2.0f;
    float i1 = fabsf(cav14_current(&s1.modifiers[0], &s1, 0.1f));
    float i2 = fabsf(cav14_current(&s2.modifiers[0], &s2, 0.1f));
    REQUIRE(i2 == Approx(2.0f * i1).margin(1e-3f * i1 + 1e-6f));
}

// ── CaBP4: bound насыщается к 1, ток = 0 ───────────────────────────────
TEST_CASE("CaBP4: bound→1, мембранный ток = 0 (не канал)", "[cabp4]") {
    Segment s = make_seg(-45.0f);
    s.modifiers[0].type = MOD_CABP4;
    s.modifiers[0].state[0] = 0.0f;
    float cur = 0.0f;
    for (int t = 0; t < 100; t++) cur = cabp4_current(&s.modifiers[0], &s, 0.1f);  // 10 мс
    REQUIRE(cur == Approx(0.0f));               // НЕ канал
    REQUIRE(s.modifiers[0].state[0] > 0.9f);    // bound → ~1
}

// ── segment_cabp4_bound: 0 без CaBP4, 1 с связанным CaBP4 ──────────────
TEST_CASE("Cav1.4: читает CaBP4 через segment_cabp4_bound", "[cav14][cabp4]") {
    Segment s = make_seg(-45.0f);
    REQUIRE(segment_cabp4_bound(&s) == Approx(0.0f));  // нет CaBP4
    s.modifiers[1].type = MOD_CABP4;
    s.modifiers[1].state[0] = 1.0f;
    s.modifier_count = 2;
    REQUIRE(segment_cabp4_bound(&s) == Approx(1.0f));  // CaBP4 связан
}

// ── CaBP4 повышает Ca²⁺-вход в темноте (через сдвиг V½) ────────────────
TEST_CASE("Cav1.4: CaBP4 повышает m² в темноте (−45 мВ)", "[cav14][cabp4]") {
    // Без CaBP4: V½=−15 → m_inf(−45) мал; с CaBP4: V½=−25 → m_inf(−45) выше.
    Segment s_wo = make_seg(-45.0f);
    Segment s_w  = make_seg(-45.0f);
    s_w.modifiers[1].type = MOD_CABP4;
    s_w.modifiers[1].state[0] = 1.0f;  // CaBP4 связан
    s_w.modifier_count = 2;
    for (int t = 0; t < 200; t++) {  // релаксация m
        cav14_current(&s_wo.modifiers[0], &s_wo, 0.1f);
        cav14_current(&s_w.modifiers[0], &s_w, 0.1f);
    }
    REQUIRE(s_w.modifiers[0].state[0] > s_wo.modifiers[0].state[0]);  // с CaBP4 m выше
}
