// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_din_balance.cpp — таблица покой-токов изолированного dIN (сома).
// Методика calibration-process Принцип C2: для каждого модификатора сомы
// замерить фактический ток на покое → найти дисбаланс (почему ребаунд −50, не −40).

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstdio>
#include <cstring>
#include "types.h"
#include "bion_loader.h"
#include "cpg_config.h"
#include "kernel_dispatch.h"

extern "C" {
    void core_run_tick(Segment*, uint32_t, IntraConnection*, uint32_t,
                       Synapse*, uint32_t, int, uint64_t*, float,
                       CurrentStimulus*, uint32_t*, VoltageClamp*, uint32_t*,
                       RampStimulus*, uint32_t*, IonInjection*, uint32_t*,
                       LigandInjection*, uint32_t*, MechanoStimulus*, uint32_t*);
}

TEST_CASE("dIN soma: per-modifier resting current table", "[din-balance]") {
    cpg::CpgNetwork net;
    cpg::add_neuron(net, 0, cpg::Y_DIN, 0, cpg::R_DIN, cpg::V_DIN,
                    cpg::dIN_soma_mods(), cpg::dIN_ais_mods(), cpg::dIN_axon_mods());
    char path[256]; snprintf(path, sizeof(path), "/tmp/din_balance.bion");
    REQUIRE(cpg::save_bion(net, path) == 0);
    BionData d; memset(&d, 0, sizeof(d));
    REQUIRE(bion_load_host(path, &d) == 0);

    uint64_t tc = 0;
    for (int t = 0; t < 3000; t++)
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

    Segment* soma = &d.segments[0];
    printf("[DIN-BAL] soma V=%.2f mV  (I_buffer=%.4f nA)\n", soma->V_m, soma->I_buffer);
    printf("[DIN-BAL] %-22s %10s\n", "modifier(type)", "I (nA)");
    float sum = 0.0f;
    for (uint8_t j = 0; j < soma->modifier_count; j++) {
        Modifier mm = soma->modifiers[j];
        double I = dispatch_modifier_current(0, mm.type, &mm, soma, 0.1f);
        sum += (float)I;
        char tag[32]; snprintf(tag, sizeof(tag), "type_%d", (int)mm.type);
        printf("[DIN-BAL] %-22s %10.4f\n", tag, (float)I);
    }
    printf("[DIN-BAL] SUM = %.4f nA (покой: 0 = баланс)\n", sum);

    bion_unload_host(&d); std::remove(path);
}
