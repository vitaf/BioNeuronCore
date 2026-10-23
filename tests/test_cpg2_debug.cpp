// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_cpg2_debug.cpp — build_cpg(2): кикстарт dL → трассировка dL/dR/cL/cR.
// Диагностика: запускается ли rebound контралатерального dR и переключение фазы.

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

TEST_CASE("CPG(2): kickstart dL → rebound + phase switch", "[cpg2-debug]") {
    cpg::CpgNetwork net = cpg::build_cpg(1);
    char path[256]; snprintf(path, sizeof(path), "/tmp/cpg2.bion");
    REQUIRE(cpg::save_bion(net, path) == 0);
    BionData d; memset(&d, 0, sizeof(d));
    REQUIRE(bion_load_host(path, &d) == 0);

    int dl0 = cpg::dinL_idx(0), dr0 = cpg::dinR_idx(0);
    int cl0 = cpg::cinL_idx(0), cr0 = cpg::cinR_idx(0);
    printf("[CPG2] indices: dL=%d dR=%d cL=%d cR=%d (num_seg=%d)\n",
           dl0, dr0, cl0, cr0, d.num_segments);

    uint64_t tc = 0;

    // Settle 2000 тиков.
    for (int t = 0; t < 2000; t++)
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    printf("[CPG2] rest: dL=%.2f dR=%.2f cL=%.2f cR=%.2f\n",
           d.segments[dl0].V_m, d.segments[dr0].V_m, d.segments[cl0].V_m, d.segments[cr0].V_m);

    // Kickstart dL(0): 400 пА × 50 мс.
    CurrentStimulus stim = {(uint32_t)dl0, 400.0f, 50.0f, tc};
    uint32_t stim_cnt = 1;

    int switches = 0, last_active = -1;
    int l_bursts = 0, r_bursts = 0;
    float min_dr = 200.0f;  // глубина гиперполяризации dR (для rebound)
    for (int t = 0; t < 4000; t++) {
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      &stim, &stim_cnt, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        float vl = d.segments[dl0].V_m, vr = d.segments[dr0].V_m;
        if (vr < min_dr) min_dr = vr;
        int cur = -1;
        if (vl > -20.0f) cur = 0;
        else if (vr > -20.0f) cur = 1;
        if (cur != last_active && cur >= 0 && last_active >= 0) switches++;
        if (cur == 0 && last_active != 0) l_bursts++;
        if (cur == 1 && last_active != 1) r_bursts++;
        if (cur >= 0) last_active = cur;
        if (t % 100 == 0)
            printf("[CPG2] t=%4d dL=%7.2f dR=%7.2f cL=%7.2f cR=%7.2f sw=%d\n",
                   t, vl, vr, d.segments[cl0].V_m, d.segments[cr0].V_m, switches);
    }
    printf("[CPG2] switches=%d | L bursts=%d R bursts=%d | min dR=%.2f (rebound depth)\n",
           switches, l_bursts, r_bursts, min_dr);

    CHECK(true);  // диагностический тест — просто фиксируем трассу

    bion_unload_host(&d); std::remove(path);
}
