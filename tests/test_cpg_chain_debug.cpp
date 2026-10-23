// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_cpg_chain_debug.cpp — диагностика цепочки d→c→dIN (где рвётся альтернация).

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
                       void*,void*,void*,void*,void*,void*,void*,void*,void*,void*, MechanoStimulus* mechano, uint32_t* mechano_count);
}

TEST_CASE("CPG chain debug: RB→dIN→cIN→ребаунд", "[cpg-chain-debug]") {
    // ТРАССИРОВКА КАСКАДА РЕБАУНДА: RB_L → dIN_L → cIN_L → (глицин) → dIN_R ребаунд (Ih+Na-де-инакт).
    auto net = cpg::build_cpg(1, 0);
    for (auto& syn : net.synapses) syn.release_probability = 1.0f;

    char path[256]; snprintf(path, sizeof(path), "/tmp/cpg_chain_debug.bion");
    cpg::save_bion(net, path);
    BionData d; memset(&d, 0, sizeof(d));
    REQUIRE(bion_load_host(path, &d) == 0);
    uint64_t tc = 0;

    int dL = cpg::dinL_idx(0), cL = cpg::cinL_idx(0);
    int dR = cpg::dinR_idx(0), cR = cpg::cinR_idx(0);

    // Триггер: касание RB_L.
    CurrentStimulus stim_rb[10];
    for (int i = 0; i < cpg::RB_PER_SIDE; i++)
        stim_rb[i] = { (uint32_t)cpg::rbL_idx(0, i), 200.0f, 5.0f, 50 };
    uint32_t stim_cnt = (uint32_t)cpg::RB_PER_SIDE;

    // Хелпер: достать state модификатора по типу из сегмента.
    auto get_state = [&](int seg_idx, uint8_t type, int st) -> float {
        for (uint8_t j = 0; j < d.segments[seg_idx].modifier_count; j++)
            if (d.segments[seg_idx].modifiers[j].type == type) return d.segments[seg_idx].modifiers[j].state[st];
        return -1.0f;
    };

    printf("[REBOUND] t dL cL dR cR\n");
    for (int t = 0; t < 3000; t++) {
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      stim_rb, &stim_cnt, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        if (t % 100 == 0)
            printf("[t=%d] dL=%.1f cL=%.1f dR=%.1f cR=%.1f\n",
                   t, d.segments[dL].V_m, d.segments[cL].V_m, d.segments[dR].V_m, d.segments[cR].V_m);
    }
    bion_unload_host(&d); std::remove(path);
    CHECK(true);
}
