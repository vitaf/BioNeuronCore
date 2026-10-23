// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_hcn.cpp — верификация HCN1-4 (129-132) против Ih (гиперполяр.-активируемый Na⁺/K⁺).
// HCN (Stieber 2005 / Biel 2009): γ≈1 пС, P_Na:P_K=1:4 (реверс ≈ −35 мВ),
// V½ per-изоформа: HCN1=−69, HCN2=−95, HCN3=−77, HCN4=−100; τ: HCN1<HCN2<HCN3<HCN4.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double hcn1_current(Modifier* m, Segment* s, float dt);
extern double hcn2_current(Modifier* m, Segment* s, float dt);
extern double hcn3_current(Modifier* m, Segment* s, float dt);
extern double hcn4_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V, uint8_t type, float vhalf, float tau_open) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = type;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[1] = vhalf;
    s.modifiers[0].state[2] = HCN_K_MV;
    float gamma_ps = (type == MOD_HCN1) ? HCN1_GAMMA_PS : (type == MOD_HCN2) ? HCN2_GAMMA_PS
                   : (type == MOD_HCN3) ? HCN3_GAMMA_PS : HCN4_GAMMA_PS;
    s.modifiers[0].state[3] = gamma_ps;
    s.modifiers[0].state[4] = tau_open;
    s.modifiers[0].state[5] = HCN_TAU_CLOSE_MS;
    s.modifier_count = 1;
    return s;  // neurotransmitter нулевой → DA=0
}

TEST_CASE("HCN: V½ per-изоформа (Stieber 2005)", "[hcn]") {
    REQUIRE(hcn_m_inf(HCN1_VHALF_MV, HCN1_VHALF_MV, HCN_K_MV) == Approx(0.5f).margin(1e-3f));
    REQUIRE(hcn_m_inf(HCN2_VHALF_MV, HCN2_VHALF_MV, HCN_K_MV) == Approx(0.5f).margin(1e-3f));
    REQUIRE(hcn_m_inf(HCN3_VHALF_MV, HCN3_VHALF_MV, HCN_K_MV) == Approx(0.5f).margin(1e-3f));
    REQUIRE(hcn_m_inf(HCN4_VHALF_MV, HCN4_VHALF_MV, HCN_K_MV) == Approx(0.5f).margin(1e-3f));
}

TEST_CASE("HCN: гиперполяр.-активация (открыт при V<покой, закрыт при деполяр.)", "[hcn]") {
    // При гиперполяризации открыт, при деполяризации закрыт.
    REQUIRE(hcn_m_inf(-120.0f, -69.0f, 6.0f) > 0.99f);
    REQUIRE(hcn_m_inf(-20.0f, -69.0f, 6.0f) < 0.01f);
    // Монотонность: ниже V → больше Po.
    REQUIRE(hcn_m_inf(-120.0f, -69.0f, 6.0f) > hcn_m_inf(-70.0f, -69.0f, 6.0f));
    REQUIRE(hcn_m_inf(-70.0f, -69.0f, 6.0f) > hcn_m_inf(-20.0f, -69.0f, 6.0f));
}

TEST_CASE("HCN: знак — inward (деполяризация) при гиперполяр., outward при деполяр.", "[hcn]") {
    // Ловит баг знака: -(In+Ik), НЕ -(In−Ik).
    Segment s_lo = make_seg(-100.0f, MOD_HCN1, HCN1_VHALF_MV, HCN1_TAU_OPEN_MS);
    s_lo.modifiers[0].state[0] = 1.0f;
    REQUIRE(hcn1_current(&s_lo.modifiers[0], &s_lo, 0.001f) > 0.0f);  // inward

    Segment s_hi = make_seg(20.0f, MOD_HCN1, HCN1_VHALF_MV, HCN1_TAU_OPEN_MS);
    s_hi.modifiers[0].state[0] = 1.0f;
    REQUIRE(hcn1_current(&s_hi.modifiers[0], &s_hi, 0.001f) < 0.0f);  // outward (K⁺ out доминирует)
}

TEST_CASE("HCN: динамика — HCN1 (τ=84мс) быстрее HCN4 (τ=3586мс)", "[hcn]") {
    Segment s1 = make_seg(-120.0f, MOD_HCN1, HCN1_VHALF_MV, HCN1_TAU_OPEN_MS);
    Segment s4 = make_seg(-120.0f, MOD_HCN4, HCN4_VHALF_MV, HCN4_TAU_OPEN_MS);
    s1.modifiers[0].state[0] = 0.0f;
    s4.modifiers[0].state[0] = 0.0f;
    for (int t = 0; t < 500; t++) {  // 500 × 1 мс
        hcn1_current(&s1.modifiers[0], &s1, 1.0f);
        hcn4_current(&s4.modifiers[0], &s4, 1.0f);
    }
    INFO("HCN1 m=" << s1.modifiers[0].state[0] << " HCN4 m=" << s4.modifiers[0].state[0]);
    REQUIRE(s1.modifiers[0].state[0] > 0.9f);   // HCN1 почти открыт
    REQUIRE(s4.modifiers[0].state[0] < 0.6f);   // HCN4 медленнее (V-зависимый τ всё же ускоряет при −120)
    REQUIRE(s1.modifiers[0].state[0] > s4.modifiers[0].state[0]);
}

TEST_CASE("HCN: порядок τ_open — HCN1 < HCN2 < HCN3 < HCN4 (литература)", "[hcn]") {
    // Ловит баг перепутанных τ (раньше HCN3=50 < HCN2=100). V-зависимый τ
    // (τ·exp((V−V½)/15)) при сильной гиперполяр. ускоряет изоформы с более
    // деполяр. V½ сильнее, поэтому динамический порядок при V=−120 не совпадает
    // с литературным порядком τ_open — проверяем константы напрямую (Stieber 2005).
    REQUIRE(HCN1_TAU_OPEN_MS < HCN2_TAU_OPEN_MS);
    REQUIRE(HCN2_TAU_OPEN_MS < HCN3_TAU_OPEN_MS);
    REQUIRE(HCN3_TAU_OPEN_MS < HCN4_TAU_OPEN_MS);
}

TEST_CASE("HCN: параметр γ (state[3]) не меняется", "[hcn]") {
    Segment s = make_seg(-120.0f, MOD_HCN1, HCN1_VHALF_MV, HCN1_TAU_OPEN_MS);
    for (int t = 0; t < 200; t++) {
        hcn1_current(&s.modifiers[0], &s, 0.01f);
    }
    REQUIRE(s.modifiers[0].state[3] == Approx(HCN1_GAMMA_PS).epsilon(1e-4f));
}
