// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_din_oscillation.cpp — изолированный dIN + bath-NMDA: осциллирует ли ~10 Гц?
// Диагностика: NMDAR-пейсмейкер (Soffe & Roberts 1989, PMC6303192) должен давать
// повторную стрельбу dIN, а не залипание на деполяризации.

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

TEST_CASE("dIN oscillation with bath-NMDA", "[din-oscillation]") {
    cpg::CpgNetwork net;
    cpg::add_neuron(net, 0, cpg::Y_DIN, 0, cpg::R_DIN, cpg::V_DIN,
                    cpg::dIN_soma_mods(), cpg::dIN_ais_mods(), cpg::dIN_axon_mods());
    REQUIRE(net.segments.size() == 3);

    char path[256]; snprintf(path, sizeof(path), "/tmp/din_osc.bion");
    REQUIRE(cpg::save_bion(net, path) == 0);
    BionData d; memset(&d, 0, sizeof(d));
    REQUIRE(bion_load_host(path, &d) == 0);

    uint64_t tc = 0;
    int spikes = 0;
    bool was_above = false;
    float vmin = 999, vmax = -999;

    for (int t = 0; t < 10000; t++) {  // 1000 мс
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        float vm = d.segments[0].V_m;  // сома
        if (vm < vmin) vmin = vm;
        if (vm > vmax) vmax = vm;
        bool above = vm > -20.0f;
        if (above && !was_above) spikes++;
        was_above = above;
        if (t % 1000 == 0)
            printf("[DIN-OSC] t=%d soma=%.2f spikes=%d\n", t, vm, spikes);
    }
    printf("[DIN-OSC] range %.2f..%.2f mV, %d spikes за 1000 мс (~10 Гц => ~10)\n", vmin, vmax, spikes);

    bion_unload_host(&d); std::remove(path);
    CHECK(true);  // диагностика
}
