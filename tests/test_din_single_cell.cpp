// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_din_single_cell.cpp — изолированный dIN (сома+AIS+аксон) как единое целое.
// Протокол: settle → электрод (CurrentStimulus) → ПД инициируется и распространяется.
// Проверка целостной ионной динамики: ПД через Nav1.2 (AIS/аксон) + Нернст-Планк (#11).

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

// Сома=0, AIS=1, аксон=2 (cpg::SOMA/AIS/AXON).
TEST_CASE("dIN single cell: settle → electrode → AP initiates and propagates", "[din-single-cell]") {
    cpg::CpgNetwork net;
    cpg::add_neuron(net, 0, cpg::Y_DIN, 0, cpg::R_DIN, cpg::V_DIN,
                    cpg::dIN_soma_mods(), cpg::dIN_ais_mods(), cpg::dIN_axon_mods());
    REQUIRE(net.segments.size() == 3);

    char path[256]; snprintf(path, sizeof(path), "/tmp/din_single_cell.bion");
    REQUIRE(cpg::save_bion(net, path) == 0);
    BionData d; memset(&d, 0, sizeof(d));
    REQUIRE(bion_load_host(path, &d) == 0);
    REQUIRE(d.num_segments == 3);

    uint64_t tc = 0;

    // Убрать bath-NMDA (пейсмейкерный вход): тест проверяет ИЗОЛИРОВАННЫЙ dIN
    // (биологичный покой −47, ПД через электрод), а не пейсмейкер (~10 Гц).
    for (uint8_t j = 0; j < d.segments[0].modifier_count; j++) {
        if (d.segments[0].modifiers[j].type == MOD_NT_RECEPTOR_NMDA) {
            for (uint8_t k = j; k + 1 < d.segments[0].modifier_count; k++)
                d.segments[0].modifiers[k] = d.segments[0].modifiers[k + 1];
            d.segments[0].modifier_count--;
            break;
        }
    }

    // 1) Settle: 2000 тиков (200 мс) без стимула — релаксация к покою.
    for (int t = 0; t < 2000; t++) {
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    }
    float rest_soma = d.segments[0].V_m;
    float rest_ais  = d.segments[1].V_m;
    float rest_axon = d.segments[2].V_m;
    printf("[DIN-SINGLE] rest: soma=%.2f AIS=%.2f axon=%.2f mV\n", rest_soma, rest_ais, rest_axon);

    // 2) Электрод: ступень деполяризующего тока в сому (segment 0).
    CurrentStimulus stim = {0, 400.0f, 50.0f, tc};  // 400 пА, 50 мс, старт сейчас (целая клетка)
    uint32_t stim_cnt = 1;
    float peak_soma = -200.0f, peak_ais = -200.0f, peak_axon = -200.0f;
    for (int t = 0; t < 500; t++) {  // 50 мс
        core_run_tick(d.segments, d.num_segments, d.connections, d.num_connections,
                      d.synapses, d.num_synapses, 0, &tc, 0.1f,
                      &stim, &stim_cnt, nullptr, nullptr, nullptr, nullptr,
                      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        if (d.segments[0].V_m > peak_soma) peak_soma = d.segments[0].V_m;
        if (d.segments[1].V_m > peak_ais)  peak_ais  = d.segments[1].V_m;
        if (d.segments[2].V_m > peak_axon) peak_axon = d.segments[2].V_m;
    }
    printf("[DIN-SINGLE] peaks: soma=%.2f AIS=%.2f axon=%.2f mV\n", peak_soma, peak_ais, peak_axon);

    // 3) Проверки: покой стабилен, ПД инициируется (сома/AIS) и доходит до аксона.
    CHECK(rest_soma < -40.0f);      // покой биологичный (не runaway)
    CHECK(peak_soma > -20.0f);      // сома спайкует (выше порога −28)
    CHECK(peak_axon > -20.0f);      // аксон получил ПД (распространение через Nav1.2)

    bion_unload_host(&d); std::remove(path);
}
