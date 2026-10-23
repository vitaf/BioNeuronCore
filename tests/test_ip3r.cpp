// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_ip3r.cpp — верификация IP3R (MOD_IP3R, 127) против Li–Rinzel (1994).
// IP3R читает IP3 из общего пула MOD_PLC_IP3_DAG (честный каскад Gq→PLC→IP3),
// выпускает Ca²⁺ из ЭПР в цитозоль. Масса сохраняется (не больше, чем есть в ЭПР).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double ip3r_current(Modifier* m, Segment* s, float dt);
extern double mglur1_current(Modifier* m, Segment* s, float dt);

// Сегмент с полным каскадом: mGluR1 (Gq) + PLC (IP3 пул) + IP3R.
static Segment make_seg(float Ca_i_mM, float ER_mM, float glu) {
    Segment s = {};
    s.V_m = -70.0f;
    s.intra_ions[ION_CA] = Ca_i_mM;
    s.ER_Ca2 = ER_mM;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_CL] = 5.0f; s.extra_ions[ION_CL] = 130.0f;
    s.extra_ions[ION_CA] = 2.0f;
    s.neurotransmitter[NT_GLUTAMATE] = glu;
    s.ATP_level = 1.0f;
    s.radius = 5.0f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = MOD_NT_RECEPTOR_MGLUR1;
    s.modifiers[0].density = 1.0f;
    s.modifiers[1].type = MOD_PLC_IP3_DAG;
    s.modifiers[1].density = 1.0f;
    s.modifiers[2].type = MOD_IP3R;
    s.modifiers[2].density = 0.15f;
    s.modifier_count = 3;
    return s;
}

TEST_CASE("IP3R: return 0 (внутриклеточная мембрана)", "[ip3r]") {
    Segment s = make_seg(0.0002f, 0.5f, 1.0f);
    float I = ip3r_current(&s.modifiers[2], &s, 1.0f);
    REQUIRE(I == 0.0f);
}

TEST_CASE("IP3R: выброс Ca²⁺ из ЭПР в цитозоль (глутамат → Gq → IP3)", "[ip3r]") {
    Segment s = make_seg(0.0002f, 0.5f, 1.0f);   // Ca=0.2µM, ER=0.5mM, глутамат
    float Ca0 = s.intra_ions[ION_CA];
    float ER0 = s.ER_Ca2;

    for (int i = 0; i < 1000; i++) {   // каскад: Gq → IP3 → выброс
        mglur1_current(&s.modifiers[0], &s, 1.0f);   // Gαq
        plc_ip3_dag_step(&s.modifiers[1], &s, 1.0f); // IP3
        ip3r_current(&s.modifiers[2], &s, 1.0f);     // Ca выброс
    }

    float dCa = s.intra_ions[ION_CA] - Ca0;   // > 0 (Ca в цитозоль)
    float dER = s.ER_Ca2 - ER0;               // < 0 (Ca из ЭПР)
    INFO("dCa=" << dCa << " dER=" << dER << " IP3=" << s.modifiers[1].state[0]);
    REQUIRE(dCa > 0.0f);
    REQUIRE(dER < 0.0f);
}

TEST_CASE("IP3R: без глутамата нет выброса (нет IP3)", "[ip3r]") {
    Segment s = make_seg(0.0002f, 0.5f, 0.0f);   // нет глутамата → нет IP3
    float Ca0 = s.intra_ions[ION_CA];

    for (int i = 0; i < 1000; i++) {
        mglur1_current(&s.modifiers[0], &s, 1.0f);
        plc_ip3_dag_step(&s.modifiers[1], &s, 1.0f);
        ip3r_current(&s.modifiers[2], &s, 1.0f);
    }

    REQUIRE(s.intra_ions[ION_CA] == Approx(Ca0).margin(1e-6f));   // нет выброса
}

TEST_CASE("IP3R: инактивация при высоком Ca²⁺ (bell-кривая)", "[ip3r]") {
    // При высоком Ca (выше K_inh=1µM) h инактивируется.
    Segment s_hiCa = make_seg(0.01f, 0.5f, 1.0f);   // Ca=10µM (инактивация)

    for (int i = 0; i < 50; i++) {
        mglur1_current(&s_hiCa.modifiers[0], &s_hiCa, 1.0f);
        plc_ip3_dag_step(&s_hiCa.modifiers[1], &s_hiCa, 1.0f);
        ip3r_current(&s_hiCa.modifiers[2], &s_hiCa, 1.0f);
    }

    INFO("h=" << s_hiCa.modifiers[2].state[1]);
    // h инактивирован (state[1] < 0.5 при Ca=10µM >> K_inh=1µM)
    REQUIRE(s_hiCa.modifiers[2].state[1] < 0.5f);
}
