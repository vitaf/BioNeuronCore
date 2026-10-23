// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_syn_nan.cpp — бисекция NaN: полный набор синапсов (d→c, c→d, c↔c, d→m, RB→dIN)
// на half-center (dL/dR/cL/cR) + aIN/MN/RB + глия + CSF. Ловит, какой синапс даёт NaN.

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

static void add_synapse(cpg::CpgNetwork& net, uint32_t pre, uint32_t post,
                        float weight, float rp, float amount, float tau, float rrp,
                        int nrec, uint8_t rt0, float d0, uint8_t rt1, float d1) {
    Synapse s = {};
    s.id = (uint32_t)net.synapses.size();
    s.pre_segment = pre; s.post_segment = post;
    s.type = SYNAPSE_TYPE_CHEMICAL;
    s.weight = weight; s.release_probability = rp; s.release_amount = amount;
    s.tau_clearance = tau; s.pre_area = cpg::PRE_AREA; s.post_area = cpg::POST_AREA;
    s.RRP = rrp; s.RRP_max = rrp; s.RRP_recovery = (rt0 == MOD_NT_RECEPTOR_GLYCINE) ? cpg::RRP_GLY_REC : cpg::RRP_AMPA_REC;
    s.post_receptor_count = (uint8_t)nrec;
    s.post_receptors[0].type = rt0; s.post_receptors[0].density = d0;
    if (nrec > 1) { s.post_receptors[1].type = rt1; s.post_receptors[1].density = d1; }
    s.bouton_Ca = 0.0001f;
    net.synapses.push_back(s);
}

// Конфиги: dL=0..2, cL=3..5, dR=6..8, cR=9..11, aL=12..14, aR=15..17,
//          mL=18..20, mR=21..23, rbL=24..26, rbR=27..29
static int settle(const char* tag, int which) {
    cpg::CpgNetwork net;
    cpg::add_neuron(net, -35, cpg::Y_DIN, 0, cpg::R_DIN, cpg::V_DIN, cpg::dIN_soma_mods(), cpg::dIN_ais_mods(), cpg::dIN_axon_mods()); // dL 0-2
    cpg::add_neuron(net, -35, cpg::Y_CIN, 0, cpg::R_CIN, cpg::V_CIN, cpg::cIN_soma_mods(), cpg::cIN_ais_mods(), cpg::cIN_axon_mods()); // cL 3-5
    cpg::add_neuron(net,  35, cpg::Y_DIN, 0, cpg::R_DIN, cpg::V_DIN, cpg::dIN_soma_mods(), cpg::dIN_ais_mods(), cpg::dIN_axon_mods()); // dR 6-8
    cpg::add_neuron(net,  35, cpg::Y_CIN, 0, cpg::R_CIN, cpg::V_CIN, cpg::cIN_soma_mods(), cpg::cIN_ais_mods(), cpg::cIN_axon_mods()); // cR 9-11
    cpg::add_neuron(net, -35, cpg::Y_AIN, 0, cpg::R_AIN, cpg::V_AIN, cpg::aIN_soma_mods(), cpg::aIN_ais_mods(), cpg::aIN_axon_mods()); // aL 12-14
    cpg::add_neuron(net,  35, cpg::Y_AIN, 0, cpg::R_AIN, cpg::V_AIN, cpg::aIN_soma_mods(), cpg::aIN_ais_mods(), cpg::aIN_axon_mods()); // aR 15-17
    cpg::add_neuron(net, -35, cpg::Y_MN,  0, cpg::R_MN,  cpg::V_MN,  cpg::MN_soma_mods(),  cpg::MN_ais_mods(),  cpg::MN_axon_mods());  // mL 18-20
    cpg::add_neuron(net,  35, cpg::Y_MN,  0, cpg::R_MN,  cpg::V_MN,  cpg::MN_soma_mods(),  cpg::MN_ais_mods(),  cpg::MN_axon_mods());  // mR 21-23
    cpg::add_neuron(net, -35, cpg::Y_RB,  0, cpg::R_RB,  cpg::V_RB,  cpg::rb_soma_mods(),  cpg::rb_ais_mods(),  cpg::rb_axon_mods());  // rbL 24-26
    cpg::add_neuron(net,  35, cpg::Y_RB,  0, cpg::R_RB,  cpg::V_RB,  cpg::rb_soma_mods(),  cpg::rb_ais_mods(),  cpg::rb_axon_mods());  // rbR 27-29
    int n_neuron_segs = (int)net.segments.size();  // 30

    // Синапсы (build_cpg-параметры), поэтапно.
    if (which >= 1) {  // d→c (AMPA+NMDA)
        add_synapse(net, 2, 3, cpg::D2C_W, cpg::EXC_RP, 1.0f, cpg::EXC_TAU, cpg::RRP_AMPA_MAX, 2, MOD_NT_RECEPTOR_AMPA, cpg::AMPA_R, MOD_NT_RECEPTOR_NMDA, cpg::NMDA_R);
        add_synapse(net, 8, 9, cpg::D2C_W, cpg::EXC_RP, 1.0f, cpg::EXC_TAU, cpg::RRP_AMPA_MAX, 2, MOD_NT_RECEPTOR_AMPA, cpg::AMPA_R, MOD_NT_RECEPTOR_NMDA, cpg::NMDA_R);
    }
    if (which >= 2) {  // c→d (glycine cross)
        add_synapse(net, 5, 6, cpg::GLY_W, cpg::GLY_RP, 1.2f, cpg::GLY_TAU, cpg::RRP_GLY_MAX, 1, MOD_NT_RECEPTOR_GLYCINE, cpg::GLY_REC, 0, 0);
        add_synapse(net, 11, 0, cpg::GLY_W, cpg::GLY_RP, 1.2f, cpg::GLY_TAU, cpg::RRP_GLY_MAX, 1, MOD_NT_RECEPTOR_GLYCINE, cpg::GLY_REC, 0, 0);
    }
    if (which >= 3) {  // c↔c (glycine reciprocal)
        add_synapse(net, 5, 9, cpg::C2C_W, cpg::GLY_RP, 1.0f, cpg::GLY_TAU, cpg::RRP_GLY_MAX, 1, MOD_NT_RECEPTOR_GLYCINE, cpg::C2C_GLY_REC, 0, 0);
        add_synapse(net, 11, 3, cpg::C2C_W, cpg::GLY_RP, 1.0f, cpg::GLY_TAU, cpg::RRP_GLY_MAX, 1, MOD_NT_RECEPTOR_GLYCINE, cpg::C2C_GLY_REC, 0, 0);
    }
    if (which >= 4) {  // d→m (AMPA)
        add_synapse(net, 2, 18, cpg::D2M_W, cpg::MN_RP, 1.2f, cpg::MN_TAU, cpg::RRP_AMPA_MAX, 1, MOD_NT_RECEPTOR_AMPA, cpg::MN_AMPA, 0, 0);
        add_synapse(net, 8, 21, cpg::D2M_W, cpg::MN_RP, 1.2f, cpg::MN_TAU, cpg::RRP_AMPA_MAX, 1, MOD_NT_RECEPTOR_AMPA, cpg::MN_AMPA, 0, 0);
    }
    if (which >= 5) {  // RB→dIN (AMPA)
        add_synapse(net, 26, 0, cpg::RB_DIN_W, cpg::EXC_RP, 1.0f, cpg::EXC_TAU, cpg::RRP_AMPA_MAX, 1, MOD_NT_RECEPTOR_AMPA, cpg::RB_DIN_AMPA, 0, 0);
        add_synapse(net, 29, 6, cpg::RB_DIN_W, cpg::EXC_RP, 1.0f, cpg::EXC_TAU, cpg::RRP_AMPA_MAX, 1, MOD_NT_RECEPTOR_AMPA, cpg::RB_DIN_AMPA, 0, 0);
    }

    // Глия + CSF (12 глии — как build_cpg: 6 L + 6 R)
    for (int g = 0; g < 6; g++) {
        cpg::add_radial_glia(net, -15.0f + g * 7.0f, cpg::Y_GLIA, 0.0f);
        cpg::add_radial_glia(net, 5.0f + g * 7.0f, cpg::Y_GLIA, 0.0f);
    }
    int total = (int)net.segments.size();
    for (int g = n_neuron_segs; g < total; g++)
        for (int n = 0; n < total; n++) {
            IntraConnection c = {}; c.segment_a = (uint32_t)g; c.segment_b = (uint32_t)n;
            c.is_intracellular = 0; net.connections.push_back(c);
        }
    // Синцитий (intra) ОТКЛЮЧЁН для изоляции — только EXTRA_ONLY.
    // for (int g1 = n_neuron_segs; g1 < total; g1++)
    //     for (int g2 = g1 + 1; g2 < total; g2++) {
    //         IntraConnection c = {}; c.segment_a = (uint32_t)g1; c.segment_b = (uint32_t)g2;
    //         c.is_intracellular = 1; net.connections.push_back(c);
    //     }
    cpg::add_csf(net, 0.0f, 0.0f, 0.0f);
    uint32_t csf_id = (uint32_t)(net.segments.size() - 1);
    for (uint32_t i = 0; i < net.segments.size(); i++)
        if (net.segments[i].neuron_id == cpg::GLIA_RADIAL_ID && net.segments[i].segment_type == SEG_TYPE_SOMA) {
            IntraConnection c = {}; c.segment_a = i; c.segment_b = csf_id;
            c.is_intracellular = 0; net.connections.push_back(c);
        }

    char path[256]; snprintf(path, sizeof(path), "/tmp/syn_nan.bion");
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
    int nan_seg = -1;
    for (uint32_t i = 0; i < d.num_segments; i++)
        if (fabsf(d.segments[i].V_m + 65.0f) < 0.01f) { nan_seg = (int)i; break; }
    printf("[SYN-NAN] %-18s dIN=%.2f | firstNaN_seg=%d\n", tag, v0, nan_seg);
    // Диагностика ионов: NaN/отрицательные концентрации (intra/extra).
    if (nan_seg >= 0) {
        for (uint32_t i = 0; i < d.num_segments && i < 40; i++) {
            for (int k = 0; k < 7; k++) {
                float a = d.segments[i].intra_ions[k], b = d.segments[i].extra_ions[k];
                if (!(a == a) || !(b == b) || a < 0 || b < 0) {
                    printf("  [ION-BAD] seg=%u ion=%d intra=%.3g extra=%.3g (type=%u)\n",
                           i, k, a, b, (unsigned)d.segments[i].segment_type);
                    break;
                }
            }
        }
    }
    bion_unload_host(&d); std::remove(path);
    return v0;
}

TEST_CASE("bisect NaN: synapses", "[syn-nan]") {
    settle("no_syn", 0);
    settle("+d2c", 1);
    settle("+c2d", 2);
    settle("+c2c", 3);
    settle("+d2m", 4);
    settle("+rb2din", 5);
    CHECK(true);
}
