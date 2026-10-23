// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_glia_nan.cpp — изоляция NaN: dIN+cIN (baseline) vs +глия vs +глия+CSF.
// NaN диагностируется по V_m == -65.0 (clamp_V_m NaN→resting).

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

// Строит dIN+cIN, опционально +глия, опционально +CSF, settle, возвращает V_m dIN сомы.
static float settle_case(bool with_glia, bool with_csf, const char* tag) {
    cpg::CpgNetwork net;
    cpg::add_neuron(net, -35, cpg::Y_DIN, 0, cpg::R_DIN, cpg::V_DIN,
                    cpg::dIN_soma_mods(), cpg::dIN_ais_mods(), cpg::dIN_axon_mods());
    cpg::add_neuron(net, -35, cpg::Y_CIN, 0, cpg::R_CIN, cpg::V_CIN,
                    cpg::cIN_soma_mods(), cpg::cIN_ais_mods(), cpg::cIN_axon_mods());
    int n_neuron_segs = (int)net.segments.size();  // 6

    if (with_glia) {
        cpg::add_radial_glia(net, -15.0f, cpg::Y_GLIA, 0.0f);
        cpg::add_radial_glia(net, 5.0f, cpg::Y_GLIA, 0.0f);
        int total = (int)net.segments.size();
        // глия↔все (EXTRA_ONLY) + глия↔глия (intra синцитий), как в build_cpg.
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
        if (with_csf) {
            cpg::add_csf(net, 0.0f, 0.0f, 0.0f);
            uint32_t csf_id = (uint32_t)(net.segments.size() - 1);
            for (uint32_t i = 0; i < net.segments.size(); i++) {
                if (net.segments[i].neuron_id == cpg::GLIA_RADIAL_ID &&
                    net.segments[i].segment_type == SEG_TYPE_SOMA) {
                    IntraConnection c = {}; c.segment_a = i; c.segment_b = csf_id;
                    c.is_intracellular = 0; net.connections.push_back(c);
                }
            }
        }
    }

    char path[256]; snprintf(path, sizeof(path), "/tmp/glia_nan_%s.bion", tag);
    cpg::save_bion(net, path);
    BionData d; memset(&d, 0, sizeof(d));
    if (bion_load_host(path, &d) != 0) { std::remove(path); return -999.0f; }
    uint64_t tc = 0;
    for (int t = 0; t < 2000; t++)
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    float v_din = d.segments[0].V_m;
    float v_cin = d.segments[3].V_m;
    float v_glia = (with_glia) ? d.segments[n_neuron_segs].V_m : 0.0f;
    printf("[GLIA-NAN] %s: dIN=%.2f cIN=%.2f glia=%.2f\n", tag, v_din, v_cin, v_glia);
    bion_unload_host(&d); std::remove(path);
    return v_din;
}

TEST_CASE("glia/CSF NaN isolation", "[glia-nan]") {
    float base   = settle_case(false, false, "neurons_only");
    float glia   = settle_case(true,  false, "neurons+glia");
    float full   = settle_case(true,  true,  "neurons+glia+csf");

    // baseline без глии — не NaN (как d2c-тест)
    CHECK(fabsf(base + 65.0f) > 0.5f);  // base != -65 (не NaN)
    // Диагностика: какая конфигурация даёт -65 (NaN)?
    printf("[GLIA-NAN] NaN-флаг: base=%s glia=%s full=%s\n",
           (fabsf(base + 65.0f) < 0.5f) ? "NaN" : "ok",
           (fabsf(glia + 65.0f) < 0.5f) ? "NaN" : "ok",
           (fabsf(full + 65.0f) < 0.5f) ? "NaN" : "ok");
    CHECK(true);
}
