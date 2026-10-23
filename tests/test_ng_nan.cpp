// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_ng_nan.cpp — минимальный репро NaN: 1 нейрон (каждого типа) + 12 глии + CSF.

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

static bool has_nan(const BionData& d) {
    for (uint32_t i = 0; i < d.num_segments; i++)
        for (int k = 0; k < 7; k++)
            if (!(d.segments[i].intra_ions[k] == d.segments[i].intra_ions[k]) ||
                !(d.segments[i].extra_ions[k] == d.segments[i].extra_ions[k]))
                return true;
    return false;
}

// 1 нейрон (soma/ais/axon mods) + 12 глии + CSF.
static bool ng(const char* tag,
               const std::vector<std::pair<uint8_t,float>>& sm,
               const std::vector<std::pair<uint8_t,float>>& am,
               const std::vector<std::pair<uint8_t,float>>& xm,
               float y, float r, float Vm, int n_per_side = 6) {
    cpg::CpgNetwork net;
    cpg::add_neuron(net, -35, y, 0, r, Vm, sm, am, xm);
    int n_neuron = (int)net.segments.size();  // 3
    for (int g = 0; g < n_per_side; g++)
        for (int side = 0; side < 2; side++) {
            float x = (side==0) ? (-15.0f+g*7.0f) : (5.0f+g*7.0f);
            cpg::add_radial_glia(net, x, cpg::Y_GLIA, 0.0f);
        }
    int total = (int)net.segments.size();
    for (int g = n_neuron; g < total; g++)
        for (int n = 0; n < total; n++) {
            IntraConnection c = {}; c.segment_a=(uint32_t)g; c.segment_b=(uint32_t)n; c.is_intracellular=0; net.connections.push_back(c);
        }
    for (int g1 = n_neuron; g1 < total; g1++)
        for (int g2 = g1+1; g2 < total; g2++) {
            IntraConnection c = {}; c.segment_a=(uint32_t)g1; c.segment_b=(uint32_t)g2; c.is_intracellular=1; net.connections.push_back(c);
        }
    cpg::add_csf(net, 0, 0, 0);
    uint32_t csf_id=(uint32_t)(net.segments.size()-1);
    for (uint32_t i=0;i<net.segments.size();i++)
        if (net.segments[i].neuron_id==cpg::GLIA_RADIAL_ID && net.segments[i].segment_type==SEG_TYPE_SOMA) {
            IntraConnection c={}; c.segment_a=i; c.segment_b=csf_id; c.is_intracellular=0; net.connections.push_back(c);
        }

    char path[256]; snprintf(path,sizeof(path),"/tmp/ng.bion");
    cpg::save_bion(net, path);
    BionData d; memset(&d,0,sizeof(d));
    if (bion_load_host(path,&d)!=0) { std::remove(path); return false; }
    uint64_t tc=0;
    int first_nan_tick=-1; int first_nan_ion=-1; int first_nan_seg=-1;
    for (int t=0;t<2000;t++) {
        core_run_tick(d.segments,d.num_segments,d.connections,d.num_connections,
                      d.synapses,d.num_synapses,0,&tc,0.1f,
                      nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,
                      nullptr,nullptr,nullptr,nullptr,nullptr,nullptr);
        if (t>=690 && t<=715 && (t%5==0))
            printf("[TRACE] t=%d CSF_Na=%.4g CSF_K=%.4g | glia3_Na=%.4g glia3_K=%.4g | n0_Na=%.4g n0_K=%.4g | n0_Vm=%.2f\n",
                   t, d.segments[27].extra_ions[0], d.segments[27].extra_ions[1],
                   d.segments[3].extra_ions[0], d.segments[3].extra_ions[1],
                   d.segments[0].extra_ions[0], d.segments[0].extra_ions[1],
                   d.segments[0].V_m);
        if (first_nan_tick < 0) {
            for (uint32_t i=0;i<d.num_segments;i++)
                for (int k=0;k<7;k++)
                    if (!(d.segments[i].intra_ions[k]==d.segments[i].intra_ions[k]) ||
                        !(d.segments[i].extra_ions[k]==d.segments[i].extra_ions[k])) {
                        first_nan_tick=t; first_nan_ion=k; first_nan_seg=(int)i; break;
                    }
            if (first_nan_tick>=0) break;
        }
    }
    if (first_nan_tick>=0) {
        // дамп всех ионов первого NaN-сегмента + I_buffer/V_m соседних
        const char* names[7]={"Na","K","Ca","Cl","Mg","A","H"};
        printf("[NG-NAN] %-8s NaN на тике %d, seg=%d:\n", tag, first_nan_tick, first_nan_seg);
        for (int k=0;k<7;k++)
            printf("   %s: intra=%.4g extra=%.4g\n", names[k], d.segments[first_nan_seg].intra_ions[k], d.segments[first_nan_seg].extra_ions[k]);
        // I_buffer и V_m всех сегментов (ищем Inf/NaN в I_buffer)
        for (uint32_t i=0;i<d.num_segments;i++) {
            double ib = d.segments[i].I_buffer, vm = d.segments[i].V_m;
            if (!(ib==ib) || ib > 1e6 || ib < -1e6 || !(vm==vm))
                printf("   [IBUF] seg=%u type=%u V_m=%.4g I_buffer=%.4g\n", i, (unsigned)d.segments[i].segment_type, vm, ib);
        }
        // Дренаж extra Na/K по всем сегментам (кто ушёл в ~0 или NaN)
        for (uint32_t i=0;i<d.num_segments;i++) {
            double eNa = d.segments[i].extra_ions[0], eK = d.segments[i].extra_ions[1];
            if (!(eNa==eNa) || !(eK==eK) || eNa < 1e-3 || eK < 1e-3)
                printf("   [DRAIN] seg=%u type=%u extra_Na=%.4g extra_K=%.4g\n", i, (unsigned)d.segments[i].segment_type, eNa, eK);
        }
    } else {
        printf("[NG-NAN] %-8s NaN=нет\n", tag);
    }
    bion_unload_host(&d); std::remove(path);
    return first_nan_tick>=0;
}

TEST_CASE("1 neuron + 12 glia NaN", "[ng-nan]") {
    ng("dIN", cpg::dIN_soma_mods(), cpg::dIN_ais_mods(), cpg::dIN_axon_mods(), cpg::Y_DIN, cpg::R_DIN, cpg::V_DIN);
    ng("cIN", cpg::cIN_soma_mods(), cpg::cIN_ais_mods(), cpg::cIN_axon_mods(), cpg::Y_CIN, cpg::R_CIN, cpg::V_CIN);
    ng("aIN", cpg::aIN_soma_mods(), cpg::aIN_ais_mods(), cpg::aIN_axon_mods(), cpg::Y_AIN, cpg::R_AIN, cpg::V_AIN);
    ng("MN",  cpg::MN_soma_mods(),  cpg::MN_ais_mods(),  cpg::MN_axon_mods(),  cpg::Y_MN,  cpg::R_MN,  cpg::V_MN);
    ng("RB",  cpg::rb_soma_mods(),  cpg::rb_ais_mods(),  cpg::rb_axon_mods(),  cpg::Y_RB,  cpg::R_RB,  cpg::V_RB);
    CHECK(true);
}

TEST_CASE("glial threshold: dIN + N glia", "[ng-threshold]") {
    for (int n : {2, 3, 4, 6, 8, 12}) {
        char tag[16]; snprintf(tag, sizeof(tag), "dIN+%d", n);
        ng(tag, cpg::dIN_soma_mods(), cpg::dIN_ais_mods(), cpg::dIN_axon_mods(), cpg::Y_DIN, cpg::R_DIN, cpg::V_DIN, n);
    }
    CHECK(true);
}
