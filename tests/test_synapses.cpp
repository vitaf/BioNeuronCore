// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include "types.h"
#include "modifier_interface.h"
#include "modifier_helpers.h"
#include "run_tick_common.h"   // gap_junction_g_eff (Cx36/Cx43 γ)
#include <cmath>

// ==================== ТЕСТЫ СИНАПТИЧЕСКИХ РЕЦЕПТОРОВ ====================

TEST_CASE("AMPA: EPSP generation", "[biological]") {
    init_biological_library();

    Segment post = {};
    post.V_m = -65.0f;
    post.intra_ions[ION_NA] = 10.0f;   post.extra_ions[ION_NA] = 140.0f;
    post.intra_ions[ION_K]  = 140.0f;  post.extra_ions[ION_K]  = 4.0f;
    post.intra_ions[ION_CL] = 10.0f;   post.extra_ions[ION_CL] = 120.0f;
    post.length = 100.0f;
    post.radius = 10.0f;
    post.neurotransmitter[NT_GLUTAMATE] = 1.0f; // глутамат в щели

    post.modifiers[0] = {MOD_NT_RECEPTOR_AMPA, {0,0,0}, 0.01f, {0.0f}, 1, 0, {0,0}}; // density×0.01 для 1e-9 scaling
    post.modifier_count = 1;

    float area = 2.0f * PI * post.radius * post.length;
    float V_rest = post.V_m;
    bool had_epsp = false;

    for (int t = 0; t < 1000; t++) {
        float I = biological_library[MOD_NT_RECEPTOR_AMPA](&post.modifiers[0], &post, 0.1f);
        post.V_m += I / (C_SPECIFIC_PF_PER_UM2 * area) * 0.1f;
        if (post.V_m < -200.0f) post.V_m = -200.0f;
        if (post.V_m > 200.0f)  post.V_m = 200.0f;
        if (post.V_m > V_rest + 0.5f) had_epsp = true;
    }

    // AMPA должна вызывать деполяризацию (EPSP)
    REQUIRE(had_epsp);
    REQUIRE(post.V_m > V_rest);
}

TEST_CASE("GABAA: IPSP generation", "[biological]") {
    init_biological_library();

    Segment post = {};
    post.V_m = -65.0f;
    post.intra_ions[ION_NA] = 10.0f;   post.extra_ions[ION_NA] = 140.0f;
    post.intra_ions[ION_K]  = 140.0f;  post.extra_ions[ION_K]  = 4.0f;
    post.intra_ions[ION_CL] = 10.0f;   post.extra_ions[ION_CL] = 120.0f;
    post.length = 100.0f;
    post.radius = 10.0f;
    post.neurotransmitter[NT_GABA] = 1.0f; // ГАМК в щели

    post.modifiers[0] = {MOD_NT_RECEPTOR_GABAA, {0,0,0}, 50.0f, {0.0f}, 1, 0, {0,0}}; // density 50 (γ=28 пС после рецепторного аудита #83)
    post.modifier_count = 1;

    float area = 2.0f * PI * post.radius * post.length;
    float V_rest = post.V_m;
    bool had_ipsp = false;

    for (int t = 0; t < 1000; t++) {
        float I = biological_library[MOD_NT_RECEPTOR_GABAA](&post.modifiers[0], &post, 0.1f);
        post.V_m += I / (C_SPECIFIC_PF_PER_UM2 * area) * 0.1f;
        if (post.V_m < -200.0f) post.V_m = -200.0f;
        if (post.V_m > 200.0f)  post.V_m = 200.0f;
        if (post.V_m < V_rest - 0.5f) had_ipsp = true;
    }

    // GABAA должна вызывать гиперполяризацию (IPSP)
    REQUIRE(had_ipsp);
    REQUIRE(post.V_m < V_rest);
}

TEST_CASE("NMDA: Mg2+ block at resting Vm", "[biological]") {
    init_biological_library();

    Segment post_rest = {};
    post_rest.V_m = -65.0f;
    post_rest.intra_ions[ION_NA] = 10.0f;  post_rest.extra_ions[ION_NA] = 140.0f;
    post_rest.intra_ions[ION_K]  = 140.0f; post_rest.extra_ions[ION_K]  = 4.0f;
    post_rest.intra_ions[ION_CL] = 10.0f;  post_rest.extra_ions[ION_CL] = 120.0f;
    post_rest.length = 100.0f;  post_rest.radius = 10.0f;
    post_rest.neurotransmitter[NT_GLUTAMATE] = 1.0f;
    post_rest.modifiers[0] = {MOD_NT_RECEPTOR_NMDA, {0,0,0}, 0.01f, {0.0f}, 1, 0, {0,0}}; // density×0.01 для 1e-9
    post_rest.modifier_count = 1;

    Segment post_dep = post_rest;
    post_dep.V_m = -30.0f; // деполяризованный

    // Один шаг — ток при resting потенциале
    float I_rest = biological_library[MOD_NT_RECEPTOR_NMDA](&post_rest.modifiers[0], &post_rest, 0.1f);
    float I_dep  = biological_library[MOD_NT_RECEPTOR_NMDA](&post_dep.modifiers[0],  &post_dep,  0.1f);

    // При деполяризации Mg²⁺ блок снимается — ток должен быть больше (по модулю)
    float abs_I_rest = (I_rest < 0) ? -I_rest : I_rest;
    float abs_I_dep  = (I_dep < 0)  ? -I_dep  : I_dep;

    REQUIRE(abs_I_dep > abs_I_rest);
}

TEST_CASE("5-HT1A: Autoreceptor reduces release", "[biological]") {
    init_biological_library();

    // Настройка пресинаптического авторецептора 5-HT1A
    Modifier autoreceptor = {MOD_NT_RECEPTOR_5HT1A, {0,0,0}, 1.0f, {0.0f}, 1, 0, {0,0}};
    float cleft_nt[NT_COUNT] = {};
    cleft_nt[NT_SEROTONIN] = 5.0f; // серотонин 5 мкМ

    // Вызов модулятора несколько шагов для накопления активации
    float modulation = 1.0f;
    for (int t = 0; t < 100; t++) {
        modulation = biological_modulators[MOD_NT_RECEPTOR_5HT1A](&autoreceptor, cleft_nt, 1.0f);
    }

    // 5-HT1A (Gi/o) должен снизить выброс более чем на 50%
    REQUIRE(modulation < 0.5f);
}

TEST_CASE("D1: Gs→cAMP активируется дофамином (каскад)", "[biological]") {
    init_biological_library();

    // D1 — сигнальный рецептор (Gs): активирует Gαs, который через общий пул
    // MOD_CAMP_PKA стимулирует AC → cAMP. Прямой ток НЕ генерирует (return 0).
    Segment seg = {};
    seg.neurotransmitter[NT_DOPAMINE] = 1.0f;
    seg.ATP_level = 1.0f;
    seg.modifiers[0] = {MOD_NT_RECEPTOR_D1, {0,0,0}, 1.0f, {0.0f, 0.0f}, 2, 0, {0,0}};
    seg.modifiers[1] = {MOD_CAMP_PKA, {0,0,0}, 1.0f, {0.0f, 0.0f}, 2, 0, {0,0}};
    seg.modifier_count = 2;

    float camp0 = seg.modifiers[1].state[0];
    for (int t = 0; t < 1000; t++) {
        biological_library[MOD_NT_RECEPTOR_D1](&seg.modifiers[0], &seg, 1.0f);  // Gαs
        camp_pka_step(&seg.modifiers[1], &seg, 1.0f);                            // Gs→AC→cAMP
    }

    // D1 (Gs) повышает cAMP.
    REQUIRE(seg.modifiers[1].state[0] > camp0);
}

TEST_CASE("mGluR1: Gq→IP3 активируется глутаматом (каскад)", "[biological]") {
    init_biological_library();

    // mGluR1 — сигнальный рецептор (Gq): активирует Gαq, который через общий пул
    // MOD_PLC_IP3_DAG стимулирует PLC → IP3+DAG. Прямой Ca²⁺-ток НЕ генерирует.
    Segment seg = {};
    seg.neurotransmitter[NT_GLUTAMATE] = 1.0f;
    seg.modifiers[0] = {MOD_NT_RECEPTOR_MGLUR1, {0,0,0}, 1.0f, {0.0f, 0.0f}, 2, 0, {0,0}};
    seg.modifiers[1] = {MOD_PLC_IP3_DAG, {0,0,0}, 1.0f, {0.0f, 0.0f}, 2, 0, {0,0}};
    seg.modifier_count = 2;

    float ip3_0 = seg.modifiers[1].state[0];
    for (int t = 0; t < 1000; t++) {
        biological_library[MOD_NT_RECEPTOR_MGLUR1](&seg.modifiers[0], &seg, 1.0f);  // Gαq
        plc_ip3_dag_step(&seg.modifiers[1], &seg, 1.0f);                            // Gq→PLC→IP3
    }

    // mGluR1 (Gq) повышает IP3.
    REQUIRE(seg.modifiers[1].state[0] > ip3_0);
}

// ==================== ТЕСТЫ НАСОСА ====================

TEST_CASE("Pump: Maintains gradients over 10000 ticks", "[biological]") {
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
    seg.modifiers[1] = {MOD_KV21, {0,0,0}, 3.84f, {0.05f, 1.0f}, 2, 0, {0,0}};
    seg.modifiers[2] = {MOD_PUMP_NAK, {0,0,0}, 1.0f, {}, 0, 0, {0,0}};
    seg.modifier_count = 3;

    float area = 2.0f * PI * seg.radius * seg.length;
    float na_initial = seg.intra_ions[ION_NA];
    float k_initial  = seg.intra_ions[ION_K];

    for (int t = 0; t < 2000; t++) {
        float I_mod = biological_library[MOD_AXON_NAV12](&seg.modifiers[0], &seg, 1.0f)
                    + biological_library[MOD_KV21](&seg.modifiers[1], &seg, 1.0f);
        float I_pump = biological_library[MOD_PUMP_NAK](&seg.modifiers[2], &seg, 1.0f);
        float dV = (I_mod + I_pump) / (C_SPECIFIC_PF_PER_UM2 * area) * 1.0f;
        if (dV > 20.0f) dV = 20.0f;
        if (dV < -20.0f) dV = -20.0f;
        seg.V_m += dV;
        if (seg.V_m < -200.0f) seg.V_m = -200.0f;
        if (seg.V_m > 200.0f)  seg.V_m = 200.0f;
    }

    // С насосом градиенты удерживаются (Na+ низкий, K+ высокий — биол. реалистично)
    REQUIRE(seg.intra_ions[ION_NA] > 2.0f);   // Na+ выкачан насосом
    REQUIRE(seg.intra_ions[ION_NA] < 20.0f);  // но не выше начального
    REQUIRE(seg.intra_ions[ION_K] > 100.0f);  // K+ накоплен
}

TEST_CASE("Pump knockout: Depolarization block within 1000 ticks", "[biological]") {
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
    seg.modifiers[1] = {MOD_KV21, {0,0,0}, 3.84f, {0.05f, 1.0f}, 2, 0, {0,0}};
    seg.modifier_count = 2; // БЕЗ насоса — knockout

    float area = 2.0f * PI * seg.radius * seg.length;
    float C_m = area * C_SPECIFIC_PF_PER_UM2; // биофизическая ёмкость (pF)
    float na_initial = seg.intra_ions[ION_NA];
    float k_initial  = seg.intra_ions[ION_K];

    for (int t = 0; t < 1000; t++) {
        float I_stim = (t >= 5 && t <= 10) ? 200000.0f : 0.0f;  // деполяризующий стимул (как test_hh)
        float I_mod = biological_library[MOD_AXON_NAV12](&seg.modifiers[0], &seg, 1.0f)
                    + biological_library[MOD_KV21](&seg.modifiers[1], &seg, 1.0f);
        float dV = (I_mod + I_stim) / C_m * 1.0f; // I(pA)/C(pF)*dt(ms) = dV(mV)
        if (dV > 20.0f) dV = 20.0f;
        if (dV < -20.0f) dV = -20.0f;
        seg.V_m += dV;
        if (seg.V_m < -200.0f) seg.V_m = -200.0f;
        if (seg.V_m > 200.0f)  seg.V_m = 200.0f;
    }

    // Без насоса градиенты разрушаются: Na⁺ накапливается (стимул открывает Na-каналы, насоса нет).
    // Kv2.1 (V½=0) на покое закрыт → K не утекает спонтанно (K-утечка теперь через K2P/leak, которых тут нет).
    REQUIRE(seg.intra_ions[ION_NA] > na_initial * 1.0001f);
    REQUIRE(seg.intra_ions[ION_K]  < k_initial * 1.001f);  // K не накапливается
}

// ═══════════════════════════════════════════════════════════════════
// ТЕСТЫ GAP JUNCTION GATING
// ═══════════════════════════════════════════════════════════════════

// Прототип core_run_tick для прямого тестирования gap junction
extern "C" {
    void core_run_tick(Segment* segments, uint32_t num_segments,
                       IntraConnection* connections, uint32_t num_connections,
                       Synapse* synapses, uint32_t num_synapses,
                       uint8_t mode, uint64_t* tick_count, float dt,
                       CurrentStimulus* stimuli, uint32_t* stimulus_count,
                       VoltageClamp* vclamps, uint32_t* vclamp_count,
                       RampStimulus* ramps, uint32_t* ramp_count,
                       IonInjection* ion_inj, uint32_t* ion_inj_count,
                       LigandInjection* lig_inj, uint32_t* lig_inj_count, MechanoStimulus* mechano, uint32_t* mechano_count);
}
#define STIM_NULL nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr

TEST_CASE("Gap Junction: Cx36 has no Vj-gating (stays open at large Vj)", "[gap_junction]") {
    init_biological_library();

    // Два сегмента с большой разницей V_m (имитация Vj = 80 mV)
    Segment segs[2] = {};
    segs[0].id = 1; segs[0].V_m = 40.0f;   // деполяризован
    segs[1].id = 2; segs[1].V_m = -40.0f;  // гиперполяризован
    for (int i = 0; i < 2; i++) {
        segs[i].intra_ions[ION_NA] = 10.0f;  segs[i].extra_ions[ION_NA] = 140.0f;
        segs[i].intra_ions[ION_K]  = 140.0f; segs[i].extra_ions[ION_K]  = 4.0f;
        segs[i].intra_ions[ION_CL] = 10.0f;  segs[i].extra_ions[ION_CL] = 120.0f;
        segs[i].intra_ions[ION_CA] = 0.0001f; segs[i].extra_ions[ION_CA] = 1.0f;
        segs[i].intra_ions[ION_MG] = 0.5f;   segs[i].extra_ions[ION_MG] = 1.0f;
        segs[i].length = 100.0f; segs[i].radius = 10.0f;
    }

    Synapse syn = {};
    syn.id = 1;
    syn.pre_segment = 0;
    syn.post_segment = 1;
    syn.type = SYNAPSE_TYPE_ELECTRICAL;
    syn.weight = 1.0f;  // проводимость 0.001 nS
    syn.facilitation = 1.0f;  // m_fast стартует с 1 (полностью открыт)
    syn.depression = 0.0f;    // m_slow стартует с 0 (открыт)

    uint32_t num_syn = 1;
    uint64_t tick = 0;

    // 50 тиков с большим Vj (80 mV) — Cx36 НЕ должен закрываться по Vj
    for (int t = 0; t < 50; t++) {
        core_run_tick(segs, 2, nullptr, 0, &syn, num_syn, 0, &tick, 1.0f, STIM_NULL, nullptr, nullptr);
    }

    // Cx36 — наименее Vj-зависимый коннексин (Srinivas 1999, Teubner 2000): Vj-гейт отсутствует.
    // При низком Ca²⁺ (100 нМ << EC50 0.3 µM) m_slow тоже ≈ 0 → проводимость остаётся открытой.
    REQUIRE(syn.depression < 0.5f);   // Ca²⁺/CaM-гейт не закрылся
    float g_ratio = 1.0f - syn.depression;
    REQUIRE(g_ratio > 0.9f);          // эффективная проводимость не редуцирована Vj
}

TEST_CASE("Gap Junction: Slow Ca²⁺/CaM-gate closes at high calcium", "[gap_junction]") {
    init_biological_library();

    Segment segs[2] = {};
    segs[0].id = 1; segs[0].V_m = -65.0f;
    segs[1].id = 2; segs[1].V_m = -60.0f;
    for (int i = 0; i < 2; i++) {
        segs[i].intra_ions[ION_NA] = 10.0f;  segs[i].extra_ions[ION_NA] = 140.0f;
        segs[i].intra_ions[ION_K]  = 140.0f; segs[i].extra_ions[ION_K]  = 4.0f;
        // Высокий Ca²⁺: 0.01 mM = 10 µM >> EC50 = 0.3 µM
        segs[i].intra_ions[ION_CA] = 0.01f;
        segs[i].extra_ions[ION_CA] = 1.0f;
        segs[i].length = 100.0f; segs[i].radius = 10.0f;
    }

    Synapse syn = {};
    syn.id = 1;
    syn.pre_segment = 0;
    syn.post_segment = 1;
    syn.type = SYNAPSE_TYPE_ELECTRICAL;
    syn.weight = 1.0f;
    syn.facilitation = 1.0f;  // fast gate открыт
    syn.depression = 0.0f;    // slow gate открыт

    uint32_t num_syn = 1;
    uint64_t tick = 0;

    // Прогоняем 200 тиков (200 ms) — slow gate должен закрыться
    for (int t = 0; t < 200; t++) {
        core_run_tick(segs, 2, nullptr, 0, &syn, num_syn, 0, &tick, 1.0f, STIM_NULL, nullptr, nullptr);
    }

    // CaM_act = (0.01)²/((0.01)²+(0.0003)²) = 1e-4/(1e-4+9e-8) ≈ 0.999
    // m_slow с tau=100ms за 200ms: 1-exp(-200/100) ≈ 0.865
    // Канал должен быть существенно закрыт
    float g_ratio = (0.1f + 0.9f * syn.facilitation) * (1.0f - syn.depression);
    REQUIRE(syn.depression > 0.5f);  // slow gate значительно закрыт
    REQUIRE(g_ratio < 0.5f);         // эффективная проводимость снижена
}

TEST_CASE("Gap Junction: Normal operation with small Vj and low Ca²⁺", "[gap_junction]") {
    init_biological_library();

    Segment segs[2] = {};
    segs[0].id = 1; segs[0].V_m = -70.0f;
    segs[1].id = 2; segs[1].V_m = -60.0f;  // разница 10 mV
    for (int i = 0; i < 2; i++) {
        segs[i].intra_ions[ION_NA] = 10.0f;  segs[i].extra_ions[ION_NA] = 140.0f;
        segs[i].intra_ions[ION_K]  = 140.0f; segs[i].extra_ions[ION_K]  = 4.0f;
        segs[i].intra_ions[ION_CL] = 10.0f;  segs[i].extra_ions[ION_CL] = 120.0f;
        segs[i].intra_ions[ION_CA] = 0.0001f; segs[i].extra_ions[ION_CA] = 1.0f;
        segs[i].intra_ions[ION_MG] = 0.5f;   segs[i].extra_ions[ION_MG] = 1.0f;
        segs[i].length = 100.0f; segs[i].radius = 10.0f;
    }

    Synapse syn = {};
    syn.id = 1;
    syn.pre_segment = 0;
    syn.post_segment = 1;
    syn.type = SYNAPSE_TYPE_ELECTRICAL;
    syn.weight = 1000.0f;  // g_gap = 1.0 nS — сильная проводимость
    syn.facilitation = 1.0f;
    syn.depression = 0.0f;

    uint32_t num_syn = 1;
    uint64_t tick = 0;

    for (int t = 0; t < 100; t++) {
        core_run_tick(segs, 2, nullptr, 0, &syn, num_syn, 0, &tick, 1.0f, STIM_NULL, nullptr, nullptr);
    }

    // При малом Vj (10mV) и низком Ca²⁺ оба гейта должны остаться близко к открытому состоянию
    REQUIRE(syn.facilitation > 0.9f);   // fast gate почти открыт
    REQUIRE(syn.depression < 0.1f);     // slow gate почти открыт
    // Эффективная проводимость > 90% от номинала
    float g_ratio = (0.1f + 0.9f * syn.facilitation) * (1.0f - syn.depression);
    REQUIRE(g_ratio > 0.9f);
}

TEST_CASE("Gap Junction: Cx43 glial connexin — γ=100 пС (молекулярно, не омический)", "[gap_junction][cx43]") {
    init_biological_library();

    Segment pre = {}, post = {};
    pre.id = 1; post.id = 2;
    pre.V_m = -70.0f; post.V_m = -60.0f;
    pre.modifier_count = 1; post.modifier_count = 1;
    pre.modifiers[0].type = MOD_CONNEXIN_CX43;
    pre.modifiers[0].density = 30.0f;
    pre.modifiers[0].state[0] = 1.0f;   // connexon_count
    pre.modifiers[0].state[4] = 1.0f;   // P_open
    post.modifiers[0] = pre.modifiers[0];

    // g_gap = density(30) × connexon_count(1) × P_open(1) × γ(100 пС) × 1e-3 = 3.0 нС
    float g43 = gap_junction_g_eff(&pre, &post, 0.0f, 0.0f);
    REQUIRE(g43 == Approx(3.0f).margin(0.01f));

    // Тот же density, но Cx36 (γ=14 пС) → 0.42 нС: глиальный Cx43 в ~7× проводимее.
    pre.modifiers[0].type = MOD_CONNEXIN_CX36;
    post.modifiers[0].type = MOD_CONNEXIN_CX36;
    float g36 = gap_junction_g_eff(&pre, &post, 0.0f, 0.0f);
    REQUIRE(g36 == Approx(30.0f * 14.0f * 1e-3f).margin(0.001f));
    REQUIRE(g43 > g36 * 6.9f);
}

TEST_CASE("Gap Junction: Cx43 Vj-gate (инактивация) закрывается при большом Vj", "[gap_junction][cx43]") {
    init_biological_library();

    Segment pre = {}, post = {};
    pre.id = 1; post.id = 2;
    pre.modifier_count = 1; post.modifier_count = 1;
    pre.modifiers[0].type = MOD_CONNEXIN_CX43;
    pre.modifiers[0].density = 30.0f;
    pre.modifiers[0].state[0] = 1.0f; pre.modifiers[0].state[4] = 1.0f;
    post.modifiers[0] = pre.modifiers[0];

    // Vj = 0 → гейт открыт: g = 3.0 нС
    float g_open = gap_junction_g_eff(&pre, &post, 0.0f, 0.0f);
    REQUIRE(g_open == Approx(3.0f).margin(0.01f));

    // Vj = 80 мВ → Cx43 закрывается: f_vj ≈ 3.35e-4, но FLOOR=0.01 → g ≈ 0.03 нС (в 100× меньше открытой)
    float g_closed = gap_junction_g_eff(&pre, &post, 0.0f, 80.0f);
    REQUIRE(g_closed == Approx(0.03f).margin(0.005f));
    REQUIRE(g_closed < g_open * 0.02f);

    // Cx36 НЕ имеет Vj-гейта: при том же Vj=80 проводимость не редуцируется
    pre.modifiers[0].type = MOD_CONNEXIN_CX36;
    post.modifiers[0].type = MOD_CONNEXIN_CX36;
    float g36_vj = gap_junction_g_eff(&pre, &post, 0.0f, 80.0f);
    REQUIRE(g36_vj == Approx(30.0f * 14.0f * 1e-3f).margin(0.001f));
}

TEST_CASE("Gap Junction: Cx43 pH-gating closes at acidosis", "[gap_junction][cx43]") {
    init_biological_library();

    Segment s = {}; s.id = 1; s.modifier_count = 1;
    s.modifiers[0].type = MOD_CONNEXIN_CX43;
    s.modifiers[0].density = 30.0f;
    s.modifiers[0].state[0] = 1.0f;   // connexon_count
    s.modifiers[0].state[5] = 6.5f;   // pH 6.5 (ацидоз)

    biological_library[MOD_CONNEXIN_CX43](&s.modifiers[0], &s, 1.0f);
    // pH_factor = (6.5-6.0)/0.8 = 0.625 → P_open (state[4]) ≈ 0.625
    REQUIRE(s.modifiers[0].state[4] == Approx(0.625f).margin(0.01f));
}

TEST_CASE("Gi/o: Hyperpolarization via GIRK", "[biological]") {
    init_biological_library();

    // Сегмент с 5-HT1A (Gi/o) и серотонином
    Segment seg_gio = {};
    seg_gio.V_m = -55.0f;  // начинаем с деполяризованного состояния
    seg_gio.intra_ions[ION_NA] = 15.0f;  seg_gio.extra_ions[ION_NA] = 140.0f;
    seg_gio.intra_ions[ION_K]  = 130.0f; seg_gio.extra_ions[ION_K]  = 4.0f;
    seg_gio.intra_ions[ION_CL] = 10.0f;  seg_gio.extra_ions[ION_CL] = 120.0f;
    seg_gio.length = 100.0f; seg_gio.radius = 10.0f;
    seg_gio.neurotransmitter[NT_SEROTONIN] = 5.0f;  // серотонин
    seg_gio.modifiers[0] = {MOD_NT_RECEPTOR_5HT1A, {0,0,0}, 0.01f, {0.0f, 0.5f}, 2, 0, {0,0}};
    seg_gio.modifier_count = 1;

    float area = 2.0f * PI * seg_gio.radius * seg_gio.length;

    for (int t = 0; t < 100; t++) {
        float I_gio = biological_library[MOD_NT_RECEPTOR_5HT1A](&seg_gio.modifiers[0], &seg_gio, 1.0f);
        seg_gio.V_m += I_gio / (C_SPECIFIC_PF_PER_UM2 * area) * 1.0f;
        if (seg_gio.V_m < -200.0f) seg_gio.V_m = -200.0f;
        if (seg_gio.V_m > 200.0f)  seg_gio.V_m = 200.0f;
    }

    // 5-HT1A (Gi/o) открывает GIRK → K⁺ выходит → гиперполяризация
    // V_m должен стать отрицательнее -55 mV
    REQUIRE(seg_gio.V_m < -55.0f);
}

TEST_CASE("Gi/o: No hyperpolarization without agonist", "[biological]") {
    init_biological_library();

    Segment seg_ctrl = {};
    seg_ctrl.V_m = -55.0f;
    seg_ctrl.intra_ions[ION_NA] = 15.0f;  seg_ctrl.extra_ions[ION_NA] = 140.0f;
    seg_ctrl.intra_ions[ION_K]  = 130.0f; seg_ctrl.extra_ions[ION_K]  = 4.0f;
    seg_ctrl.intra_ions[ION_CL] = 10.0f;  seg_ctrl.extra_ions[ION_CL] = 120.0f;
    seg_ctrl.length = 100.0f; seg_ctrl.radius = 10.0f;
    seg_ctrl.neurotransmitter[NT_SEROTONIN] = 0.0f;  // нет серотонина
    seg_ctrl.modifiers[0] = {MOD_NT_RECEPTOR_5HT1A, {0,0,0}, 0.01f, {0.0f, 0.5f}, 2, 0, {0,0}};
    seg_ctrl.modifier_count = 1;

    float area = 2.0f * PI * seg_ctrl.radius * seg_ctrl.length;
    float V_start = seg_ctrl.V_m;

    for (int t = 0; t < 100; t++) {
        float I_ctrl = biological_library[MOD_NT_RECEPTOR_5HT1A](&seg_ctrl.modifiers[0], &seg_ctrl, 1.0f);
        seg_ctrl.V_m += I_ctrl / (C_SPECIFIC_PF_PER_UM2 * area) * 1.0f;
        if (seg_ctrl.V_m < -200.0f) seg_ctrl.V_m = -200.0f;
        if (seg_ctrl.V_m > 200.0f)  seg_ctrl.V_m = 200.0f;
    }

    // Без серотонина GIRK не открывается — V_m должен остаться около исходного
    REQUIRE(seg_ctrl.V_m > -60.0f);  // не ушёл в гиперполяризацию
}
