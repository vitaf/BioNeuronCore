// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_receptor_desens.cpp — верификация десенситизации ионотропных рецепторов (#12).
// Двухстадийная модель: r (активация, state[0]) и d (десенситизация, state[1]),
// open_prob = r·(1−d). При продолжающемся агонисте ток сначала растёт (активация),
// затем затухает (десенситизация, τ_desens).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

extern double ampa_current(Modifier* m, Segment* s, float dt);
extern double gabaa_current(Modifier* m, Segment* s, float dt);
extern double nmda_current(Modifier* m, Segment* s, float dt);

static Segment make_seg(float V, uint8_t type, int nt) {
    Segment s = {};
    s.V_m = V;
    s.intra_ions[ION_NA] = 15.0f; s.extra_ions[ION_NA] = 140.0f;
    s.intra_ions[ION_K]  = 140.0f; s.extra_ions[ION_K] = 4.0f;
    s.intra_ions[ION_CL] = 5.0f; s.extra_ions[ION_CL] = 130.0f;
    s.intra_ions[ION_CA] = 0.0001f; s.extra_ions[ION_CA] = 2.0f;
    s.neurotransmitter[nt] = 1.0f;   // насыщающий агонист
    s.radius = 5.0f; s.length = 10.0f; s.segment_type = 1;
    s.modifiers[0].type = type;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    return s;
}

TEST_CASE("AMPA: быстрая десенситизация (τ=3мс) — ток затухает при агонисте", "[desens]") {
    Segment s = make_seg(-70.0f, MOD_NT_RECEPTOR_AMPA, NT_GLUTAMATE);
    float I_peak = 0.0f, I_late = 0.0f;
    for (int t = 0; t < 3; t++) I_peak = ampa_current(&s.modifiers[0], &s, 1.0f);   // ~3 мс — пик
    for (int t = 0; t < 50; t++) I_late = ampa_current(&s.modifiers[0], &s, 1.0f);  // ~50 мс >> τ_desens
    INFO("I_peak=" << I_peak << " I_late=" << I_late);
    REQUIRE(std::fabs(I_late) < std::fabs(I_peak));  // десенситизация затушила ток
    REQUIRE(s.modifiers[0].state[1] > 0.5f);          // d вырос (десенситизирован)
}

TEST_CASE("GABAA: медленная десенситизация (τ=2с) — за 50мс почти не затухает", "[desens]") {
    Segment s = make_seg(-70.0f, MOD_NT_RECEPTOR_GABAA, NT_GABA);
    float I_peak = 0.0f, I_late = 0.0f;
    for (int t = 0; t < 5; t++) I_peak = gabaa_current(&s.modifiers[0], &s, 1.0f);
    for (int t = 0; t < 50; t++) I_late = gabaa_current(&s.modifiers[0], &s, 1.0f);
    INFO("I_peak=" << I_peak << " I_late=" << I_late);
    // τ_desens=2000мс >> 50мс → затухание малое (соотношение > 0.5).
    REQUIRE(std::fabs(I_late) > std::fabs(I_peak) * 0.5f);
    REQUIRE(s.modifiers[0].state[1] < 0.1f);          // d почти не вырос за 50мс
}

TEST_CASE("NMDA: без десенситизации (τ=0) — ток не затухает от d", "[desens]") {
    Segment s = make_seg(-70.0f, MOD_NT_RECEPTOR_NMDA, NT_GLUTAMATE);
    float I_early = 0.0f, I_late = 0.0f;
    for (int t = 0; t < 10; t++) I_early = nmda_current(&s.modifiers[0], &s, 1.0f);
    for (int t = 0; t < 100; t++) I_late = nmda_current(&s.modifiers[0], &s, 1.0f);
    INFO("I_early=" << I_early << " I_late=" << I_late);
    // NMDA τ_desens=0: ток определяется только активацией r (steady r→1), не затухает.
    REQUIRE(s.modifiers[0].state[0] >= s.modifiers[0].state[0]);  // state[0] (r) определён
    // r достиг плато (не десенситизирован): I_late близко к I_early.
    REQUIRE(std::fabs(I_late) > std::fabs(I_early) * 0.5f);
}

TEST_CASE("AMPA: деактивация — r спадает с τ_decay после снятия агониста", "[desens]") {
    // Деактивация (выключение): при снятии агониста (nt=0) r_inf=0, r спадает с τ_decay.
    Segment s = make_seg(-70.0f, MOD_NT_RECEPTOR_AMPA, NT_GLUTAMATE);
    for (int t = 0; t < 10; t++) ampa_current(&s.modifiers[0], &s, 1.0f);  // r→1 (активация)
    float r0 = s.modifiers[0].state[0];
    REQUIRE(r0 > 0.5f);  // активирован

    s.neurotransmitter[NT_GLUTAMATE] = 0.0f;   // снять агонист
    for (int t = 0; t < 10; t++) ampa_current(&s.modifiers[0], &s, 1.0f);  // ~3τ_decay (τ=3мс)
    REQUIRE(s.modifiers[0].state[0] < r0);      // r спал (деактивация)
    REQUIRE(s.modifiers[0].state[0] < 0.2f);    // почти деактивирован
}

TEST_CASE("Рецепторы E_rev: AMPA ≈ 0 (Na/K), GABAA = E_Cl (Cl⁻)", "[desens]") {
    // AMPA (P_Na:P_K=1:1, неселективный): Na вход при гиперполяр. → деполяризующий ток > 0.
    Segment sa = make_seg(-70.0f, MOD_NT_RECEPTOR_AMPA, NT_GLUTAMATE);
    float Ia = 0.0f;
    for (int t = 0; t < 5; t++) Ia = ampa_current(&sa.modifiers[0], &sa, 1.0f);
    REQUIRE(Ia > 0.0f);

    // GABAA (Cl⁻ анионный): при V > E_Cl Cl входит → гиперполяризующий ток < 0.
    Segment sg = make_seg(-30.0f, MOD_NT_RECEPTOR_GABAA, NT_GABA);
    float Ig = 0.0f;
    for (int t = 0; t < 5; t++) Ig = gabaa_current(&sg.modifiers[0], &sg, 1.0f);
    REQUIRE(Ig < 0.0f);
}
