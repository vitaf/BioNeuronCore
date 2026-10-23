// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_k2p.cpp — верификация K2P (mod 107) против TREK-1/KCNK2.
// K2P: voltage-independent background K⁺ leak. Single-channel γ=100 пС, Po=0.012 (покой).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double k2p_leak_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 4.0f;  // мМ
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_K2P_LEAK;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[0] = 100.0f;   // γ (пС)
    s.modifiers[0].state[1] = 0.012f;   // Po
    s.modifier_count = 1;
    return s;
}

TEST_CASE("K2P: параметры — voltage-independent (γ, Po не меняются)", "[k2p]") {
    Segment s = make_seg(-70.0f);
    for (int t = 0; t < 200; t++) {
        s.V_m = (t % 2 == 0) ? -40.0f : -90.0f;
        k2p_leak_current(&s.modifiers[0], &s, 0.01f);
    }
    REQUIRE(s.modifiers[0].state[0] == Approx(100.0f).epsilon(1e-4));   // γ
    REQUIRE(s.modifiers[0].state[1] == Approx(0.012f).epsilon(1e-4));   // Po
}

TEST_CASE("K2P: динамика — I-V монотонна, K⁺-эффлюкс (leak)", "[k2p]") {
    // K2P — K⁺ leak: при деполяризации (V → E_K) эффлюкс растёт (ток уменьшается по модулю,
    // т.к. движущая сила V−E_K падает). Проверяем монотонность.
    Segment sA = make_seg(-90.0f);
    Segment sB = make_seg(-70.0f);
    Segment sC = make_seg(-50.0f);
    float IA = k2p_leak_current(&sA.modifiers[0], &sA, 0.01f);
    float IB = k2p_leak_current(&sB.modifiers[0], &sB, 0.01f);
    float IC = k2p_leak_current(&sC.modifiers[0], &sC, 0.01f);
    INFO("IA=" << IA << " IB=" << IB << " IC=" << IC);
    // K⁺ выходит (outward) → return = −I_K = гиперполяризация (отрицательный).
    // При деполяризации (V дальше от E_K≈−95) эффлюкс растёт по модулю.
    REQUIRE(IA < 0.0f);   // гиперполяризация (K out)
    REQUIRE(IB < IA);     // |IB| > |IA|
    REQUIRE(IC < IB);
}

TEST_CASE("K2P: noninactivating — γ/Po прежние после долгой деполяризации", "[k2p]") {
    Segment s = make_seg(-70.0f);
    for (int t = 0; t < 500; t++) { s.V_m = -20.0f; k2p_leak_current(&s.modifiers[0], &s, 0.01f); }
    REQUIRE(s.modifiers[0].state[0] == Approx(100.0f).epsilon(1e-4));
    REQUIRE(s.modifiers[0].state[1] == Approx(0.012f).epsilon(1e-4));
}
