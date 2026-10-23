// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_cpu_k2p_direct.cpp — ПРЯМОЙ вызов core_run_tick для K2P-сегмента
#include <cstdio>
#include <cmath>
#include <cstring>

#include "types.h"
#include "modifier_interface.h"
#include "kernel_dispatch.h"
#include "plugin_manager.h"

// Из simulation_host.cpp
extern "C" void core_run_tick(Segment* segments, uint32_t num_segments,
    IntraConnection* connections, uint32_t num_connections,
    Synapse* synapses, uint32_t num_synapses,
    uint8_t mode, uint64_t* tick, float dt,
    void* stim, void* stim_cnt, void* vcl, void* vcl_cnt,
    void* rmp, void* rmp_cnt, void* ion, void* ion_cnt,
    void* lig, void* lig_cnt, MechanoStimulus* mechano, uint32_t* mechano_count);

int main() {
    // Init
    for (int i = 0; i < MOD_TYPE_COUNT; i++) {
        biological_library[i] = nullptr;
        arbitrary_library[i] = nullptr;
        modifier_current_table[i] = nullptr;
    }
    init_biological_library();
    plugin_load_builtin(".");

    // Manual segment with ONLY K2P
    Segment seg;
    memset(&seg, 0, sizeof(seg));
    seg.id = 0;
    seg.neuron_id = 0;
    seg.segment_type = 1; // SOMA
    seg.radius = 5.0f;
    seg.length = 10.0f;
    seg.V_m = -65.0f;
    seg.intra_ions[0] = 15.0f;   // Na
    seg.intra_ions[1] = 140.0f;   // K
    seg.intra_ions[2] = 0.05f; // Ca
    seg.intra_ions[3] = 5.0f;   // Cl
    seg.extra_ions[0] = 140.0f;
    seg.extra_ions[1] = 5.0f;
    seg.extra_ions[2] = 1.2f;
    seg.extra_ions[3] = 130.0f;
    seg.ATP_level = 0.1f;

    // Только K2P
    seg.modifiers[0].type = 107; // K2P
    seg.modifiers[0].density = 0.03f;
    seg.modifier_count = 1;

    float C_m = 4.0f * PI * seg.radius * seg.radius * 0.01f; // биофизическая
    printf("C_m=%.4f pF  area=%.1f um^2  radius=%.1f length=%.1f\n",
           C_m, 4.0f*PI*seg.radius*seg.radius, seg.radius, seg.length);
    printf("Initial: V_m=%.4f K_i=%.6f K_o=%.6f\n",
           seg.V_m, seg.intra_ions[1], seg.extra_ions[1]);

    // Буферы стимулов (пустые)
    struct { uint32_t seg_id; float amp_pA, dur_ms; uint64_t start; } stim_buf[1] = {};
    uint32_t stim_cnt = 0;
    struct { uint32_t seg_id; float target_mV; uint64_t start, end; } vcl_buf[1] = {};
    uint32_t vcl_cnt = 0;
    struct { uint32_t seg_id; float start_pA, end_pA; uint64_t s_tick, e_tick; } rmp_buf[1] = {};
    uint32_t rmp_cnt = 0;
    struct { uint32_t seg_id; uint8_t ii, xc; float amt; } ion_buf[1] = {};
    uint32_t ion_cnt = 0;
    struct { uint32_t seg_id; uint8_t ni, lc; float amt; } lig_buf[1] = {};
    uint32_t lig_cnt = 0;

    uint64_t tick = 0;

    float prev_Vm = -65.0f;
    for (int t = 0; t < 10; t++) {
        // ДО тика: вызываем K2P напрямую для сравнения
        seg.modifiers[0].state[7] = 4.0f * PI * seg.radius * seg.radius;
        float direct_I = k2p_leak_current(&seg.modifiers[0], &seg, 0.1f);
        float direct_dV = direct_I / C_m * 0.1f;

        core_run_tick(&seg, 1, nullptr, 0, nullptr, 0, 0,
                      &tick, 0.1f,
                      stim_buf, &stim_cnt, vcl_buf, &vcl_cnt,
                      rmp_buf, &rmp_cnt, ion_buf, &ion_cnt,
                      lig_buf, &lig_cnt, nullptr, nullptr);

        float actual_dV = seg.V_m - prev_Vm;
        float actual_I = actual_dV * C_m / 0.1f;
        prev_Vm = seg.V_m;

        if (t < 3)
            printf("Tick %d: direct_I=%.6f direct_dV=%.6f  actual_dV=%.6f actual_I=%.6f ratio=%.1f\n",
                   t+1, direct_I, direct_dV, actual_dV, actual_I,
                   (direct_I != 0 ? actual_I/direct_I : 0));
    }

    float dV = seg.V_m - (-65.0f);
    printf("After 10 ticks: V_m=%.6f dV=%.6f (per tick: %.6f)\n",
           seg.V_m, dV, dV/10.0f);

    return 0;
}
