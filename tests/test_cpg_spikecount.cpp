// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_cpg_spikecount.cpp — считаем спайки каждого нейрона CPG (dL/cL/dR/cR)
// за 1000 мс + внеклеточный K⁺. Диагностика сетевой синхронизации.

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

TEST_CASE("CPG spike count", "[cpg-spikecount]") {
    auto net = cpg::build_cpg(2, 0);
    char path[256]; snprintf(path, sizeof(path), "/tmp/cpg_spikes.bion");
    cpg::save_bion(net, path);
    BionData d; memset(&d, 0, sizeof(d));
    REQUIRE(bion_load_host(path, &d) == 0);

    int dL = cpg::dinL_idx(0), cL = cpg::cinL_idx(0);
    int dR = cpg::dinR_idx(0), cR = cpg::cinR_idx(0);

    uint64_t tc = 0;
    int s_dL = 0, s_cL = 0, s_dR = 0, s_cR = 0;
    bool a_dL = false, a_cL = false, a_dR = false, a_cR = false;
    for (int t = 0; t < 10000; t++) {
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        auto cnt = [](float vm, bool& a){ bool x = vm > -20.0f; int r = (x && !a) ? 1 : 0; a = x; return r; };
        s_dL += cnt(d.segments[dL].V_m, a_dL);
        s_cL += cnt(d.segments[cL].V_m, a_cL);
        s_dR += cnt(d.segments[dR].V_m, a_dR);
        s_cR += cnt(d.segments[cR].V_m, a_cR);
    }
    printf("[SPIKES] dL=%d cL=%d dR=%d cR=%d\n", s_dL, s_cL, s_dR, s_cR);
    int glia = cpg::neuron_seg(0, 0, 0) + (int)(cpg::NEURONS_PER_SIDE * 2 * cpg::SEGS_PER_NEURON);
    printf("[IONS ] K_out dL=%.3f dR=%.3f  V dL=%.1f dR=%.1f | glia V=%.1f K_out=%.3f\n",
           d.segments[dL].extra_ions[1], d.segments[dR].extra_ions[1],
           d.segments[dL].V_m, d.segments[dR].V_m, d.segments[cL].V_m, d.segments[cL].intra_ions[2],
           d.segments[glia].V_m, d.segments[glia].extra_ions[1]);
    printf("[CIN  ] cL V=%.1f Ca=%.5f  (SK AHP нужен Ca>0.1)\n", d.segments[cL].V_m, d.segments[cL].intra_ions[2]);

    bion_unload_host(&d); std::remove(path);
    CHECK(true);
}
