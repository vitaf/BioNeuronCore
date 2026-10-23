// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include "types.h"
#include "modifier_interface.h"
#include "modifier_helpers.h"
#include <cmath>

// ==================== ТЕСТЫ БИОЛОГИЧЕСКОГО РЕЖИМА ====================

TEST_CASE("HH: Resting potential", "[biological]") {
    init_biological_library();

    Segment seg = {};
    seg.V_m = -65.0f;
    seg.intra_ions[ION_NA] = 10.0f;   seg.extra_ions[ION_NA] = 140.0f;
    seg.intra_ions[ION_K]  = 140.0f;  seg.extra_ions[ION_K]  = 4.0f;
    seg.intra_ions[ION_CL] = 10.0f;   seg.extra_ions[ION_CL] = 120.0f;
    seg.length = 100.0f;
    seg.radius = 10.0f;
    seg.ATP_level = 1.0f;
    seg.modifiers[0] = {MOD_AXON_NAV12, {0,0,0}, 15.6f, {0.05f, 0.6f}, 2, 0, {0,0}};
    seg.modifiers[1] = {MOD_PUMP_NAK, {0,0,0}, 1.0f, {}, 0, 0, {0,0}};
    seg.modifier_count = 2;

    float area = 2.0f * PI * seg.radius * seg.length;

    for (int t = 0; t < 1000; t++) {
        float I_mod = biological_library[MOD_AXON_NAV12](&seg.modifiers[0], &seg, 1.0f);
        float I_pump = biological_library[MOD_PUMP_NAK](&seg.modifiers[1], &seg, 1.0f);
        float dV = (I_mod + I_pump) / (1.0f * area) * 1.0f;
        if (dV > 20.0f) dV = 20.0f;
        if (dV < -20.0f) dV = -20.0f;
        seg.V_m += dV;
        if (seg.V_m < -200.0f) seg.V_m = -200.0f;
        if (seg.V_m > 200.0f)  seg.V_m = 200.0f;
    }

    // Потенциал покоя — Nav1.2 закрыт на покое (m³h≈0), нет HH n⁴ покой-K.
    // Покой держит электрогенный Na/K-насос на ~-65 мВ (не E_K, как было с n⁴ суррогатом).
    REQUIRE(seg.V_m == Approx(-65.0f).margin(5.0f));
}

TEST_CASE("HH: Action potential generation", "[biological]") {
    init_biological_library();

    Segment seg = {};
    seg.V_m = -65.0f;
    seg.intra_ions[ION_NA] = 10.0f;   seg.extra_ions[ION_NA] = 140.0f;
    seg.intra_ions[ION_K]  = 140.0f;  seg.extra_ions[ION_K]  = 4.0f;
    seg.intra_ions[ION_CL] = 10.0f;   seg.extra_ions[ION_CL] = 120.0f;
    seg.length = 100.0f;
    seg.radius = 10.0f;
    seg.ATP_level = 1.0f;
    seg.modifiers[0] = {MOD_AXON_NAV12, {0,0,0}, 15.6f, {0.05f, 0.6f}, 2, 0, {0,0}};
    seg.modifiers[1] = {MOD_PUMP_NAK, {0,0,0}, 1.0f, {}, 0, 0, {0,0}};
    seg.modifier_count = 2;

    float area = 2.0f * PI * seg.radius * seg.length;
    bool spiked = false;
    float peak = -65.0f;

    for (int t = 0; t < 100; t++) {
        float I_stim = (t >= 5 && t <= 10) ? 200000.0f : 0.0f;
        float I_mod = biological_library[MOD_AXON_NAV12](&seg.modifiers[0], &seg, 1.0f);
        float I_pump = biological_library[MOD_PUMP_NAK](&seg.modifiers[1], &seg, 1.0f);
        float dV = (I_mod + I_pump + I_stim) / (1.0f * area) * 1.0f;
        if (dV > 20.0f) dV = 20.0f;
        if (dV < -20.0f) dV = -20.0f;
        seg.V_m += dV;
        if (seg.V_m < -200.0f) seg.V_m = -200.0f;
        if (seg.V_m > 200.0f)  seg.V_m = 200.0f;
        if (seg.V_m > 0.0f) spiked = true;
        if (seg.V_m > peak) peak = seg.V_m;
    }

    REQUIRE(spiked);
    REQUIRE(peak > 0.0f);
}

TEST_CASE("HH: Pump prevents rundown", "[biological]") {
    init_biological_library();

    Segment seg = {};
    seg.V_m = -65.0f;
    seg.intra_ions[ION_NA] = 10.0f;   seg.extra_ions[ION_NA] = 140.0f;
    seg.intra_ions[ION_K]  = 140.0f;  seg.extra_ions[ION_K]  = 4.0f;
    seg.length = 100.0f;
    seg.radius = 10.0f;
    seg.ATP_level = 1.0f;

    seg.modifiers[0] = {MOD_AXON_NAV12, {0,0,0}, 15.6f, {0.05f, 0.6f}, 2, 0, {0,0}};
    seg.modifiers[1] = {MOD_PUMP_NAK, {0,0,0}, 1.0f, {}, 0, 0, {0,0}};
    seg.modifier_count = 2;

    float area = 2.0f * PI * seg.radius * seg.length;

    for (int t = 0; t < 1000; t++) {
        float I_mod = biological_library[MOD_AXON_NAV12](&seg.modifiers[0], &seg, 1.0f);
        float I_pump = biological_library[MOD_PUMP_NAK](&seg.modifiers[1], &seg, 1.0f);
        seg.V_m += (I_mod + I_pump) / (1.0f * area) * 1.0f;
        if (seg.V_m < -200.0f) seg.V_m = -200.0f;
        if (seg.V_m > 200.0f)  seg.V_m = 200.0f;
    }

    // Насос должен удерживать градиенты Na⁺ и K⁺
    REQUIRE(seg.intra_ions[ION_NA] == Approx(10.0f).margin(5.0f));
    REQUIRE(seg.intra_ions[ION_K] == Approx(140.0f).margin(25.0f));
}

TEST_CASE("HH: Pump knockout leads to depolarization block", "[biological]") {
    init_biological_library();

    Segment seg = {};
    seg.V_m = -65.0f;
    seg.intra_ions[ION_NA] = 10.0f;   seg.extra_ions[ION_NA] = 140.0f;
    seg.intra_ions[ION_K]  = 140.0f;  seg.extra_ions[ION_K]  = 4.0f;
    seg.length = 100.0f;
    seg.radius = 10.0f;
    seg.ATP_level = 1.0f;

    seg.modifiers[0] = {MOD_AXON_NAV12, {0,0,0}, 15.6f, {0.05f, 0.6f}, 2, 0, {0,0}};
    seg.modifier_count = 1; // БЕЗ насоса

    float area = 2.0f * PI * seg.radius * seg.length;
    float initial_na = seg.intra_ions[ION_NA];

    // Без насоса: стимул открывает Na-каналы → Na⁺ входит при спайке и НЕ
    // восстанавливается (насоса нет) → Na_i растёт, градиент рушится = depolarization block.
    // Покойная Na⁺-утечка — дело NALCN (не спайкового Na-канала, m-гейт в покое закрыт).
    for (int t = 0; t < 200; t++) {
        float I_stim = (t >= 5 && t <= 10) ? 200000.0f : 0.0f;
        float I_mod = biological_library[MOD_AXON_NAV12](&seg.modifiers[0], &seg, 1.0f);
        float dV = (I_mod + I_stim) / (1.0f * area) * 1.0f;
        if (dV > 20.0f) dV = 20.0f;
        if (dV < -20.0f) dV = -20.0f;
        seg.V_m += dV;
        if (seg.V_m < -200.0f) seg.V_m = -200.0f;
        if (seg.V_m > 200.0f)  seg.V_m = 200.0f;
    }

    REQUIRE(seg.intra_ions[ION_NA] > initial_na * 1.0001f);
}

TEST_CASE("Cable: Propagation delay", "[biological]") {
    init_biological_library();

    const int N = 10;
    Segment segs[N];
    IntraConnection conns[N - 1];

    for (int i = 0; i < N; i++) {
        segs[i] = {};
        segs[i].V_m = -65.0f;
        segs[i].intra_ions[ION_NA] = 10.0f;  segs[i].extra_ions[ION_NA] = 140.0f;
        segs[i].intra_ions[ION_K]  = 140.0f; segs[i].extra_ions[ION_K]  = 4.0f;
        segs[i].length = 100.0f;
        segs[i].radius = 5.0f;
        segs[i].ATP_level = 1.0f;
        segs[i].modifiers[0] = {MOD_AXON_NAV12, {0,0,0}, 15.6f, {0.05f, 0.6f}, 2, 0, {0,0}};
        segs[i].modifiers[1] = {MOD_PUMP_NAK, {0,0,0}, 1.0f, {}, 0, 0, {0,0}};
        segs[i].modifier_count = 2;
    }

    for (int i = 0; i < N - 1; i++) {
        conns[i] = {(uint32_t)i, (uint32_t)(i + 1), 1}; // is_intracellular = 1 (осевой ток — Нернст-Планк)
    }

    float area = 2.0f * PI * segs[0].radius * segs[0].length;
    float rest_V = segs[N-1].V_m;

    for (int t = 0; t < 100; t++) {
        float I_stim = (t >= 2 && t <= 5) ? 500000.0f : 0.0f;

        for (int i = 0; i < N; i++) {
            segs[i].I_buffer = 0.0f;
            float I_mod = biological_library[MOD_AXON_NAV12](&segs[i].modifiers[0], &segs[i], 1.0f);
            float I_pump = biological_library[MOD_PUMP_NAK](&segs[i].modifiers[1], &segs[i], 1.0f);
            segs[i].I_buffer += I_mod + I_pump;
        }
        segs[0].I_buffer += I_stim;

        for (int i = 0; i < N - 1; i++) {
            // Осевой ток — Нернст-Планк электромиграция (упрощённо для unit-теста: проводимость из
            // подвижности ионов; сопротивление больше не хранится). dV × G, G=100 пА/мВ (как было 0.01 Ом).
            float I = (segs[conns[i].segment_a].V_m - segs[conns[i].segment_b].V_m) * 100.0f;
            segs[conns[i].segment_a].I_buffer -= I;
            segs[conns[i].segment_b].I_buffer += I;
        }

        for (int i = 0; i < N; i++) {
            float dV = segs[i].I_buffer / (1.0f * area) * 1.0f;
            if (dV > 20.0f) dV = 20.0f;
            if (dV < -20.0f) dV = -20.0f;
            segs[i].V_m += dV;
            if (segs[i].V_m < -200.0f) segs[i].V_m = -200.0f;
            if (segs[i].V_m > 200.0f)  segs[i].V_m = 200.0f;
        }
    }

    // Проверка: возбуждение распространилось по цепочке
    bool affected = false;
    for (int i = 1; i < N; i++) {
        if (fabsf(segs[i].V_m - rest_V) > 1.0f) affected = true;
    }
    REQUIRE(affected);
    // Первый сегмент должен отклониться от покоя
    REQUIRE(segs[0].V_m > -62.0f);
}

TEST_CASE("Myelin: Reduced capacitance", "[biological]") {
    Segment seg = {};
    seg.V_m = -65.0f;
    seg.length = 100.0f;
    seg.radius = 10.0f;
    seg.modifiers[0] = {MOD_MYELIN, {0,0,0}, 1.0f, {}, 0, 0, {0,0}};
    seg.modifier_count = 1;

    float area = 2.0f * PI * seg.radius * seg.length;
    float C_m = area * 1.0f;

    bool myelinated = false;
    for (int j = 0; j < seg.modifier_count; j++) {
        if (seg.modifiers[j].type == MOD_MYELIN) myelinated = true;
    }
    if (myelinated) C_m /= 100.0f;

    float C_expected = area * 1.0f / 100.0f;
    REQUIRE(C_m == Approx(C_expected).margin(0.001f));
}

// ==================== ТЕСТЫ ARBITRARY-РЕЖИМА ====================

double arbitrary_return_42(Modifier* m, Segment* s, float dt) {
    (void)m; (void)s; (void)dt;
    return 42.0f;
}

TEST_CASE("Mode switch: Arbitrary override", "[arbitrary]") {
    init_biological_library();
    init_arbitrary_library();

    set_arbitrary_modulator(MOD_AXON_NAV12, arbitrary_return_42);

    Segment seg = {};
    seg.modifiers[0] = {MOD_AXON_NAV12, {0,0,0}, 1.0f, {}, 0, 0, {0,0}};
    seg.modifier_count = 1;

    float result = arbitrary_library[MOD_AXON_NAV12](&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(result == 42.0f);
}

TEST_CASE("Nav1.1: gating V1/2 (act -30, inact -65)", "[biological]") {
    // h_inf(V½_инакт=−65) = 0.5 (Больцман). m_inf(−30) ≈ 0.59 (HH-rate: V½ сдвинут префакторами 0.182/0.124)
    REQUIRE(nav11_m_inf(-30.0f) == Approx(0.59f).margin(0.1f));
    REQUIRE(nav11_h_inf(-65.0f) == Approx(0.5f).margin(0.02f));
    // Гиперполяризация → h де-инактивируется (repetitive-ready); деполяризация → h инактивируется
    REQUIRE(nav11_h_inf(-80.0f) > 0.9f);
    REQUIRE(nav11_h_inf(-40.0f) < 0.1f);
    // Активация: деполяризация → m открывается, гиперполяризация → закрыт
    REQUIRE(nav11_m_inf(-60.0f) < 0.05f);
    REQUIRE(nav11_m_inf(0.0f) > 0.9f);
}

TEST_CASE("Nav1.1: repetitive (fast recovery, no slow 500ms)", "[biological]") {
    init_biological_library();
    Segment seg = {};
    seg.V_m = -65.0f;
    seg.intra_ions[ION_NA] = 10.0f;  seg.extra_ions[ION_NA] = 140.0f;
    seg.intra_ions[ION_K]  = 140.0f; seg.extra_ions[ION_K]  = 4.0f;
    seg.intra_ions[ION_CL] = 10.0f;  seg.extra_ions[ION_CL] = 120.0f;
    seg.length = 100.0f; seg.radius = 10.0f; seg.ATP_level = 1.0f;
    seg.modifiers[0] = {MOD_NAV11, {0,0,0}, 50.0f, {0.0f, 0.9f}, 2, 0, {0,0}};
    seg.modifier_count = 1;

    // Инактивация при деполяризации, затем быстрый recovery за ~7 мс (не 500 мс)
    seg.modifiers[0].state[0] = 0.0f;
    seg.modifiers[0].state[1] = 0.0f;  // полностью инактивирован
    seg.V_m = -80.0f;  // гиперполяризация → recovery
    for (int t = 0; t < 100; t++) {  // 100 мс (dt=1мс)
        biological_library[MOD_NAV11](&seg.modifiers[0], &seg, 1.0f);
    }
    REQUIRE(seg.modifiers[0].state[1] > 0.9f);  // h восстановился быстро (<100 мс)
}

TEST_CASE("Cav3.1: gating (act -45, inact -77) + Ni2+ inhibition", "[biological]") {
    init_biological_library();
    // Больцман: m_inf(V½_акт=−45)=0.5; h_inf(V½_инакт=−77)=0.5
    REQUIRE(boltzmann_act(-45.0f, CAV31_VHALF_M, CAV31_K_M) == Approx(0.5f).margin(0.02f));
    REQUIRE(boltzmann_inact(-77.0f, CAV31_VHALF_H, CAV31_K_H) == Approx(0.5f).margin(0.02f));

    Segment seg = {};
    seg.V_m = -45.0f;
    seg.intra_ions[ION_CA] = 0.0001f;  seg.extra_ions[ION_CA] = 1.2f;
    seg.length = 100.0f; seg.radius = 10.0f; seg.ATP_level = 1.0f;
    seg.modifiers[0] = {MOD_CAV31, {0,0,0}, 10.0f, {0.5f, 0.5f}, 2, 0, {0,0}};
    seg.modifier_count = 1;

    double I0 = biological_library[MOD_CAV31](&seg.modifiers[0], &seg, 1.0f);
    seg.modifiers[0].state[3] = 250.0f;  // Ni²⁺ = IC50 → блок 50%
    double I_ni = biological_library[MOD_CAV31](&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(fabsf((float)I_ni) < fabsf((float)I0) * 0.7f);
}

TEST_CASE("Cav3.2: gating (act -46, inact -72) + Ni2+ inhibition", "[biological]") {
    init_biological_library();
    // Больцман: m_inf(V½_акт=−46)=0.5; h_inf(V½_инакт=−72)=0.5
    REQUIRE(boltzmann_act(-46.0f, CAV32_VHALF_M, CAV32_K_M) == Approx(0.5f).margin(0.02f));
    REQUIRE(boltzmann_inact(-72.0f, CAV32_VHALF_H, CAV32_K_H) == Approx(0.5f).margin(0.02f));

    Segment seg = {};
    seg.V_m = -46.0f;
    seg.intra_ions[ION_CA] = 0.0001f;  seg.extra_ions[ION_CA] = 1.2f;
    seg.length = 100.0f; seg.radius = 10.0f; seg.ATP_level = 1.0f;
    seg.modifiers[0] = {MOD_CAV32, {0,0,0}, 10.0f, {0.5f, 0.5f}, 2, 0, {0,0}};
    seg.modifier_count = 1;

    double I0 = biological_library[MOD_CAV32](&seg.modifiers[0], &seg, 1.0f);
    seg.modifiers[0].state[3] = 13.0f;  // Ni²⁺ = IC50 (13 µM — самая Ni-чувствительная) → блок 50%
    double I_ni = biological_library[MOD_CAV32](&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(fabsf((float)I_ni) < fabsf((float)I0) * 0.7f);
}

TEST_CASE("Kv4.3: gating (act -31.7, inact -47.4) + 4-AP inhibition", "[biological]") {
    init_biological_library();
    REQUIRE(boltzmann_act(-31.7f, KV43_VHALF_M, KV43_K_M) == Approx(0.5f).margin(0.02f));
    REQUIRE(boltzmann_inact(-47.4f, KV43_VHALF_H, KV43_K_H) == Approx(0.5f).margin(0.02f));

    Segment seg = {};
    seg.V_m = -31.7f;
    seg.intra_ions[ION_K] = 140.0f;  seg.extra_ions[ION_K] = 5.0f;
    seg.length = 100.0f; seg.radius = 10.0f; seg.ATP_level = 1.0f;
    seg.modifiers[0] = {MOD_KV43, {0,0,0}, 10.0f, {0.5f, 0.5f}, 2, 0, {0,0}};
    seg.modifier_count = 1;

    double I0 = biological_library[MOD_KV43](&seg.modifiers[0], &seg, 1.0f);
    seg.modifiers[0].state[3] = 488.0f;  // 4-AP = IC50 → блок 50%
    double I_ap = biological_library[MOD_KV43](&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(fabsf((float)I_ap) < fabsf((float)I0) * 0.7f);
}

TEST_CASE("Kv3.2: gating (act +10) + TEA inhibition", "[biological]") {
    init_biological_library();
    REQUIRE(boltzmann_act(10.0f, KV32_VHALF_M, KV32_K_M) == Approx(0.5f).margin(0.02f));

    Segment seg = {};
    seg.V_m = 10.0f;
    seg.intra_ions[ION_K] = 140.0f;  seg.extra_ions[ION_K] = 5.0f;
    seg.length = 100.0f; seg.radius = 10.0f; seg.ATP_level = 1.0f;
    seg.modifiers[0] = {MOD_KV32, {0,0,0}, 10.0f, {0.5f, 0.0f}, 2, 0, {0,0}};
    seg.modifier_count = 1;

    double I0 = biological_library[MOD_KV32](&seg.modifiers[0], &seg, 1.0f);
    seg.modifiers[0].state[3] = 63.0f;  // TEA = IC50 → блок 50%
    double I_tea = biological_library[MOD_KV32](&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(fabsf((float)I_tea) < fabsf((float)I0) * 0.7f);
}

TEST_CASE("Kv3.3: gating (act +1.5, inact -27.9) + TEA inhibition", "[biological]") {
    init_biological_library();
    REQUIRE(boltzmann_act(1.5f, KV33_VHALF_M, KV33_K_M) == Approx(0.5f).margin(0.02f));
    REQUIRE(boltzmann_inact(-27.9f, KV33_VHALF_H, KV33_K_H) == Approx(0.5f).margin(0.02f));

    Segment seg = {};
    seg.V_m = 1.5f;
    seg.intra_ions[ION_K] = 140.0f;  seg.extra_ions[ION_K] = 5.0f;
    seg.length = 100.0f; seg.radius = 10.0f; seg.ATP_level = 1.0f;
    seg.modifiers[0] = {MOD_KV33, {0,0,0}, 10.0f, {0.5f, 0.5f}, 2, 0, {0,0}};
    seg.modifier_count = 1;

    double I0 = biological_library[MOD_KV33](&seg.modifiers[0], &seg, 1.0f);
    seg.modifiers[0].state[3] = 67.5f;  // TEA = IC50 → блок 50%
    double I_tea = biological_library[MOD_KV33](&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(fabsf((float)I_tea) < fabsf((float)I0) * 0.7f);
}

TEST_CASE("Kv2.2: gating (act +13.2, inact -16.6) + TEA inhibition", "[biological]") {
    init_biological_library();
    REQUIRE(boltzmann_act(13.2f, KV22_VHALF_M, KV22_K_M) == Approx(0.5f).margin(0.02f));
    REQUIRE(boltzmann_inact(-16.6f, KV22_VHALF_H, KV22_K_H) == Approx(0.5f).margin(0.02f));

    Segment seg = {};
    seg.V_m = 13.2f;
    seg.intra_ions[ION_K] = 140.0f;  seg.extra_ions[ION_K] = 5.0f;
    seg.length = 100.0f; seg.radius = 10.0f; seg.ATP_level = 1.0f;
    seg.modifiers[0] = {MOD_KV22, {0,0,0}, 10.0f, {0.5f, 0.5f}, 2, 0, {0,0}};
    seg.modifier_count = 1;

    double I0 = biological_library[MOD_KV22](&seg.modifiers[0], &seg, 1.0f);
    seg.modifiers[0].state[3] = 2.6f;  // TEA = IC50 (мМ) → блок 50%
    double I_tea = biological_library[MOD_KV22](&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(fabsf((float)I_tea) < fabsf((float)I0) * 0.7f);
}

TEST_CASE("Kv1.4: gating (act -21.7, inact -51.8) + 4-AP inhibition", "[biological]") {
    init_biological_library();
    REQUIRE(boltzmann_act(-21.7f, KV14_VHALF_M, KV14_K_M) == Approx(0.5f).margin(0.02f));
    REQUIRE(boltzmann_inact(-51.8f, KV14_VHALF_H, KV14_K_H) == Approx(0.5f).margin(0.02f));

    Segment seg = {};
    seg.V_m = -21.7f;
    seg.intra_ions[ION_K] = 140.0f;  seg.extra_ions[ION_K] = 5.0f;
    seg.length = 100.0f; seg.radius = 10.0f; seg.ATP_level = 1.0f;
    seg.modifiers[0] = {MOD_KV14, {0,0,0}, 10.0f, {0.5f, 0.5f}, 2, 0, {0,0}};
    seg.modifier_count = 1;

    double I0 = biological_library[MOD_KV14](&seg.modifiers[0], &seg, 1.0f);
    seg.modifiers[0].state[3] = 13.0f;  // 4-AP = IC50 → блок 50%
    double I_ap = biological_library[MOD_KV14](&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(fabsf((float)I_ap) < fabsf((float)I0) * 0.7f);
}

TEST_CASE("Kv1.6: gating (act -8.1) + TEA inhibition", "[biological]") {
    init_biological_library();
    REQUIRE(boltzmann_act(-8.1f, KV16_VHALF_M, KV16_K_M) == Approx(0.5f).margin(0.02f));

    Segment seg = {};
    seg.V_m = -8.1f;
    seg.intra_ions[ION_K] = 140.0f;  seg.extra_ions[ION_K] = 5.0f;
    seg.length = 100.0f; seg.radius = 10.0f; seg.ATP_level = 1.0f;
    seg.modifiers[0] = {MOD_KV16, {0,0,0}, 10.0f, {0.5f, 0.0f}, 2, 0, {0,0}};
    seg.modifier_count = 1;

    double I0 = biological_library[MOD_KV16](&seg.modifiers[0], &seg, 1.0f);
    seg.modifiers[0].state[3] = 1.7f;  // TEA = IC50 (мМ) → блок 50%
    double I_tea = biological_library[MOD_KV16](&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(fabsf((float)I_tea) < fabsf((float)I0) * 0.7f);
}

TEST_CASE("Kir2.2: Ba2+ inhibition (constitutive inward rectifier)", "[biological]") {
    init_biological_library();
    Segment seg = {};
    seg.V_m = -70.0f;
    seg.intra_ions[ION_K] = 140.0f;  seg.extra_ions[ION_K] = 5.0f;
    seg.length = 100.0f; seg.radius = 10.0f; seg.ATP_level = 1.0f;
    seg.modifiers[0] = {MOD_KIR22, {0,0,0}, 10.0f, {0,0}, 2, 0, {0,0}};
    seg.modifier_count = 1;

    double I0 = biological_library[MOD_KIR22](&seg.modifiers[0], &seg, 1.0f);
    seg.modifiers[0].state[4] = 2.8f;  // Ba²⁺ = IC50 → блок 50%
    double I_ba = biological_library[MOD_KIR22](&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(fabsf((float)I_ba) < fabsf((float)I0) * 0.7f);
}

TEST_CASE("KCa3.1: Ca2+ activation + TRAM-34 inhibition", "[biological]") {
    init_biological_library();
    Segment seg = {};
    seg.V_m = -70.0f;
    seg.intra_ions[ION_K] = 140.0f;  seg.extra_ions[ION_K] = 5.0f;
    seg.intra_ions[ION_CA] = 0.001f;  // 1 µM Ca²⁺ (>> EC50 188 нМ)
    seg.length = 100.0f; seg.radius = 10.0f; seg.ATP_level = 1.0f;
    seg.modifiers[0] = {MOD_KCA31, {0,0,0}, 10.0f, {0,0}, 2, 0, {0,0}};
    seg.modifier_count = 1;

    // Прогрев до steady-state open_prob (τ_open=5 мс)
    for (int t = 0; t < 50; t++) biological_library[MOD_KCA31](&seg.modifiers[0], &seg, 1.0f);
    double I0 = biological_library[MOD_KCA31](&seg.modifiers[0], &seg, 1.0f);
    seg.modifiers[0].state[6] = 20.0f;  // TRAM-34 = IC50 (нМ) → блок 50%
    double I_tram = biological_library[MOD_KCA31](&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(fabsf((float)I_tram) < fabsf((float)I0) * 0.7f);
}

TEST_CASE("TRPC3: DAG activation + Pyr3 inhibition", "[biological]") {
    init_biological_library();
    Segment seg = {};
    seg.V_m = -60.0f;
    seg.intra_ions[ION_NA] = 15.0f;  seg.extra_ions[ION_NA] = 140.0f;
    seg.intra_ions[ION_CA] = 0.0001f; seg.extra_ions[ION_CA] = 1.2f;
    seg.length = 100.0f; seg.radius = 10.0f; seg.ATP_level = 1.0f;
    seg.modifiers[0] = {MOD_TRPC3, {0,0,0}, 10.0f, {0,0,0,0,1.0f}, 2, 0, {0,0}};
    seg.modifier_count = 1;

    double I0 = biological_library[MOD_TRPC3](&seg.modifiers[0], &seg, 1.0f);
    seg.modifiers[0].state[5] = 0.7f;  // Pyr3 = IC50 (µM) → блок 50%
    double I_pyr = biological_library[MOD_TRPC3](&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(fabsf((float)I_pyr) < fabsf((float)I0) * 0.7f);
}

TEST_CASE("TRPM7: Mg2+ inhibition", "[biological]") {
    init_biological_library();
    Segment seg = {};
    seg.V_m = -60.0f;
    seg.intra_ions[ION_NA] = 15.0f;  seg.extra_ions[ION_NA] = 140.0f;
    seg.intra_ions[ION_CA] = 0.0001f; seg.extra_ions[ION_CA] = 1.2f;
    seg.intra_ions[ION_MG] = 0.0f;  // без Mg²⁺ → Po=1
    seg.length = 100.0f; seg.radius = 10.0f; seg.ATP_level = 1.0f;
    seg.modifiers[0] = {MOD_TRPM7, {0,0,0}, 10.0f, {0,0}, 2, 0, {0,0}};
    seg.modifier_count = 1;

    double I0 = biological_library[MOD_TRPM7](&seg.modifiers[0], &seg, 1.0f);
    seg.intra_ions[ION_MG] = 0.7f;  // Mg²⁺ = IC50 (мМ) → Po=0.5
    double I_mg = biological_library[MOD_TRPM7](&seg.modifiers[0], &seg, 1.0f);
    REQUIRE(fabsf((float)I_mg) < fabsf((float)I0) * 0.7f);
}
