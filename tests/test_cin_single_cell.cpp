// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_cin_single_cell.cpp — изолированный cIN (сома+AIS+аксон).
// Методика calibration-process (земля → канон → баланс):
// (A) земля = faithful харнесс с правильным покоем → settle → замерить V_rest.
// (B) канон = полный литературный состав cIN (cpg_config.h cIN_*_mods).
// (C) баланс = порог firing: ступень тока → спайкает ли cIN, при какой амплитуде.

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

// Сома=0, AIS=1, аксон=2 (cpg::SOMA/AIS/AXON).
TEST_CASE("cIN single cell: settle → rest → firing threshold", "[cin-single-cell]") {
    cpg::CpgNetwork net;
    cpg::add_neuron(net, 0, cpg::Y_CIN, 0, cpg::R_CIN, cpg::V_CIN,
                    cpg::cIN_soma_mods(), cpg::cIN_ais_mods(), cpg::cIN_axon_mods());
    REQUIRE(net.segments.size() == 3);

    char path[256]; snprintf(path, sizeof(path), "/tmp/cin_single_cell.bion");
    REQUIRE(cpg::save_bion(net, path) == 0);
    BionData d; memset(&d, 0, sizeof(d));
    REQUIRE(bion_load_host(path, &d) == 0);
    REQUIRE(d.num_segments == 3);

    uint64_t tc = 0;

    // (A) Settle: 2000 тиков (200 мс) без стимула — релаксация к покою.
    for (int t = 0; t < 2000; t++) {
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    }
    printf("[CIN-SINGLE] rest: soma=%.2f AIS=%.2f axon=%.2f mV (V_CIN init=%.1f)\n",
           d.segments[0].V_m, d.segments[1].V_m, d.segments[2].V_m, cpg::V_CIN);

    // (C) Порог firing: ступени тока разной амплитуды на сому.
    // Каждая ступень — свежая клетка (пересобрать), иначе кумулятивный дрейф ионов.
    const float amps[] = {400.0f, 800.0f, 1200.0f, 2000.0f, 4000.0f};
    for (float amp : amps) {
        cpg::CpgNetwork net2;
        cpg::add_neuron(net2, 0, cpg::Y_CIN, 0, cpg::R_CIN, cpg::V_CIN,
                        cpg::cIN_soma_mods(), cpg::cIN_ais_mods(), cpg::cIN_axon_mods());
        char p2[256]; snprintf(p2, sizeof(p2), "/tmp/cin_%d.bion", (int)amp);
        cpg::save_bion(net2, p2);
        BionData d2; memset(&d2, 0, sizeof(d2));
        if (bion_load_host(p2, &d2) != 0) { continue; }
        uint64_t t2 = 0;
        for (int t = 0; t < 2000; t++)
            core_run_tick(d2.segments, d2.num_segments, d2.connections, d2.num_connections,
                          d2.synapses, d2.num_synapses, 0, &t2, 0.1f,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        CurrentStimulus stim = {0, amp, 50.0f, t2};
        uint32_t stim_cnt = 1;
        float peak = -200.0f; int spikes = 0; bool above = false;
        for (int t = 0; t < 500; t++) {
            core_run_tick(d2.segments, d2.num_segments, d2.connections, d2.num_connections,
                          d2.synapses, d2.num_synapses, 0, &t2, 0.1f,
                          &stim, &stim_cnt, nullptr, nullptr, nullptr, nullptr,
                          nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            if (d2.segments[0].V_m > peak) peak = d2.segments[0].V_m;
            if (!above && d2.segments[0].V_m > -20.0f) { above = true; spikes++; }
            if (above && d2.segments[0].V_m < -20.0f) above = false;
        }
        printf("[CIN-SINGLE] amp=%.0fpA → peak=%.2f mV, spikes=%d\n", amp, peak, spikes);
        bion_unload_host(&d2); std::remove(p2);
    }

    bion_unload_host(&d); std::remove(path);
}
