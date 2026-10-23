// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. All rights reserved.

// test_c2d_synapse.cpp — минимальная cIN→dIN сеть (одна связь глицин).
// Локализация: cIN кикстартуем → выброс глицина → гиперполяризует ли dIN.

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

TEST_CASE("cIN→dIN: glycine release hyperpolarizes dIN", "[c2d-synapse]") {
    cpg::CpgNetwork net;
    // cIN: сома 0, AIS 1, аксон 2
    cpg::add_neuron(net, -35, cpg::Y_CIN, 0, cpg::R_CIN, cpg::V_CIN,
                    cpg::cIN_soma_mods(), cpg::cIN_ais_mods(), cpg::cIN_axon_mods());
    // dIN: сома 3, AIS 4, аксон 5
    cpg::add_neuron(net, -35, cpg::Y_DIN, 0, cpg::R_DIN, cpg::V_DIN,
                    cpg::dIN_soma_mods(), cpg::dIN_ais_mods(), cpg::dIN_axon_mods());

    // c→d синапс: cIN аксон (2) → dIN сома (3), глицин (как в build_cpg).
    Synapse s = {};
    s.id = 0;
    s.pre_segment = 2;
    s.post_segment = 3;
    s.type = SYNAPSE_TYPE_CHEMICAL;
    s.weight = cpg::GLY_W;
    s.release_probability = cpg::GLY_RP;
    s.release_amount = 1.2f;
    s.tau_clearance = cpg::GLY_TAU;
    s.pre_area = cpg::PRE_AREA;
    s.post_area = cpg::POST_AREA;
    s.RRP = cpg::RRP_GLY_MAX; s.RRP_max = cpg::RRP_GLY_MAX; s.RRP_recovery = cpg::RRP_GLY_REC;
    s.post_receptor_count = 1;
    s.post_receptors[0].type = MOD_NT_RECEPTOR_GLYCINE;
    s.post_receptors[0].density = cpg::GLY_REC;
    s.bouton_Ca = 0.0001f;
    net.synapses.push_back(s);

    char path[256]; snprintf(path, sizeof(path), "/tmp/c2d_synapse.bion");
    REQUIRE(cpg::save_bion(net, path) == 0);
    BionData d; memset(&d, 0, sizeof(d));
    REQUIRE(bion_load_host(path, &d) == 0);

    uint64_t tc = 0;

    // Settle 2000 тиков.
    for (int t = 0; t < 2000; t++) {
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    }
    float din_rest = d.segments[3].V_m;
    printf("[C2D] rest: cIN=%.2f dIN=%.2f mV\n", d.segments[0].V_m, din_rest);

    // Kickstart cIN: 400 пА × 50 мс на сому cIN (segment 0).
    CurrentStimulus stim = {0, 400.0f, 50.0f, tc};
    uint32_t stim_cnt = 1;
    float peak_cin = -200.0f, min_din = 200.0f;
    for (int t = 0; t < 800; t++) {
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      &stim, &stim_cnt, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        if (d.segments[0].V_m > peak_cin) peak_cin = d.segments[0].V_m;
        if (d.segments[3].V_m < min_din) min_din = d.segments[3].V_m;
        if (t % 25 == 0)
            printf("[C2D] t=%3d cIN=%7.2f dIN=%7.2f\n", t, d.segments[0].V_m, d.segments[3].V_m);
    }
    printf("[C2D] peak cIN=%.2f mV | dIN rest=%.2f min=%.2f (Δ=%.2f)\n",
           peak_cin, din_rest, min_din, din_rest - min_din);

    CHECK(peak_cin > -20.0f);        // cIN спайкает
    CHECK(min_din < din_rest - 2.0f); // dIN гиперполяризовался (глицин дошёл) — минимум ниже покоя на 2 мВ

    bion_unload_host(&d); std::remove(path);
}
