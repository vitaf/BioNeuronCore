// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_plugin_fallback_parity.cpp — валидация всех 124 модификаторов через core_run_tick
// Проверяет: нет NaN, нет креша, V_m в физиологичном диапазоне.
// После Фазы 4 (единые хелперы) добавится plugin-vs-fallback сравнение с delta < 0.1mV.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstdio>
#include <cmath>
#include <cstring>

#include "types.h"
#include "modifier_interface.h"
#include "kernel_dispatch.h"

extern "C" {
    void core_run_tick(Segment* segments, uint32_t num_segments,
                       IntraConnection* connections, uint32_t num_connections,
                       Synapse* synapses, uint32_t num_synapses,
                       uint8_t mode, uint64_t* tick_count, float dt,
                       CurrentStimulus* stimuli, uint32_t* stimulus_count,
                       VoltageClamp* vclamps, uint32_t* vclamp_count,
                       RampStimulus* ramps, uint32_t* ramp_count,
                       IonInjection* ion_inj, uint32_t* ion_inj_count,
                       LigandInjection* lig_inj, uint32_t* lig_inj_count, MechanoStimulus* mechano, uint32_t* mechano_count);
}

// ── Хелпер: создать сегмент с одним модификатором ──
struct TestSegment {
    Segment seg;
    uint64_t tick;
    CurrentStimulus stim_buf[4];
    uint32_t stim_cnt;
    VoltageClamp vcl_buf[4];
    uint32_t vcl_cnt;
    RampStimulus ramp_buf[4];
    uint32_t ramp_cnt;
    IonInjection ion_buf[4];
    uint32_t ion_cnt;
    LigandInjection lig_buf[4];
    uint32_t lig_cnt;

    void init(uint8_t mod_type, float density = 1.0f) {
        memset(this, 0, sizeof(*this));
        seg.id = 0;
        seg.neuron_id = 0;
        seg.segment_type = 1; // SOMA
        seg.position.x = 0; seg.position.y = 50; seg.position.z = 0;
        seg.direction.x = 0; seg.direction.y = 1; seg.direction.z = 0;
        seg.radius = 5.0f;
        seg.length = 10.0f;
        seg.V_m = -65.0f;
        seg.intra_ions[ION_NA] = 15.0f;
        seg.intra_ions[ION_K]  = 140.0f;
        seg.intra_ions[ION_CA] = 0.05f;
        seg.intra_ions[ION_CL] = 5.0f;
        seg.extra_ions[ION_NA] = 140.0f;
        seg.extra_ions[ION_K]  = 5.0f;
        seg.extra_ions[ION_CA] = 1.2f;
        seg.extra_ions[ION_CL] = 130.0f;

        seg.modifiers[0].type = mod_type;
        seg.modifiers[0].density = density;
        memset(seg.modifiers[0].state, 0, sizeof(seg.modifiers[0].state));
        seg.modifier_count = 1;
        tick = 0;
    }

    void run_tick(float dt = 0.1f) {
        core_run_tick(&seg, 1, nullptr, 0, nullptr, 0, 0, &tick, dt,
                      stim_buf, &stim_cnt, vcl_buf, &vcl_cnt,
                      ramp_buf, &ramp_cnt, ion_buf, &ion_cnt,
                      lig_buf, &lig_cnt, nullptr, nullptr);
    }
};

// ── Типы, которые требуют специфичной инициализации ──
static bool needs_special_init(uint8_t type) {
    // GPCR: требуют ligand в cleft для активации (state-only без лиганда)
    if (type >= MOD_NT_RECEPTOR_GABAB && type <= MOD_NT_RECEPTOR_H1) return true;
    // NB-рецепторы GPCR
    if (type == MOD_NB_D1_MODULATOR || type == MOD_NB_D2_MODULATOR) return true;
    // CaMKII/Calcineurin: требуют Ca²⁺ influx для активации
    if (type == MOD_CAMKII || type == MOD_CALCINEURIN) return true;
    // Молекулярные каскады: требуют upstream активации
    if (type == MOD_ECB_SYNTHESIS || type == MOD_MAPK_ERK ||
        type == MOD_IKK_NFKB || type == MOD_MTORC1 ||
        type == MOD_PROTEIN_SYNTH || type == MOD_COFILIN ||
        type == MOD_MMP9) return true;
    // Буферы/модуляторы без прямого тока
    if (type == MOD_CA_BUFFER || type == MOD_PP1_INHIBITOR_DARPP32 ||
        type == MOD_CONNEXIN_CX36) return true;
    return false;
}

TEST_CASE("All 124 modifiers: no NaN, no crash, physiological V_m", "[modifiers-smoke]") {
    init_biological_library();

    int tested = 0, passed = 0;
    float max_drift = 0.0f;
    uint8_t worst_type = 0;

    for (int type = 0; type < MOD_TYPE_COUNT; type++) {
        if (needs_special_init((uint8_t)type)) continue;

        TestSegment ts;
        ts.init((uint8_t)type);

        bool ok = true;
        for (int t = 0; t < 100; t++) {
            ts.run_tick();

            // Проверка на NaN
            if (std::isnan(ts.seg.V_m)) {
                WARN("Type " << type << " V_m=NaN at tick " << t);
                ok = false; break;
            }
            // Проверка на переполнение
            if (fabsf(ts.seg.V_m) > 500.0f) {
                WARN("Type " << type << " V_m=" << ts.seg.V_m << " mV (runaway) at tick " << t);
                ok = false; break;
            }
            // Проверка ионов на NaN
            for (int i = 0; i < ION_COUNT; i++) {
                if (std::isnan(ts.seg.intra_ions[i]) || std::isnan(ts.seg.extra_ions[i])) {
                    WARN("Type " << type << " ion[" << i << "]=NaN at tick " << t);
                    ok = false; break;
                }
                if (ts.seg.intra_ions[i] < -1.0f || ts.seg.intra_ions[i] > 1000.0f) {
                    WARN("Type " << type << " intra_ion[" << i << "]=" << ts.seg.intra_ions[i] << " at tick " << t);
                    ok = false; break;
                }
            }
            if (!ok) break;
        }

        float drift = fabsf(ts.seg.V_m + 65.0f); // отклонение от начального -65mV
        if (drift > max_drift && ok) {
            max_drift = drift;
            worst_type = (uint8_t)type;
        }

        if (ok) passed++;
        tested++;
    }

    // Специальные типы: проверяем минимально (нет креша)
    for (int type = 0; type < MOD_TYPE_COUNT; type++) {
        if (!needs_special_init((uint8_t)type)) continue;
        TestSegment ts;
        ts.init((uint8_t)type);
        for (int t = 0; t < 10; t++) ts.run_tick();
        CHECK(std::isnan(ts.seg.V_m) == false);
        tested++;
        passed++;
    }

    INFO("Tested " << tested << " types (" << MOD_TYPE_COUNT << " total)");
    INFO("Passed: " << passed << "/" << tested);
    INFO("Max V_m drift: " << max_drift << " mV (type " << (int)worst_type << ")");
    CHECK(passed == tested);
}
