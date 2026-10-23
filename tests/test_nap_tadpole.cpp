// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_nap_tadpole.cpp — верификация NaP-tadpole (mod 195) против dIN INaP.
// Svensson 2017 (J Neurophysiol 118:121): dIN INaP активируется выше −40 мВ, пик ~0 мВ
// → V½≈−25 мВ (НЕ Nav1.6 Mes V V½=−58.7). Без инактивации (persistent), Po=1.6%.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double nap_tadpole_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;  // мМ
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K]  = 4.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_NAP_TADPOLE;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[3] = 20.8f;   // γ (пС, Nav1.2 Ben-Shalom 2017)
    s.modifiers[0].state[5] = 0.016f;  // Po (persistent-фракция)
    s.modifier_count = 1;
    return s;
}

TEST_CASE("NaP-tadpole: m_inf активация (V½=−25, Svensson dIN)", "[nap-tadpole]") {
    REQUIRE(nap_tad_m_inf(-25.0f) == Approx(0.5f).epsilon(1e-3));    // V½
    REQUIRE(nap_tad_m_inf(-10.0f) == Approx(0.924f).margin(0.01f));  // активирован (пик ~0 мВ)
    REQUIRE(nap_tad_m_inf(-51.0f) == Approx(0.013f).margin(0.002f)); // закрыт на покое (НЕ Nav1.6)
    // Контраст с Nav1.6 (V½=−58.7): на покое −51 Nav1.6 NaP открыт ~78%, а dIN-NaP — закрыт.
    REQUIRE(nap_tad_m_inf(-51.0f) < nap_m_inf(-51.0f));
}

TEST_CASE("NaP-tadpole: persistent — γ/Po не меняются (нет h-инактивации)", "[nap-tadpole]") {
    Segment s = make_seg(-60.0f);
    for (int t = 0; t < 200; t++) {
        s.V_m = (t % 2 == 0) ? -10.0f : -60.0f;
        nap_tadpole_current(&s.modifiers[0], &s, 0.01f);
    }
    REQUIRE(s.modifiers[0].state[3] == Approx(20.8f).epsilon(1e-4));   // γ
    REQUIRE(s.modifiers[0].state[5] == Approx(0.016f).epsilon(1e-4));  // Po
}

TEST_CASE("NaP-tadpole: динамика — m-гейт релаксирует к m_inf (tau=5 мс)", "[nap-tadpole]") {
    Segment s = make_seg(-10.0f);  // деполяризация → m_inf ≈ 0.924
    s.modifiers[0].state[0] = 0.0f;  // m=0 старт
    for (int t = 0; t < 1000; t++) {  // 1000 × 0.05 мс = 50 мс >> 5·tau
        nap_tadpole_current(&s.modifiers[0], &s, 0.05f);
    }
    REQUIRE(s.modifiers[0].state[0] == Approx(nap_tad_m_inf(-10.0f)).margin(0.02f));  // m → m_inf
}

TEST_CASE("NaP-tadpole: I-V — Na входит при деполяризации (persistent)", "[nap-tadpole]") {
    Segment sA = make_seg(-60.0f);
    Segment sB = make_seg(-20.0f);
    float IA = nap_tadpole_current(&sA.modifiers[0], &sA, 0.01f);
    float IB = nap_tadpole_current(&sB.modifiers[0], &sB, 0.01f);
    REQUIRE(IB > 0.0f);   // деполяризующий ток при активации
    REQUIRE(IB > IA);     // при деполяризации активация выше → ток больше
}
