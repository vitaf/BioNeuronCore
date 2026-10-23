// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_d2c_synapse.cpp — минимальная dIN→cIN сеть (одна связь AMPA+NMDA).
// Локализация разрыва: dIN кикстартуем → доходит ли AMPA-EPSP до cIN → спайкает ли cIN.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstdio>
#include <cstring>
#include "types.h"
#include "bion_loader.h"
#include "cpg_config.h"

extern "C" {
    void core_run_tick(Segment*, uint32_t, IntraConnection*, uint32_t,
                       Synapse*, uint32_t, int, uint64_t*, float,
                       CurrentStimulus*, uint32_t*, VoltageClamp*, uint32_t*,
                       RampStimulus*, uint32_t*, IonInjection*, uint32_t*,
                       LigandInjection*, uint32_t*, MechanoStimulus* mechano, uint32_t* mechano_count);
}

TEST_CASE("dIN→cIN: AMPA EPSP reaches cIN and fires", "[d2c-synapse]") {
    cpg::CpgNetwork net;
    // dIN: сома 0, AIS 1, аксон 2
    cpg::add_neuron(net, -35, cpg::Y_DIN, 0, cpg::R_DIN, cpg::V_DIN,
                    cpg::dIN_soma_mods(), cpg::dIN_ais_mods(), cpg::dIN_axon_mods());
    // cIN: сома 3, AIS 4, аксон 5
    cpg::add_neuron(net, -35, cpg::Y_CIN, 0, cpg::R_CIN, cpg::V_CIN,
                    cpg::cIN_soma_mods(), cpg::cIN_ais_mods(), cpg::cIN_axon_mods());

    // d→c синапс: dIN аксон (2) → cIN сома (3), AMPA+NMDA (как в build_cpg).
    Synapse s = {};
    s.id = 0;
    s.pre_segment = 2;
    s.post_segment = 3;
    s.type = SYNAPSE_TYPE_CHEMICAL;
    s.weight = cpg::D2C_W;
    s.release_probability = cpg::EXC_RP;
    s.release_amount = 1.0f;
    s.tau_clearance = cpg::EXC_TAU;
    s.pre_area = cpg::PRE_AREA;
    s.post_area = cpg::POST_AREA;
    s.RRP = cpg::RRP_AMPA_MAX; s.RRP_max = cpg::RRP_AMPA_MAX; s.RRP_recovery = cpg::RRP_AMPA_REC;
    s.post_receptor_count = 2;
    s.post_receptors[0].type = MOD_NT_RECEPTOR_AMPA; s.post_receptors[0].density = cpg::AMPA_R;
    s.post_receptors[1].type = MOD_NT_RECEPTOR_NMDA; s.post_receptors[1].density = cpg::NMDA_R;
    s.bouton_Ca = 0.0001f;
    net.synapses.push_back(s);

    char path[256]; snprintf(path, sizeof(path), "/tmp/d2c_synapse.bion");
    REQUIRE(cpg::save_bion(net, path) == 0);
    BionData d; memset(&d, 0, sizeof(d));
    REQUIRE(bion_load_host(path, &d) == 0);
    REQUIRE(d.num_segments == 6);
    REQUIRE(d.num_synapses == 1);

    uint64_t tc = 0;

    // Settle 2000 тиков (200 мс).
    for (int t = 0; t < 2000; t++) {
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    }
    printf("[D2C] rest: dIN=%.2f cIN=%.2f mV\n", d.segments[0].V_m, d.segments[3].V_m);

    // Kickstart dIN: 400 пА × 50 мс на сому dIN (segment 0).
    CurrentStimulus stim = {0, 400.0f, 50.0f, tc};
    uint32_t stim_cnt = 1;
    float peak_din = -200.0f, peak_cin = -200.0f;
    int cin_spikes = 0; bool above = false;
    for (int t = 0; t < 800; t++) {  // 80 мс
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      &stim, &stim_cnt, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        if (d.segments[0].V_m > peak_din) peak_din = d.segments[0].V_m;
        if (d.segments[3].V_m > peak_cin) peak_cin = d.segments[3].V_m;
        if (!above && d.segments[3].V_m > -20.0f) { above = true; cin_spikes++; }
        if (above && d.segments[3].V_m < -20.0f) above = false;
        if (t % 25 == 0)
            printf("[D2C] t=%3d dIN=%7.2f cIN=%7.2f\n", t, d.segments[0].V_m, d.segments[3].V_m);
    }
    printf("[D2C] peaks: dIN=%.2f cIN=%.2f mV | cIN spikes=%d\n", peak_din, peak_cin, cin_spikes);

    // Проверка: dIN спайкает, cIN получает EPSP (деполяризация выше покоя).
    CHECK(peak_din > -20.0f);       // dIN спайкает
    CHECK(peak_cin > d.segments[3].V_m); // cIN деполяризовался (EPSP дошёл)

    bion_unload_host(&d); std::remove(path);
}

// dIN→dIN рекуррентный фон: сколько деполяризации даёт ОДИН d→d вход (AMPA+NMDA, per-synapse n_dd=5),
// и тоническое суммирование при 10 Гц (медленный NMDA-EPSP ~50 мс). Цель: сравнить с требуемыми
// +120 pA steady depolarisation (скан [rebound-drive]).
TEST_CASE("dIN→dIN: d→d AMPA+NMDA EPSP + tonic sum", "[d2d-synapse]") {
    cpg::CpgNetwork net;
    cpg::add_neuron(net, -35, cpg::Y_DIN, 0, cpg::R_DIN, cpg::V_DIN,
                    cpg::dIN_soma_mods(), cpg::dIN_ais_mods(), cpg::dIN_axon_mods());  // dA: 0,1,2
    cpg::add_neuron(net, -35, cpg::Y_DIN, 150, cpg::R_DIN, cpg::V_DIN,
                    cpg::dIN_soma_mods(), cpg::dIN_ais_mods(), cpg::dIN_axon_mods());  // dB: 3,4,5

    const float n_dd = 5.0f;  // конвергенция d→d (n_seg=2 × 3 dIN/сторону − 1)
    Synapse s = {};
    s.id = 0;
    s.pre_segment = 2;   // аксон dA
    s.post_segment = 3;  // сома dB
    s.type = SYNAPSE_TYPE_CHEMICAL;
    s.weight = cpg::D2D_W;
    s.release_probability = cpg::EXC_RP;
    s.release_amount = 1.0f;
    s.tau_clearance = cpg::EXC_TAU;
    s.pre_area = cpg::PRE_AREA;
    s.post_area = cpg::POST_AREA;
    s.RRP = cpg::RRP_AMPA_MAX; s.RRP_max = cpg::RRP_AMPA_MAX; s.RRP_recovery = cpg::RRP_AMPA_REC;
    s.post_receptor_count = 2;
    s.post_receptors[0].type = MOD_NT_RECEPTOR_AMPA; s.post_receptors[0].density = cpg::D2D_AMPA_R / n_dd;
    s.post_receptors[1].type = MOD_NT_RECEPTOR_NMDA; s.post_receptors[1].density = cpg::D2D_NMDA_R / n_dd;
    s.bouton_Ca = 0.0001f;
    net.synapses.push_back(s);

    char path[256]; snprintf(path, sizeof(path), "/tmp/d2d_synapse.bion");
    REQUIRE(cpg::save_bion(net, path) == 0);
    BionData d; memset(&d, 0, sizeof(d));
    REQUIRE(bion_load_host(path, &d) == 0);

    uint64_t tc = 0;
    for (int t = 0; t < 2000; t++)  // settle 200 мс
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    float rest_dB = d.segments[3].V_m;
    printf("[D2D] rest: dA=%.2f dB=%.2f mV\n", d.segments[0].V_m, rest_dB);

    // A) одиночный пре-спайк: пик EPSP на dB
    CurrentStimulus one = {0, 400.0f, 50.0f, tc};
    uint32_t oc = 1;
    float peak_epsP = -200.0f;
    for (int t = 0; t < 800; t++) {
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      &one, &oc, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        if (d.segments[3].V_m > peak_epsP) peak_epsP = d.segments[3].V_m;
    }
    printf("[D2D] single-spike EPSP peak on dB = %.2f mV (rest %.2f, Δ=%.2f)\n",
           peak_epsP, rest_dB, peak_epsP - rest_dB);

    // B) повторные пре-спайки 10 Гц × 20 → тонический NMDA-фон на dB
    const int N_P = 20;
    CurrentStimulus burst[N_P];
    for (int i = 0; i < N_P; i++) burst[i] = {0, 400.0f, 50.0f, tc + i * 100.0f};
    uint32_t bc = N_P;
    float peak_tonic = -200.0f, end_tonic = -200.0f;
    for (int t = 0; t < 30000; t++) {  // 3000 мс
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      burst, &bc, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        if (d.segments[3].V_m > peak_tonic) peak_tonic = d.segments[3].V_m;
        end_tonic = d.segments[3].V_m;
    }
    printf("[D2D] tonic 10Hz×20: peak=%.2f end=%.2f (rest %.2f)\n",
           peak_tonic, end_tonic, rest_dB);

    bion_unload_host(&d); std::remove(path);
}

// Скан силы NMDA в d→d: накапливается ли тонический фон при усиленном NMDA (Mg²⁺-блок vs сила)?
TEST_CASE("dIN→dIN: d→d NMDA strength scan (Mg block vs strength)", "[d2d-synapse]") {
    for (float nmda_mult : {1.0f, 5.0f, 20.0f}) {
        cpg::CpgNetwork net;
        cpg::add_neuron(net, -35, cpg::Y_DIN, 0, cpg::R_DIN, cpg::V_DIN,
                        cpg::dIN_soma_mods(), cpg::dIN_ais_mods(), cpg::dIN_axon_mods());  // dA: 0,1,2
        cpg::add_neuron(net, -35, cpg::Y_DIN, 150, cpg::R_DIN, cpg::V_DIN,
                        cpg::dIN_soma_mods(), cpg::dIN_ais_mods(), cpg::dIN_axon_mods());  // dB: 3,4,5

        const float n_dd = 5.0f;
        Synapse s = {};
        s.id = 0; s.pre_segment = 2; s.post_segment = 3;
        s.type = SYNAPSE_TYPE_CHEMICAL;
        s.weight = cpg::D2D_W; s.release_probability = cpg::EXC_RP;
        s.release_amount = 1.0f; s.tau_clearance = cpg::EXC_TAU;
        s.pre_area = cpg::PRE_AREA; s.post_area = cpg::POST_AREA;
        s.RRP = cpg::RRP_AMPA_MAX; s.RRP_max = cpg::RRP_AMPA_MAX; s.RRP_recovery = cpg::RRP_AMPA_REC;
        s.post_receptor_count = 2;
        s.post_receptors[0].type = MOD_NT_RECEPTOR_AMPA; s.post_receptors[0].density = cpg::D2D_AMPA_R / n_dd;
        s.post_receptors[1].type = MOD_NT_RECEPTOR_NMDA; s.post_receptors[1].density = cpg::D2D_NMDA_R / n_dd * nmda_mult;
        s.bouton_Ca = 0.0001f;
        net.synapses.push_back(s);

        char path[256]; snprintf(path, sizeof(path), "/tmp/d2d_synapse.bion");
        REQUIRE(cpg::save_bion(net, path) == 0);
        BionData d; memset(&d, 0, sizeof(d));
        REQUIRE(bion_load_host(path, &d) == 0);

        uint64_t tc = 0;
        for (int t = 0; t < 2000; t++)
            core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                          d.synapses, d.num_synapses, 0, &tc, 0.1f,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        float rest_dB = d.segments[3].V_m;

        const int N_P = 20;
        CurrentStimulus burst[N_P];
        for (int i = 0; i < N_P; i++) burst[i] = {0, 400.0f, 50.0f, tc + i * 100.0f};
        uint32_t bc = N_P;
        float peak = -200.0f, end = -200.0f;
        for (int t = 0; t < 30000; t++) {
            core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                          d.synapses, d.num_synapses, 0, &tc, 0.1f,
                          burst, &bc, nullptr, nullptr, nullptr, nullptr,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            if (d.segments[3].V_m > peak) peak = d.segments[3].V_m;
            end = d.segments[3].V_m;
        }
        printf("[D2D-NMDA] mult=%.0f | rest=%.2f peak=%.2f end=%.2f (Δend=%.2f)\n",
               nmda_mult, rest_dB, peak, end, end - rest_dB);

        bion_unload_host(&d); std::remove(path);
    }
}

// RB→dIN сенсорный вход (AMPA, НЕ Mg-блокированный): сколько деполяризации даёт активный RB?
// Цель: сравнить с требуемыми +120 pA steady depolarisation (скан [rebound-drive]).
TEST_CASE("RB→dIN: AMPA EPSP + tonic sum", "[rb-din-synapse]") {
    cpg::CpgNetwork net;
    cpg::add_neuron(net, -35, cpg::Y_RB, 0, cpg::R_RB, cpg::V_RB,
                    cpg::rb_soma_mods(), cpg::rb_ais_mods(), cpg::rb_axon_mods());   // RB: 0,1,2
    cpg::add_neuron(net, -35, cpg::Y_DIN, 0, cpg::R_DIN, cpg::V_DIN,
                    cpg::dIN_soma_mods(), cpg::dIN_ais_mods(), cpg::dIN_axon_mods()); // dIN: 3,4,5

    Synapse s = {};
    s.id = 0;
    s.pre_segment = 2;   // аксон RB
    s.post_segment = 3;  // сома dIN
    s.type = SYNAPSE_TYPE_CHEMICAL;
    s.weight = cpg::RB_DIN_W;
    s.release_probability = cpg::EXC_RP;
    s.release_amount = 1.0f;
    s.tau_clearance = cpg::EXC_TAU;
    s.pre_area = cpg::PRE_AREA;
    s.post_area = cpg::POST_AREA;
    s.RRP = cpg::RRP_AMPA_MAX; s.RRP_max = cpg::RRP_AMPA_MAX; s.RRP_recovery = cpg::RRP_AMPA_REC;
    s.post_receptor_count = 1;
    s.post_receptors[0].type = MOD_NT_RECEPTOR_AMPA; s.post_receptors[0].density = cpg::RB_DIN_AMPA;
    s.bouton_Ca = 0.0001f;
    net.synapses.push_back(s);

    char path[256]; snprintf(path, sizeof(path), "/tmp/rb_din_synapse.bion");
    REQUIRE(cpg::save_bion(net, path) == 0);
    BionData d; memset(&d, 0, sizeof(d));
    REQUIRE(bion_load_host(path, &d) == 0);

    uint64_t tc = 0;
    for (int t = 0; t < 2000; t++)  // settle 200 мс
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    float rest_dIN = d.segments[3].V_m;
    printf("[RB-DIN] rest: RB=%.2f dIN=%.2f mV\n", d.segments[0].V_m, rest_dIN);

    // A) одиночный RB-спайк → EPSP на dIN
    CurrentStimulus one = {0, 400.0f, 50.0f, tc};
    uint32_t oc = 1;
    float peak_epsP = -200.0f;
    for (int t = 0; t < 800; t++) {
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      &one, &oc, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        if (d.segments[3].V_m > peak_epsP) peak_epsP = d.segments[3].V_m;
    }
    printf("[RB-DIN] single-spike EPSP peak on dIN = %.2f mV (Δ=%.2f)\n",
           peak_epsP, peak_epsP - rest_dIN);

    // B) тонический RB-вход 10 Гц × 20 → steady depolarisation dIN
    const int N_P = 20;
    CurrentStimulus burst[N_P];
    for (int i = 0; i < N_P; i++) burst[i] = {0, 400.0f, 50.0f, tc + i * 100.0f};
    uint32_t bc = N_P;
    float peak = -200.0f, end = -200.0f;
    for (int t = 0; t < 30000; t++) {
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      burst, &bc, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        if (d.segments[3].V_m > peak) peak = d.segments[3].V_m;
        end = d.segments[3].V_m;
    }
    printf("[RB-DIN] tonic 10Hz×20: peak=%.2f end=%.2f (rest %.2f, Δend=%.2f)\n",
           peak, end, rest_dIN, end - rest_dIN);

    bion_unload_host(&d); std::remove(path);
}

// Конвергенция RB→dIN: сколько деполяризации на dIN от N одновременно активных RB (сенсорный залп)?
// В сети RB_PER_SIDE=10, каждый RB→dIN density=RB_DIN_AMPA. Цель: достичь ~+12 мВ (≈+120 pA).
TEST_CASE("RB→dIN: convergence scan (N active RB)", "[rb-din-synapse]") {
    for (int n_rb : {1, 3, 10}) {
        cpg::CpgNetwork net;
        for (int i = 0; i < n_rb; i++)
            cpg::add_neuron(net, -35, cpg::Y_RB, 0, cpg::R_RB, cpg::V_RB,
                            cpg::rb_soma_mods(), cpg::rb_ais_mods(), cpg::rb_axon_mods());
        cpg::add_neuron(net, -35, cpg::Y_DIN, 0, cpg::R_DIN, cpg::V_DIN,
                        cpg::dIN_soma_mods(), cpg::dIN_ais_mods(), cpg::dIN_axon_mods());
        uint32_t din_soma = (uint32_t)(n_rb * 3);  // dIN сома
        for (int i = 0; i < n_rb; i++) {
            Synapse s = {};
            s.id = (uint32_t)i;
            s.pre_segment = (uint32_t)(i * 3 + 2);   // аксон RB_i
            s.post_segment = din_soma;               // сома dIN
            s.type = SYNAPSE_TYPE_CHEMICAL;
            s.weight = cpg::RB_DIN_W; s.release_probability = cpg::EXC_RP;
            s.release_amount = 1.0f; s.tau_clearance = cpg::EXC_TAU;
            s.pre_area = cpg::PRE_AREA; s.post_area = cpg::POST_AREA;
            s.RRP = cpg::RRP_AMPA_MAX; s.RRP_max = cpg::RRP_AMPA_MAX; s.RRP_recovery = cpg::RRP_AMPA_REC;
            s.post_receptor_count = 1;
            s.post_receptors[0].type = MOD_NT_RECEPTOR_AMPA; s.post_receptors[0].density = cpg::RB_DIN_AMPA;
            s.bouton_Ca = 0.0001f;
            net.synapses.push_back(s);
        }

        char path[256]; snprintf(path, sizeof(path), "/tmp/rb_din_conv.bion");
        REQUIRE(cpg::save_bion(net, path) == 0);
        BionData d; memset(&d, 0, sizeof(d));
        REQUIRE(bion_load_host(path, &d) == 0);

        uint64_t tc = 0;
        for (int t = 0; t < 2000; t++)
            core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                          d.synapses, d.num_synapses, 0, &tc, 0.1f,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        float rest_dIN = d.segments[din_soma].V_m;

        // одновременный залп всех RB (400 pA × 50 мс на каждую сому RB)
        CurrentStimulus burst[10];
        for (int i = 0; i < n_rb; i++) burst[i] = {(uint32_t)(i * 3), 400.0f, 50.0f, tc};
        uint32_t bc = (uint32_t)n_rb;
        float peak = -200.0f;
        int din_spikes = 0; bool above = false;
        for (int t = 0; t < 800; t++) {
            core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                          d.synapses, d.num_synapses, 0, &tc, 0.1f,
                          burst, &bc, nullptr, nullptr, nullptr, nullptr,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            if (d.segments[din_soma].V_m > peak) peak = d.segments[din_soma].V_m;
            if (!above && d.segments[din_soma].V_m > 0.0f) { above = true; din_spikes++; }
            if (above && d.segments[din_soma].V_m < 0.0f) above = false;
        }
        printf("[RB-CONV] n_rb=%d | rest=%.2f peak=%.2f (Δ=%.2f) dIN_spikes=%d\n",
               n_rb, rest_dIN, peak, peak - rest_dIN, din_spikes);

        bion_unload_host(&d); std::remove(path);
    }
}

// Гипотеза пользователя (2026-10-08): рекуррентный d→d глутамат СУММИРУЕТСЯ → эндогенный
// аналог bath-NMDA (steady depolarisation). Проверка: 5 dIN all-to-all, зажечь dIN[0],
// распространяется ли спайк через d→d на dIN[1..4].
TEST_CASE("d→d recurrent: AMPA propagates spike through 5 dIN", "[d2d-recurrent]") {
    const int N_DIN = 5;
    for (float mult : {1.0f, 2.0f, 4.0f, 8.0f}) {
    cpg::CpgNetwork net;
    for (int i = 0; i < N_DIN; i++)
        cpg::add_neuron(net, -35, cpg::Y_DIN, i * 150.0f, cpg::R_DIN, cpg::V_DIN,
                        cpg::dIN_soma_mods(), cpg::dIN_ais_mods(), cpg::dIN_axon_mods());

    // d→d all-to-all (конвергенция N_DIN-1), AMPA+NMDA
    uint32_t sid = 0;
    for (int a = 0; a < N_DIN; a++)
        for (int b = 0; b < N_DIN; b++) {
            if (a == b) continue;
            Synapse s = {};
            s.id = sid++;
            s.pre_segment = (uint32_t)(a * 3 + 2);   // аксон dIN[a]
            s.post_segment = (uint32_t)(b * 3);      // сома dIN[b]
            s.type = SYNAPSE_TYPE_CHEMICAL;
            s.weight = cpg::D2D_W; s.release_probability = 1.0f;
            s.release_amount = 1.0f; s.tau_clearance = cpg::EXC_TAU;
            s.pre_area = cpg::PRE_AREA; s.post_area = cpg::POST_AREA;
            s.RRP = cpg::RRP_AMPA_MAX; s.RRP_max = cpg::RRP_AMPA_MAX; s.RRP_recovery = cpg::RRP_AMPA_REC;
            s.post_receptor_count = 2;
            s.post_receptors[0].type = MOD_NT_RECEPTOR_AMPA; s.post_receptors[0].density = cpg::D2D_AMPA_R / (N_DIN - 1) * mult;
            s.post_receptors[1].type = MOD_NT_RECEPTOR_NMDA; s.post_receptors[1].density = cpg::D2D_NMDA_R / (N_DIN - 1);
            s.bouton_Ca = 0.0001f;
            net.synapses.push_back(s);
        }

    char path[256]; snprintf(path, sizeof(path), "/tmp/d2d_recurrent.bion");
    REQUIRE(cpg::save_bion(net, path) == 0);
    BionData d; memset(&d, 0, sizeof(d));
    REQUIRE(bion_load_host(path, &d) == 0);

    uint64_t tc = 0;
    for (int t = 0; t < 2000; t++)
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

    // зажечь dIN[0] (400 pA × 50 мс)
    CurrentStimulus one = {0, 400.0f, 50.0f, tc};
    uint32_t oc = 1;
    int spikes[5] = {0,0,0,0,0};
    float peak[5] = {-200,-200,-200,-200,-200};
    for (int t = 0; t < 2000; t++) {
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      &one, &oc, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        for (int i = 0; i < 5; i++) {
            float v = d.segments[i * 3].V_m;
            if (v > peak[i]) peak[i] = v;
            if (v > 0.0f) spikes[i]++;
        }
    }
    fprintf(stderr, "[REC] mult=%.0f | peaks: %.1f %.1f %.1f %.1f %.1f | spikes: %d %d %d %d %d\n",
            mult, peak[0], peak[1], peak[2], peak[3], peak[4], spikes[0], spikes[1], spikes[2], spikes[3], spikes[4]);

    bion_unload_host(&d); std::remove(path);
    }
}
