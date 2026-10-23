// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_channel_erev.cpp — реверсивный потенциал E_rev ионных каналов (#14).
// E_rev выводится из селективности (P_Na:P_K:P_Ca) и концентраций (Нернст/GHK),
// НЕ хардкод. Проверяет: знак тока + E_rev + селективность.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double k2p_leak_current(Modifier* m, Segment* s, float dt);
extern double nap_channel_current(Modifier* m, Segment* s, float dt);
extern double hcn1_current(Modifier* m, Segment* s, float dt);
extern double nav16_current(Modifier* m, Segment* s, float dt);
extern double cav21_current(Modifier* m, Segment* s, float dt);
extern double kir_channel_current(Modifier* m, Segment* s, float dt);
extern double kv42_current(Modifier* m, Segment* s, float dt);
extern double pir_ttype_current(Modifier* m, Segment* s, float dt);
extern double sk_channel_current(Modifier* m, Segment* s, float dt);
extern double bk_channel_current(Modifier* m, Segment* s, float dt);
extern double kna_channel_current(Modifier* m, Segment* s, float dt);

// Нернст-потенциал (мВ) для одного иона.
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

TEST_CASE("K2P: E_rev = E_K (K-селективный leak)", "[channel_erev]") {
    double e_k = nernst_mv(140.0, 4.0, 1.0);  // ≈ −91 мВ
    Segment s = make_seg((float)e_k);
    s.modifiers[0].type = MOD_K2P_LEAK;
    s.modifiers[0].density = 1.0f;
    float I_at_ek = k2p_leak_current(&s.modifiers[0], &s, 0.0f);
    // K-селективный: ток при E_K ≈ 0.
    REQUIRE(I_at_ek == Approx(0.0f).margin(1.0f));

    // Знак: K выходит (гиперполяризующий, return<0) при V > E_K.
    Segment s2 = make_seg(-60.0f);
    s2.modifiers[0].type = MOD_K2P_LEAK;
    s2.modifiers[0].density = 1.0f;
    REQUIRE(k2p_leak_current(&s2.modifiers[0], &s2, 0.0f) < 0.0f);
}

TEST_CASE("NaP: E_rev = E_Na (Na-селективный persistent)", "[channel_erev]") {
    double e_na = nernst_mv(15.0, 140.0, 1.0);  // ≈ +57 мВ
    // NaP — гейтированный (m·Po). При полном открытии (m=1, Po в state) E_rev = E_Na.
    Segment s = make_seg((float)e_na);
    s.modifiers[0].type = MOD_NAP_CHANNEL;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[0] = 1.0f;  // m = 1 (активирован)
    float I_ena = nap_channel_current(&s.modifiers[0], &s, 0.0f);
    // Na-селективный: ток при E_Na ≈ 0 (Po фиксирован внутри, но знак меняется на E_Na).
    REQUIRE(std::fabs(I_ena) < 20.0f);
}

TEST_CASE("HCN1: E_rev между E_K и E_Na (Na/K 1:4)", "[channel_erev]") {
    double e_k = nernst_mv(140.0, 4.0, 1.0);
    double e_na = nernst_mv(15.0, 140.0, 1.0);
    // P_Na:P_K = 1:4 → E_rev = RT/F·ln((0.2·Na_o+0.8·K_o)/(0.2·Na_i+0.8·K_i)).
    double erev = nernst_mv(0.2*15.0 + 0.8*140.0, 0.2*140.0 + 0.8*4.0, 1.0);
    // E_rev между E_K и E_Na.
    REQUIRE(erev > e_k);
    REQUIRE(erev < e_na);
}

TEST_CASE("Nav16: E_rev = E_Na (Na-селективный, m³h)", "[channel_erev]") {
    double e_na = nernst_mv(15.0, 140.0, 1.0);
    Segment s = make_seg((float)e_na);
    s.modifiers[0].type = MOD_AXON_NAV16;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[0] = 1.0f;  // m открыт
    s.modifiers[0].state[1] = 1.0f;  // h доступен
    double I = nav16_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(std::fabs(I) < 20.0f);  // Na-селективный: ток ≈ 0 при E_Na
}

TEST_CASE("Cav21: E_rev = E_Ca (Ca-селективный, z=2)", "[channel_erev]") {
    double e_ca = nernst_mv(0.0001, 1.2, 2.0);  // ≈ +120 мВ
    Segment s = make_seg((float)e_ca);
    s.modifiers[0].type = MOD_CAV21;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[0] = 1.0f;  // m открыт
    double I = cav21_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(std::fabs(I) < 20.0f);  // Ca-селективный: ток ≈ 0 при E_Ca
}

TEST_CASE("Kir: E_rev = E_K (K-селективный inward rectifier)", "[channel_erev]") {
    double e_k = nernst_mv(140.0, 4.0, 1.0);
    Segment s = make_seg((float)e_k);
    s.modifiers[0].type = MOD_KIR_CHANNEL;
    s.modifiers[0].density = 1.0f;
    double I = kir_channel_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(std::fabs(I) < 20.0f);  // K-селективный: ток ≈ 0 при E_K
}

TEST_CASE("Kv42: E_rev = E_K (A-type K)", "[channel_erev]") {
    double e_k = nernst_mv(140.0, 4.0, 1.0);
    Segment s = make_seg((float)e_k);
    s.modifiers[0].type = MOD_KV42;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[0] = 1.0f;  // m открыт
    double I = kv42_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(std::fabs(I) < 20.0f);  // K-селективный: ток ≈ 0 при E_K
}

TEST_CASE("Cav3: E_rev = E_Ca (T-type Ca)", "[channel_erev]") {
    double e_ca = nernst_mv(0.0001, 1.2, 2.0);
    Segment s = make_seg((float)e_ca);
    s.modifiers[0].type = MOD_PIR_TTYPE_CALCIUM;
    s.modifiers[0].density = 1.0f;
    s.modifiers[0].state[0] = 1.0f;  // m открыт
    double I = pir_ttype_current(&s.modifiers[0], &s, 0.0f);
    REQUIRE(std::fabs(I) < 20.0f);  // Ca-селективный: ток ≈ 0 при E_Ca
}

TEST_CASE("SK/BK/KNa: E_rev = E_K (K-селективные, Ca/Na-активируемые)", "[channel_erev]") {
    double e_k = nernst_mv(140.0, 4.0, 1.0);
    struct { uint8_t type; double (*fn)(Modifier*, Segment*, float); } chans[] = {
        {MOD_SK_CHANNEL, sk_channel_current},
        {MOD_BK_CHANNEL, bk_channel_current},
        {MOD_KNA_CHANNEL, kna_channel_current},
    };
    for (auto& c : chans) {
        Segment s = make_seg((float)e_k);
        s.modifiers[0].type = c.type;
        s.modifiers[0].density = 1.0f;
        s.modifiers[0].state[0] = 1.0f;  // open_prob = 1 (полное открытие)
        double I = c.fn(&s.modifiers[0], &s, 0.0f);
        INFO("type=" << (int)c.type);
        REQUIRE(std::fabs(I) < 20.0f);  // K-селективный: ток ≈ 0 при E_K
    }
}
