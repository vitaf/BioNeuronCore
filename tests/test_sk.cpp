// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_sk.cpp — верификация SK (mod 103) против KCa2.2 (SK2).
// SK2: γ≈10 пС, Ca²⁺ EC50=0.4 µM, Hill 4 (кальмодулин), чисто Ca²⁺-гейт (нет V).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double sk_channel_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V, float Ca_mM) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 4.0f;   // мМ
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_CA] = Ca_mM;                                // мМ (моль/м³)
    s.radius = 5.64f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_SK_CHANNEL;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[3] = 10.0f;  // γ (пС)
    s.modifier_count = 1;
    return s;
}

TEST_CASE("SK: p_inf(Ca) — Hill-4, EC50=0.4 µM", "[sk]") {
    REQUIRE(sk_p_inf(0.0004f) == Approx(0.5f).epsilon(1e-3));   // EC50 (0.4 µM)
    REQUIRE(sk_p_inf(0.004f) > 0.99f);                           // насыщение (4 µM)
    REQUIRE(sk_p_inf(0.00004f) < 0.001f);                        // закрыт (40 nM)
    REQUIRE(sk_p_inf(0.001f) > sk_p_inf(0.0001f));               // монотонность
}

TEST_CASE("SK: нет V-гейта — open_prob зависит только от Ca", "[sk]") {
    Segment sDep = make_seg(30.0f, 0.004f);    // деполяризация
    Segment sHyp = make_seg(-80.0f, 0.004f);   // гиперполяризация
    sDep.modifiers[0].state[0] = 0.0f;
    sHyp.modifiers[0].state[0] = 0.0f;
    for (int t = 0; t < 1000; t++) {           // 50 мс >> τ
        sk_channel_current(&sDep.modifiers[0], &sDep, 0.05f);
        sk_channel_current(&sHyp.modifiers[0], &sHyp, 0.05f);
    }
    // Одинаковый Ca → одинаковый open_prob, несмотря на разный V.
    REQUIRE(sDep.modifiers[0].state[0] == Approx(sHyp.modifiers[0].state[0]).margin(0.02f));
    REQUIRE(sDep.modifiers[0].state[0] == Approx(1.0f).margin(0.02f));
}

TEST_CASE("SK: параметры — γ не меняется (нет скрытого гейтинга state[3])", "[sk]") {
    Segment s = make_seg(30.0f, 0.004f);
    for (int t = 0; t < 200; t++) {
        sk_channel_current(&s.modifiers[0], &s, 0.01f);
    }
    REQUIRE(s.modifiers[0].state[3] == Approx(10.0f).epsilon(1e-4));  // γ
}

TEST_CASE("SK: динамика — open_prob релаксирует к p_inf (открытие τ=5 мс)", "[sk]") {
    Segment s = make_seg(30.0f, 0.004f);  // p_inf ≈ 1.0
    s.modifiers[0].state[0] = 0.0f;
    for (int t = 0; t < 1000; t++) {      // 1000 × 0.05 мс = 50 мс >> τ_open
        sk_channel_current(&s.modifiers[0], &s, 0.05f);
    }
    REQUIRE(s.modifiers[0].state[0] == Approx(sk_p_inf(0.004f)).margin(0.02f));
}

TEST_CASE("SK: знак — return = −I_K (гиперполяризация при outward K)", "[sk]") {
    Segment s = make_seg(30.0f, 0.004f);  // деполяризация + Ca → SK открыт → K⁺ выходит
    float I = sk_channel_current(&s.modifiers[0], &s, 0.01f);
    REQUIRE(I < 0.0f);
}
