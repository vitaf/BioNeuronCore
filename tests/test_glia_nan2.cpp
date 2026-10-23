// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_glia_nan2.cpp — бисекция NaN: какие доп. нейроны (aIN/MN/RB) дают NaN в сети.

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

// Строит сеть: dIN+cIN+2глии+CSF + опционально aIN/MN/RB, settle, возвращает V_m dIN.
static float settle(const char* tag, bool add_ain, bool add_mn, bool add_rb) {
    cpg::CpgNetwork net;
    cpg::add_neuron(net, -35, cpg::Y_DIN, 0, cpg::R_DIN, cpg::V_DIN,
                    cpg::dIN_soma_mods(), cpg::dIN_ais_mods(), cpg::dIN_axon_mods());
    cpg::add_neuron(net, -35, cpg::Y_CIN, 0, cpg::R_CIN, cpg::V_CIN,
                    cpg::cIN_soma_mods(), cpg::cIN_ais_mods(), cpg::cIN_axon_mods());
    if (add_ain)
        cpg::add_neuron(net, -35, cpg::Y_AIN, 0, cpg::R_AIN, cpg::V_AIN,
                        cpg::aIN_soma_mods(), cpg::aIN_ais_mods(), cpg::aIN_axon_mods());
    if (add_mn)
        cpg::add_neuron(net, -35, cpg::Y_MN, 0, cpg::R_MN, cpg::V_MN,
                        cpg::MN_soma_mods(), cpg::MN_ais_mods(), cpg::MN_axon_mods());
    if (add_rb)
        for (int i = 0; i < 4; i++)
            cpg::add_neuron(net, -35, cpg::Y_RB, 0, cpg::R_RB, cpg::V_RB,
                            cpg::rb_soma_mods(), cpg::rb_ais_mods(), cpg::rb_axon_mods());
    int n_neuron_segs = (int)net.segments.size();

    cpg::add_radial_glia(net, -15.0f, cpg::Y_GLIA, 0.0f);
    cpg::add_radial_glia(net, 5.0f, cpg::Y_GLIA, 0.0f);
    int total = (int)net.segments.size();
    for (int g = n_neuron_segs; g < total; g++)
        for (int n = 0; n < total; n++) {
            IntraConnection c = {}; c.segment_a = (uint32_t)g; c.segment_b = (uint32_t)n;
            c.is_intracellular = 0; net.connections.push_back(c);
        }
    for (int g1 = n_neuron_segs; g1 < total; g1++)
        for (int g2 = g1 + 1; g2 < total; g2++) {
            IntraConnection c = {}; c.segment_a = (uint32_t)g1; c.segment_b = (uint32_t)g2;
            c.is_intracellular = 1; net.connections.push_back(c);
        }
    cpg::add_csf(net, 0.0f, 0.0f, 0.0f);
    uint32_t csf_id = (uint32_t)(net.segments.size() - 1);
    for (uint32_t i = 0; i < net.segments.size(); i++)
        if (net.segments[i].neuron_id == cpg::GLIA_RADIAL_ID && net.segments[i].segment_type == SEG_TYPE_SOMA) {
            IntraConnection c = {}; c.segment_a = i; c.segment_b = csf_id;
            c.is_intracellular = 0; net.connections.push_back(c);
        }

    char path[256]; snprintf(path, sizeof(path), "/tmp/nan2_%s.bion", tag);
    cpg::save_bion(net, path);
    BionData d; memset(&d, 0, sizeof(d));
    if (bion_load_host(path, &d) != 0) { std::remove(path); return -999.0f; }
    uint64_t tc = 0;
    for (int t = 0; t < 2000; t++)
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    float v0 = d.segments[0].V_m;
    // пройти по всем сегментам, найти первый NaN (== -65)
    int nan_seg = -1;
    for (uint32_t i = 0; i < d.num_segments; i++)
        if (fabsf(d.segments[i].V_m + 65.0f) < 0.01f) { nan_seg = (int)i; break; }
    printf("[NAN2] %-12s dIN=%.2f | firstNaN_seg=%d (type=%u)\n",
           tag, v0, nan_seg, nan_seg >= 0 ? (unsigned)d.segments[nan_seg].segment_type : 0);
    bion_unload_host(&d); std::remove(path);
    return v0;
}

TEST_CASE("bisect NaN: aIN/MN/RB", "[nan2]") {
    settle("base",        false, false, false);
    settle("+aIN",        true,  false, false);
    settle("+MN",         false, true,  false);
    settle("+RB",         false, false, true);
    settle("+aIN+MN+RB",  true,  true,  true);
    CHECK(true);
}
