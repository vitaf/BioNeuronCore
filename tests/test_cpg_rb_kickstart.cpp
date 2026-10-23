// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_cpg_rb_kickstart.cpp — физиологический старт ритма через RB→dIN.
// Касание → RB (механосенсор) → глутамат → dIN → альтернация.
// Вместо CurrentStimulus на dL (экспериментальный) — ток в RB (рецепторный потенциал касания).

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
                       LigandInjection*, uint32_t*, MechanoStimulus*, uint32_t*);
}

TEST_CASE("CPG(2): RB→dIN physiological kickstart", "[cpg-rb-kickstart]") {
    cpg::CpgNetwork net = cpg::build_cpg(2);
    char path[256]; snprintf(path, sizeof(path), "/tmp/cpg_rb.bion");
    REQUIRE(cpg::save_bion(net, path) == 0);
    BionData d; memset(&d, 0, sizeof(d));
    REQUIRE(bion_load_host(path, &d) == 0);

    int dl0 = cpg::dinL_idx(0), dr0 = cpg::dinR_idx(0);
    int cl0 = cpg::cinL_idx(0), cr0 = cpg::cinR_idx(0);
    int rb0 = cpg::rbL_idx(0, 0);
    printf("[RB-KICK] num_seg=%d dL=%d dR=%d cL=%d cR=%d rbL=%d\n",
           d.num_segments, dl0, dr0, cl0, cr0, rb0);

    uint64_t tc = 0;
    for (int t = 0; t < 2000; t++)
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    printf("[RB-KICK] rest: dL=%.2f dR=%.2f cL=%.2f cR=%.2f\n",
           d.segments[dl0].V_m, d.segments[dr0].V_m, d.segments[cl0].V_m, d.segments[cr0].V_m);
    printf("[RB-KICK] extra_ions[Na,K,Cl] dL=(%.1f,%.1f,%.1f) cR=(%.1f,%.1f,%.1f) CSF=(%.1f,%.1f,%.1f)\n",
           d.segments[dl0].extra_ions[0], d.segments[dl0].extra_ions[1], d.segments[dl0].extra_ions[3],
           d.segments[cr0].extra_ions[0], d.segments[cr0].extra_ions[1], d.segments[cr0].extra_ions[3],
           d.segments[d.num_segments-1].extra_ions[0], d.segments[d.num_segments-1].extra_ions[1], d.segments[d.num_segments-1].extra_ions[3]);

    // Касание левой стороны: рецепторный ток во ВСЕ 10 RB слева.
    int n_rb = cpg::RB_PER_SIDE;
    CurrentStimulus stim[10];
    uint32_t stim_cnt = 0;
    for (int n = 0; n < n_rb; n++) {
        stim[stim_cnt] = {(uint32_t)cpg::rbL_idx(0, n), 800.0f, 50.0f, tc};
        stim_cnt++;
    }

    int switches = 0, last_active = -1;
    int l_bursts = 0, r_bursts = 0;
    for (int t = 0; t < 6000; t++) {
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      stim, &stim_cnt, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        float vl = d.segments[dl0].V_m, vr = d.segments[dr0].V_m;
        int cur = -1;
        if (vl > -20.0f) cur = 0;
        else if (vr > -20.0f) cur = 1;
        if (cur != last_active && cur >= 0 && last_active >= 0) switches++;
        if (cur == 0 && last_active != 0) l_bursts++;
        if (cur == 1 && last_active != 1) r_bursts++;
        if (cur >= 0) last_active = cur;
        if (t % 500 == 0)
            printf("[RB-KICK] t=%4d dL=%7.2f dR=%7.2f cL=%7.2f cR=%7.2f rb(soma/ais/axon)=%7.2f/%7.2f/%7.2f sw=%d\n",
                   t, vl, vr, d.segments[cl0].V_m, d.segments[cr0].V_m,
                   d.segments[rb0].V_m, d.segments[rb0+1].V_m, d.segments[rb0+2].V_m, switches);
    }
    printf("[RB-KICK] switches=%d | L bursts=%d R bursts=%d\n", switches, l_bursts, r_bursts);

    CHECK(true);
    bion_unload_host(&d); std::remove(path);
}
