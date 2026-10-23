// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// task.cpp — TASK фоновая K⁺-утечка
// Тип: MOD_TASK (151)
// Категория: Leak

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double task_current_impl(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float gamma_ps = 30.0f;  // TASK-1/3 γ ~14–38 пС (среднее)
    float Po = 0.3f;         // фоновый open probability (voltage-independent)
    // pH-ингибиция (протонный сенсор): кислый pH закрывает (IC50 pH 6.7)
    double pH = (m->state[4] > 0.0f) ? m->state[4] : 7.4f;
    float Po_eff = Po / (1.0f + powf(10.0f, 6.7f - pH));

    float K_i = s->intra_ions[ION_K];
    float K_o = s->extra_ions[ION_K];
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * Po_eff;
    float I_K = (float)(I_A * A_TO_PA);

    float vol = segment_volume(s);
    float cpm = charge_per_mole(vol);
    s->intra_ions[ION_K] -= (I_K * dt) / cpm;
    s->extra_ions[ION_K] += (I_K * dt) / (cpm * EXTRA_VOLUME_RATIO);
    plugin_clamp_ions(s->extra_ions, ION_COUNT);

    return -I_K;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_TASK, task_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"TASK background K+ leak","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":151,"name":"TASK background K+ leak","category":"Leak","description":"Background K+ leak channel (TASK-1/3, KCNK3/9). Voltage-independent, Po~0.3. gamma~14-38 pS. GHK flux K+ (z=1).","release":false}]})BIOCORE";
}
