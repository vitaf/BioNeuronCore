// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_camp_pka.cpp — верификация каскада Gs→AC→cAMP→PKA (MOD_CAMP_PKA, 135).
// Базальный [cAMP] ~0.1 µM; Gs-активация → [cAMP]↑; PKA=Hill(cAMP); HCN V½ сдвиг; L-type PKA-буст.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cmath>
#include "modifier_helpers.h"

// Сегмент с MOD_CAMP_PKA; опционально — активированный Gs-рецептор D1.
static Segment make_seg(bool with_gs, float gs_act) {
    Segment s = {};
    s.ATP_level = 1.0f;                    // 1 мМ ATP
    s.modifiers[0].type = MOD_CAMP_PKA;
    s.modifiers[0].density = 1.0f;
    s.modifier_count = 1;
    if (with_gs) {
        s.modifiers[1].type = MOD_NT_RECEPTOR_D1;
        s.modifiers[1].state[0] = gs_act;  // Gαs-активация
        s.modifier_count = 2;
    }
    return s;
}

TEST_CASE("cAMP: базальный [cAMP] ~0.1 µM (без Gs)", "[camp_pka]") {
    Segment s = make_seg(false, 0.0f);
    for (int t = 0; t < 50000; t++) camp_pka_step(&s.modifiers[0], &s, 0.1f);  // 5 с → равновесие (PDE τ~2с)
    float camp = s.modifiers[0].state[0];
    INFO("[cAMP]=" << camp << " µM");
    REQUIRE(camp > 0.05f);
    REQUIRE(camp < 0.5f);
}

TEST_CASE("cAMP: Gs-активация поднимает [cAMP] выше базального", "[camp_pka]") {
    Segment s_base = make_seg(false, 0.0f);
    Segment s_gs   = make_seg(true, 1.0f);
    for (int t = 0; t < 50000; t++) {
        camp_pka_step(&s_base.modifiers[0], &s_base, 0.1f);
        camp_pka_step(&s_gs.modifiers[0], &s_gs, 0.1f);
    }
    INFO("basal=" << s_base.modifiers[0].state[0] << " gs=" << s_gs.modifiers[0].state[0]);
    REQUIRE(s_gs.modifiers[0].state[0] > s_base.modifiers[0].state[0]);
    REQUIRE(s_gs.modifiers[0].state[0] > 1.0f);  // существенно выше базального
}

TEST_CASE("cAMP: Gi/o-ингибирование AC — D2 снижает [cAMP] ниже базального", "[camp_pka]") {
    Segment s_base = make_seg(false, 0.0f);
    Segment s_gi   = make_seg(false, 0.0f);
    s_gi.modifiers[1].type = MOD_NT_RECEPTOR_D2;  // Gi/o
    s_gi.modifiers[1].state[0] = 1.0f;            // Gαi активен
    s_gi.modifier_count = 2;
    for (int t = 0; t < 50000; t++) {
        camp_pka_step(&s_base.modifiers[0], &s_base, 0.1f);
        camp_pka_step(&s_gi.modifiers[0], &s_gi, 0.1f);
    }
    INFO("basal=" << s_base.modifiers[0].state[0] << " gi=" << s_gi.modifiers[0].state[0]);
    REQUIRE(s_gi.modifiers[0].state[0] < s_base.modifiers[0].state[0]);
    REQUIRE(s_gi.modifiers[0].state[1] < s_base.modifiers[0].state[1]);  // PKA ниже
}

TEST_CASE("PKA: Hill(cAMP) — растёт с [cAMP]", "[camp_pka]") {
    Segment s_base = make_seg(false, 0.0f);
    Segment s_gs   = make_seg(true, 1.0f);
    for (int t = 0; t < 50000; t++) {
        camp_pka_step(&s_base.modifiers[0], &s_base, 0.1f);
        camp_pka_step(&s_gs.modifiers[0], &s_gs, 0.1f);
    }
    INFO("PKA_base=" << s_base.modifiers[0].state[1] << " PKA_gs=" << s_gs.modifiers[0].state[1]);
    REQUIRE(s_base.modifiers[0].state[1] < 0.5f);   // базальная PKA ниже половины
    REQUIRE(s_gs.modifiers[0].state[1] > 0.9f);     // насыщенная PKA ≈ 1
}

TEST_CASE("Кросс-модуляция: segment_camp_um/segment_pka_act читают пул", "[camp_pka]") {
    Segment s = make_seg(true, 1.0f);
    for (int t = 0; t < 5000; t++) camp_pka_step(&s.modifiers[0], &s, 0.1f);
    REQUIRE(segment_camp_um(&s) == Approx(s.modifiers[0].state[0]).epsilon(1e-6));
    REQUIRE(segment_pka_act(&s) == Approx(s.modifiers[0].state[1]).epsilon(1e-6));

    // Без каскада — default (базальный cAMP, PKA=0)
    Segment no_cascade = make_seg(false, 0.0f);
    no_cascade.modifier_count = 0;
    REQUIRE(segment_camp_um(&no_cascade) == Approx(CAMP_BASAL_UM).epsilon(1e-6));
    REQUIRE(segment_pka_act(&no_cascade) == Approx(0.0f).epsilon(1e-6));
}

TEST_CASE("HCN V½ сдвиг: насыщается к +15 мВ при высоком [cAMP]", "[camp_pka]") {
    // Формула из hh_biological.cpp: Ve = V_half + HCN_CAMP_VHALF_SHIFT_MV * camp/(camp+EC50)
    float camp_high = 10.0f;
    float shift_high = HCN_CAMP_VHALF_SHIFT_MV * camp_high / (camp_high + HCN_CAMP_EC50_UM);
    float camp_low = CAMP_BASAL_UM;
    float shift_low = HCN_CAMP_VHALF_SHIFT_MV * camp_low / (camp_low + HCN_CAMP_EC50_UM);
    INFO("shift_high=" << shift_high << " shift_low=" << shift_low);
    REQUIRE(shift_high > 12.0f);     // ≈ +14 мВ при насыщении
    REQUIRE(shift_low < 5.0f);       // малый сдвиг при базальном cAMP
    REQUIRE(shift_high > shift_low);
}
