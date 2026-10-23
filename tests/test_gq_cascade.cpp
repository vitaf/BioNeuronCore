// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_gq_cascade.cpp — верификация Gq→PLC→IP3+DAG→IP3R/PKC (MOD_PLC_IP3_DAG 136, MOD_PKC 137).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

static Segment make_seg_gq(bool with_gq) {
    Segment s = {};
    s.intra_ions[ION_CA] = 0.0001f;   // 0.1 µM = 0.0001 мМ
    s.ER_Ca2 = 1.0f;                  // 1 мМ Ca²⁺ в ЭПР
    s.modifiers[0].type = MOD_PLC_IP3_DAG;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    if (with_gq) {
        s.modifiers[1].type = MOD_NT_RECEPTOR_5HT2A;  // Gq
        s.modifiers[1].state[0] = 1.0f;               // Gαq активен
        s.modifier_count = 2;
    }
    return s;
}

TEST_CASE("Gq: PLC производит IP3 и DAG (1:1) из Gq-рецепторов", "[gq]") {
    Segment s_base = make_seg_gq(false);
    Segment s_gq   = make_seg_gq(true);
    for (int t = 0; t < 50000; t++) {
        plc_ip3_dag_step(&s_base.modifiers[0], &s_base, 0.1f);
        plc_ip3_dag_step(&s_gq.modifiers[0], &s_gq, 0.1f);
    }
    INFO("IP3_base=" << s_base.modifiers[0].state[0] << " IP3_gq=" << s_gq.modifiers[0].state[0]);
    REQUIRE(s_gq.modifiers[0].state[0] > s_base.modifiers[0].state[0]);  // IP3 выше
    REQUIRE(s_gq.modifiers[0].state[1] > s_base.modifiers[0].state[1]);  // DAG выше
}

TEST_CASE("PKC: DAG + Ca²⁺ → активность (классическая, нужны оба)", "[gq]") {
    Segment s_gq = make_seg_gq(true);
    s_gq.modifiers[2].type = MOD_PKC;
    s_gq.modifier_count = 3;
    for (int t = 0; t < 50000; t++) plc_ip3_dag_step(&s_gq.modifiers[0], &s_gq, 0.1f);
    pkc_step(&s_gq.modifiers[2], &s_gq, 0.1f);
    float pkc = s_gq.modifiers[2].state[0];
    INFO("DAG=" << s_gq.modifiers[0].state[1] << " Ca=" << s_gq.intra_ions[ION_CA] << " PKC=" << pkc);
    REQUIRE(pkc > 0.0f);
    REQUIRE(pkc <= 1.0f);

    // Без DAG (нет каскада) → PKC=0
    Segment s_empty = make_seg_gq(false);
    s_empty.modifiers[1].type = MOD_PKC;
    s_empty.modifier_count = 2;
    pkc_step(&s_empty.modifiers[1], &s_empty, 0.1f);
    REQUIRE(s_empty.modifiers[1].state[0] == Approx(0.0f).margin(1e-6));
}

TEST_CASE("Кросс-модуляция: segment_ip3_mm/segment_dag/segment_pkc_act читают пулы", "[gq]") {
    Segment s = make_seg_gq(true);
    s.modifiers[2].type = MOD_PKC;
    s.modifier_count = 3;
    for (int t = 0; t < 50000; t++) {
        plc_ip3_dag_step(&s.modifiers[0], &s, 0.1f);
        pkc_step(&s.modifiers[2], &s, 0.1f);
    }
    REQUIRE(segment_ip3_mm(&s) == Approx(s.modifiers[0].state[0]).epsilon(1e-6));
    REQUIRE(segment_dag(&s) == Approx(s.modifiers[0].state[1]).epsilon(1e-6));
    REQUIRE(segment_pkc_act(&s) == Approx(s.modifiers[2].state[0]).epsilon(1e-6));
}

TEST_CASE("IP3R: читает IP3 из пула → P_open > 0 при Gq", "[gq]") {
    // Соберём: MOD_PLC_IP3_DAG + Gq (5-HT2A) + IP3R, с ER Ca²⁺.
    Segment s = make_seg_gq(true);
    s.modifiers[2].type = MOD_IP3R;
    s.modifiers[2].density = 1.0f;
    s.modifier_count = 3;
    float ca0 = s.intra_ions[ION_CA];
    extern double ip3r_current(Modifier*, Segment*, float);
    for (int t = 0; t < 50000; t++) {
        plc_ip3_dag_step(&s.modifiers[0], &s, 0.1f);
        ip3r_current(&s.modifiers[2], &s, 0.1f);
    }
    INFO("IP3=" << segment_ip3_mm(&s) << " Ca_i=" << s.intra_ions[ION_CA] << " (start " << ca0 << ")");
    // IP3R выпустил Ca²⁺ из ЭПР → цитозольный Ca²⁺ вырос.
    REQUIRE(s.intra_ions[ION_CA] > ca0);
}

TEST_CASE("Mrgpr: лиганд state[4] → Gαq-активация → PLC→IP3", "[gq][mrgpr]") {
    Segment s = make_seg_gq(false);
    s.modifiers[1].type = MOD_MRGPR;
    s.modifiers[1].state[4] = 100.0f;  // хлороквин 100 µM (EC50 28)
    s.modifier_count = 2;
    for (int t = 0; t < 50000; t++) {
        mrgpr_signal(&s.modifiers[1], &s, 0.1f);
        plc_ip3_dag_step(&s.modifiers[0], &s, 0.1f);
    }
    INFO("Gαq=" << s.modifiers[1].state[0] << " IP3=" << s.modifiers[0].state[0]);
    REQUIRE(s.modifiers[1].state[0] > 0.7f);   // Gαq активирован (occupancy 100/128≈0.78)
    REQUIRE(s.modifiers[0].state[0] > 0.0f);   // IP3 растёт через PLC (Mrgpr → Gq-каскад)
}

TEST_CASE("TRPA1: Ca²⁺-активация без AITC (Mrgpr-каскад)", "[gq][trpa1]") {
    Segment s = {};
    s.V_m = -60.0f;
    s.intra_ions[ION_NA] = 10.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_CA] = 0.006f; s.extra_ions[ION_CA] = 2.0f;  // 6 µM Ca²⁺ (EC50 активации)
    s.modifiers[0].type = MOD_TRPA1;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[4] = 0.0f;   // нет AITC
    s.modifiers[0].state[7] = 100.0f; // effective_area = 100 µm² (иначе area=0 → ток 0)
    s.modifier_count = 1;
    extern double trpa1_current(Modifier*, Segment*, float);
    float I = trpa1_current(&s.modifiers[0], &s, 0.1f);
    INFO("I_TRPA1(Ca-only, 6µM)=" << I);
    REQUIRE(I != 0.0f);  // Ca²⁺-активация открывает канал даже без AITC
}

TEST_CASE("PIEZO2: механо-стимул → открытие (Hill λ50=2 мН/м)", "[piezo2]") {
    Segment s = {};
    s.V_m = -60.0f;
    s.intra_ions[ION_CA] = 0.0001f; s.extra_ions[ION_CA] = 2.0f;
    s.intra_ions[ION_K] = 140.0f; s.extra_ions[ION_K] = 5.0f;
    s.intra_ions[ION_NA] = 10.0f; s.extra_ions[ION_NA] = 140.0f;
    s.modifiers[0].type = MOD_PIEZO2;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[4] = 2.0f;   // стимул = λ50 → Po_inf ~0.5
    s.modifiers[0].state[7] = 100.0f; // effective_area = 100 µm²
    s.modifier_count = 1;
    extern double piezo2_current(Modifier*, Segment*, float);
    float I = piezo2_current(&s.modifiers[0], &s, 0.1f);
    INFO("I_PIEZO2(стимул=2 мН/м)=" << I);
    REQUIRE(I != 0.0f);  // канал открывается механо-стимулом
}

TEST_CASE("TMC1: базальная Po ~10% даёт ток без стимула", "[tmc1]") {
    Segment s = {};
    s.V_m = -60.0f;
    s.intra_ions[ION_CA] = 0.0001f; s.extra_ions[ION_CA] = 2.0f;
    s.intra_ions[ION_NA] = 10.0f; s.extra_ions[ION_NA] = 140.0f;
    s.modifiers[0].type = MOD_TMC1;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[4] = 0.0f;   // нет стимула → базальная Po 10%
    s.modifiers[0].state[7] = 100.0f;
    s.modifier_count = 1;
    extern double tmc1_current(Modifier*, Segment*, float);
    float I = tmc1_current(&s.modifiers[0], &s, 0.1f);
    INFO("I_TMC1(базальная Po)=" << I);
    REQUIRE(I != 0.0f);  // базальная Po ~10% → покойный MET-ток
}
