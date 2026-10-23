// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// Тесты стимуляции через core_run_tick:
// CurrentStimulus, VoltageClamp, RampStimulus, IonInjection, LigandInjection.
#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"
#include "types.h"
#include <cstring>
#include <cmath>

extern "C" void core_run_tick(Segment* segments, uint32_t num_segments,
                   IntraConnection* connections, uint32_t num_connections,
                   Synapse* synapses, uint32_t num_synapses,
                   uint8_t mode, uint64_t* tick_count, float dt,
                   CurrentStimulus* stimuli, uint32_t* stimulus_count,
                   VoltageClamp* vclamps, uint32_t* vclamp_count,
                   RampStimulus* ramps, uint32_t* ramp_count,
                   IonInjection* ion_inj, uint32_t* ion_inj_count,
                   LigandInjection* lig_inj, uint32_t* lig_inj_count, MechanoStimulus* mechano, uint32_t* mechano_count);

static Segment make_bare_seg() {
    Segment s;
    memset(&s, 0, sizeof(s));
    s.id = 1;
    s.length = 100.0f;
    s.radius = 10.0f;
    s.V_m = -65.0f;
    for (int i = 0; i < ION_COUNT; i++) {
        s.intra_ions[i] = 10.0f; s.extra_ions[i] = 140.0f;
    }
    s.temperature = 37.0f;
    s.modifier_count = 0;
    s.I_buffer = 0.0f;
    return s;
}

// ═══════════════════════════════════════════════════════════════
// CurrentStimulus: проверка базовых свойств
// ═══════════════════════════════════════════════════════════════
TEST_CASE("Stimuli: I_clamp does not crash with NULL stimulus array", "[stimuli]") {
    Segment seg = make_bare_seg();
    uint64_t tick = 0;

    core_run_tick(&seg, 1, nullptr, 0, nullptr, 0, 0, &tick, 0.1f,
                  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

    CHECK(seg.V_m < 0.0f); // всё ещё отрицательный
    CHECK_FALSE(std::isnan(seg.V_m));
}

TEST_CASE("Stimuli: zero-count stimulus buffer is safe", "[stimuli]") {
    Segment seg = make_bare_seg();
    CurrentStimulus stim = {1, 100.0f, 100.0f, 0};
    uint32_t stim_cnt = 0; // count = 0 → не применяется
    uint64_t tick = 0;

    core_run_tick(&seg, 1, nullptr, 0, nullptr, 0, 0, &tick, 0.1f,
                  &stim, &stim_cnt, nullptr, nullptr, nullptr, nullptr,
                  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

    CHECK_FALSE(std::isnan(seg.V_m));
}

TEST_CASE("Stimuli: expired I_clamp removed after duration", "[stimuli]") {
    Segment seg = make_bare_seg();
    CurrentStimulus stim = {1, 100.0f, 0.0f, 0}; // duration=0 → expires immediately
    uint32_t stim_cnt = 1;
    uint64_t tick = 0;

    core_run_tick(&seg, 1, nullptr, 0, nullptr, 0, 0, &tick, 0.1f,
                  &stim, &stim_cnt, nullptr, nullptr, nullptr, nullptr,
                  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

    // duration_ms=0 → start_tick + 0 = 0, tick=0 → 0 >= 0 → expired
    CHECK(stim_cnt == 0);
}

// ═══════════════════════════════════════════════════════════════
// VoltageClamp: фиксирует V_m на заданном значении
// ═══════════════════════════════════════════════════════════════
TEST_CASE("Stimuli: voltage clamp holds V_m", "[stimuli]") {
    Segment seg = make_bare_seg();
    VoltageClamp vc = {1, -70.0f, 0, 100};
    uint32_t vc_cnt = 1;
    uint64_t tick = 0;

    core_run_tick(&seg, 1, nullptr, 0, nullptr, 0, 0, &tick, 0.1f,
                  nullptr, nullptr, &vc, &vc_cnt, nullptr, nullptr,
                  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

    CHECK(seg.V_m == Approx(-70.0f).margin(0.1));
}

TEST_CASE("Stimuli: voltage clamp persists within interval", "[stimuli]") {
    Segment seg = make_bare_seg();
    VoltageClamp vc = {1, -70.0f, 0, 100};
    uint32_t vc_cnt = 1;
    uint64_t tick = 0;

    core_run_tick(&seg, 1, nullptr, 0, nullptr, 0, 0, &tick, 0.1f,
                  nullptr, nullptr, &vc, &vc_cnt, nullptr, nullptr,
                  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

    // end_tick=100, tick=0 → не истёк
    CHECK(vc_cnt == 1);
}

TEST_CASE("Stimuli: voltage clamp expires after end_tick", "[stimuli]") {
    Segment seg = make_bare_seg();
    VoltageClamp vc = {1, -70.0f, 0, 1}; // end_tick=1
    uint32_t vc_cnt = 1;
    uint64_t tick = 0;

    // Тик 0: 0 < 1 → не истёк, clamp активен
    core_run_tick(&seg, 1, nullptr, 0, nullptr, 0, 0, &tick, 0.1f,
                  nullptr, nullptr, &vc, &vc_cnt, nullptr, nullptr,
                  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    CHECK(vc_cnt == 1);
    CHECK(seg.V_m == Approx(-70.0f).margin(0.1));

    // Тик 1: 1 >= 1 → истёк, удаляется ДО шага 1
    core_run_tick(&seg, 1, nullptr, 0, nullptr, 0, 0, &tick, 0.1f,
                  nullptr, nullptr, &vc, &vc_cnt, nullptr, nullptr,
                  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    CHECK(vc_cnt == 0);
}

// ═══════════════════════════════════════════════════════════════
// RampStimulus: линейно меняющийся ток
// ═══════════════════════════════════════════════════════════════
TEST_CASE("Stimuli: ramp stimulus increases current over time", "[stimuli]") {
    Segment seg_early = make_bare_seg();
    Segment seg_late = make_bare_seg();

    RampStimulus ramp = {1, 0.0f, 100.0f, 0, 10};
    uint32_t ramp_cnt = 1;
    uint64_t tick_early = 0;
    uint64_t tick_late = 5;

    // Тик 0: frac ≈ 0/10 = 0 → ток ≈ 0 pA
    core_run_tick(&seg_early, 1, nullptr, 0, nullptr, 0, 0, &tick_early, 0.1f,
                  nullptr, nullptr, nullptr, nullptr, &ramp, &ramp_cnt,
                  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

    // Тик 5: frac ≈ 5/10 = 0.5 → ток ≈ 50 pA
    ramp_cnt = 1;
    core_run_tick(&seg_late, 1, nullptr, 0, nullptr, 0, 0, &tick_late, 0.1f,
                  nullptr, nullptr, nullptr, nullptr, &ramp, &ramp_cnt,
                  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

    // На позднем тике больше тока → сильнее деполяризация
    CHECK(seg_late.V_m > seg_early.V_m);
}

TEST_CASE("Stimuli: ramp expires after end_tick", "[stimuli]") {
    Segment seg = make_bare_seg();
    RampStimulus ramp = {1, 0.0f, 100.0f, 0, 1}; // end_tick=1
    uint32_t ramp_cnt = 1;
    uint64_t tick = 0;

    // Тик 0: 0 < 1 → не истёк
    core_run_tick(&seg, 1, nullptr, 0, nullptr, 0, 0, &tick, 0.1f,
                  nullptr, nullptr, nullptr, nullptr, &ramp, &ramp_cnt,
                  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    CHECK(ramp_cnt == 1);

    // Тик 1: 1 >= 1 → истёк
    core_run_tick(&seg, 1, nullptr, 0, nullptr, 0, 0, &tick, 0.1f,
                  nullptr, nullptr, nullptr, nullptr, &ramp, &ramp_cnt,
                  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    CHECK(ramp_cnt == 0);
}

// ═══════════════════════════════════════════════════════════════
// IonInjection: добавляет ионы в сегмент
// ═══════════════════════════════════════════════════════════════
TEST_CASE("Stimuli: ion injection into intracellular space", "[stimuli]") {
    Segment seg = make_bare_seg();
    IonInjection inj = {1, ION_NA, 5.0f, 0}; // +5 мМ Na внутрь
    uint32_t inj_cnt = 1;
    uint64_t tick = 0;

    float na_before = seg.intra_ions[ION_NA];

    core_run_tick(&seg, 1, nullptr, 0, nullptr, 0, 0, &tick, 0.1f,
                  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                  &inj, &inj_cnt, nullptr, nullptr, nullptr, nullptr);

    CHECK(seg.intra_ions[ION_NA] == Approx(na_before + 5.0f));
}

TEST_CASE("Stimuli: ion injection into extracellular space", "[stimuli]") {
    Segment seg = make_bare_seg();
    IonInjection inj = {1, ION_K, 3.0f, 1}; // +3 мМ K наружу
    uint32_t inj_cnt = 1;
    uint64_t tick = 0;

    float k_before = seg.extra_ions[ION_K];

    core_run_tick(&seg, 1, nullptr, 0, nullptr, 0, 0, &tick, 0.1f,
                  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                  &inj, &inj_cnt, nullptr, nullptr, nullptr, nullptr);

    CHECK(seg.extra_ions[ION_K] == Approx(k_before + 3.0f));
}

TEST_CASE("Stimuli: ion injection cleared after one tick", "[stimuli]") {
    Segment seg = make_bare_seg();
    IonInjection inj = {1, ION_NA, 5.0f, 0};
    uint32_t inj_cnt = 1;
    uint64_t tick = 0;

    core_run_tick(&seg, 1, nullptr, 0, nullptr, 0, 0, &tick, 0.1f,
                  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                  &inj, &inj_cnt, nullptr, nullptr, nullptr, nullptr);

    CHECK(inj_cnt == 0); // одноразовые — очищаются после шага 1
}

// ═══════════════════════════════════════════════════════════════
// LigandInjection: добавляет нейромедиатор
// ═══════════════════════════════════════════════════════════════
TEST_CASE("Stimuli: ligand injection extracellular", "[stimuli]") {
    Segment seg = make_bare_seg();
    LigandInjection linj = {1, NT_DOPAMINE, 0.5f, 0}; // внеклеточно
    uint32_t linj_cnt = 1;
    uint64_t tick = 0;

    float da_before = seg.neurotransmitter[NT_DOPAMINE];

    core_run_tick(&seg, 1, nullptr, 0, nullptr, 0, 0, &tick, 0.1f,
                  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                  nullptr, nullptr, &linj, &linj_cnt, nullptr, nullptr);

    CHECK(seg.neurotransmitter[NT_DOPAMINE] == Approx(da_before + 0.5f));
}

TEST_CASE("Stimuli: ligand injection cleared after one tick", "[stimuli]") {
    Segment seg = make_bare_seg();
    LigandInjection linj = {1, NT_SEROTONIN, 1.0f, 0};
    uint32_t linj_cnt = 1;
    uint64_t tick = 0;

    core_run_tick(&seg, 1, nullptr, 0, nullptr, 0, 0, &tick, 0.1f,
                  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                  nullptr, nullptr, &linj, &linj_cnt, nullptr, nullptr);

    CHECK(linj_cnt == 0);
}

// ═══════════════════════════════════════════════════════════════
// Комбинированный тест: все типы стимулов одновременно
// ═══════════════════════════════════════════════════════════════
TEST_CASE("Stimuli: multiple stimulus types in same tick", "[stimuli]") {
    Segment seg = make_bare_seg();

    CurrentStimulus stim = {1, 500.0f, 100.0f, 0};
    uint32_t stim_cnt = 1;
    IonInjection inj = {1, ION_CA, 1.0f, 0};
    uint32_t inj_cnt = 1;
    LigandInjection linj = {1, NT_GLUTAMATE, 0.2f, 0};
    uint32_t linj_cnt = 1;
    uint64_t tick = 0;

    float v_before = seg.V_m;
    float ca_before = seg.intra_ions[ION_CA];
    float glu_before = seg.neurotransmitter[NT_GLUTAMATE];

    core_run_tick(&seg, 1, nullptr, 0, nullptr, 0, 0, &tick, 0.1f,
                  &stim, &stim_cnt, nullptr, nullptr, nullptr, nullptr,
                  &inj, &inj_cnt, &linj, &linj_cnt, nullptr, nullptr);

    CHECK(seg.V_m > v_before);
    CHECK(seg.intra_ions[ION_CA] == Approx(ca_before + 1.0f));
    CHECK(seg.neurotransmitter[NT_GLUTAMATE] == Approx(glu_before + 0.2f));
    CHECK(inj_cnt == 0);
    CHECK(linj_cnt == 0);
}

// ═══════════════════════════════════════════════════════════════
// LigandInjection intracellular: пишет в intra_nt[]
// ═══════════════════════════════════════════════════════════════
TEST_CASE("Stimuli: ligand injection intracellular", "[stimuli]") {
    Segment seg = make_bare_seg();
    LigandInjection linj = {1, NT_DOPAMINE, 0.5f, 1}; // intracellular=true
    uint32_t linj_cnt = 1;
    uint64_t tick = 0;

    float da_before = seg.intra_nt[NT_DOPAMINE];

    core_run_tick(&seg, 1, nullptr, 0, nullptr, 0, 0, &tick, 0.1f,
                  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                  nullptr, nullptr, &linj, &linj_cnt, nullptr, nullptr);

    CHECK(seg.intra_nt[NT_DOPAMINE] == Approx(da_before + 0.5f));
    CHECK(seg.neurotransmitter[NT_DOPAMINE] == Approx(0.0f));
    CHECK(linj_cnt == 0);
}
