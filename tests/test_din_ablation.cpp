// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_din_ablation.cpp — абляция модификаторов dIN: убираем по одному и смотрим
// вклад в осцилляцию (спайки) и покой. Ищем закономерности/ошибки в клетке.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstdio>
#include <cstring>
#include <vector>
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

static std::vector<std::pair<uint8_t, float>> full_mods() {
    return {{MOD_AXON_NAV12, 37.64f}, {MOD_NAP_CHANNEL, cpg::NAP_DIN}, {MOD_HCN1, cpg::PIR_HCN},
            {MOD_NT_RECEPTOR_NMDA, cpg::NMDAR_DENS}, {MOD_KV42, cpg::KV42_DENS},
            {MOD_K2P_LEAK, cpg::K2P}, {MOD_NALCN, cpg::DIN_NALCN}, {MOD_PUMP_NAK, cpg::PUMP},
            {MOD_KCC2, cpg::KCC2}, {MOD_IONOTROPIC_CL, cpg::CL_LEAK}, {MOD_CA_BUFFER, cpg::CA_BUF},
            {MOD_PMCA, cpg::PMCA_DENS}, {MOD_CONNEXIN_CX36, cpg::CX36_DENS}};
}

// Запускает одиночный dIN 1000 мс, возвращает (spikes, v_rest).
static void run_cell(const std::vector<std::pair<uint8_t, float>>& soma_mods, int* spikes, float* vrest, float vm = cpg::V_DIN) {
    cpg::CpgNetwork net;
    cpg::add_neuron(net, 0, cpg::Y_DIN, 0, cpg::R_DIN, vm,
                    soma_mods, cpg::dIN_ais_mods(), cpg::dIN_axon_mods());
    char path[256]; snprintf(path, sizeof(path), "/tmp/din_abl.bion");
    cpg::save_bion(net, path);
    BionData d; memset(&d, 0, sizeof(d));
    bion_load_host(path, &d);

    uint64_t tc = 0;
    int sp = 0; bool above = false;
    for (int t = 0; t < 10000; t++) {
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        float vm = d.segments[0].V_m;
        bool a = vm > -20.0f;
        if (a && !above) sp++;
        above = a;
    }
    *spikes = sp;
    *vrest = d.segments[0].V_m;
    bion_unload_host(&d); std::remove(path);
}

TEST_CASE("dIN modifier ablation", "[din-ablation]") {
    auto full = full_mods();
    int sp; float vr;
    run_cell(full, &sp, &vr);
    printf("[ABL] ПОЛНАЯ          → %d spikes, rest=%.2f\n", sp, vr);

    for (size_t k = 0; k < full.size(); k++) {
        auto v = full; v.erase(v.begin() + k);
        run_cell(v, &sp, &vr);
        printf("[ABL] без MOD_%3d     → %d spikes, rest=%.2f\n", full[k].first, sp, vr);
    }
    CHECK(true);
}

TEST_CASE("dIN hysteresis: начальное V_m", "[din-hysteresis]") {
    auto full = full_mods();
    int sp; float vr;
    for (float vm : {-20.0f, -40.0f, -52.0f, -60.0f, -70.0f, -75.0f, -80.0f}) {
        run_cell(full, &sp, &vr, vm);
        printf("[HYS] V0=%7.1f → %d spikes, rest=%.2f\n", vm, sp, vr);
    }
    CHECK(true);
}
