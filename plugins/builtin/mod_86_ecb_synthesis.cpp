// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// ecb_synthesis.cpp — эндоканнабиноидный синтез (2-AG)
// Тип: MOD_ECB_SYNTHESIS (86)
// Категория: STDP Cascade

#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double ecb_synthesis_current_impl(Modifier* m, Segment* s, float dt) {
    float Ca = s->intra_ions[ION_CA] * 1000.0f; // uM
    double synth_rate = (m->state[1] > 0.0f) ? m->state[1] : 0.01f;
    double degrad_rate = (m->state[2] > 0.0f) ? m->state[2] : 0.005f;
    float dag = segment_dag(s);  // DAG из MOD_PLC_IP3_DAG (PLCβ→DAG→DGLα)
    dt *= m->density;  // enzyme_amount

    // Ca²⁺- и DAG-зависимый синтез 2-AG (Hill n=2, Kd~0.5uM)
    float Ca_n = Ca * Ca;
    float synth = synth_rate * Ca_n / (Ca_n + 0.25f + 1e-9f) * (1.0f + dag);

    m->state[0] += (synth - m->state[0] * degrad_rate) * dt;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 10.0f) m->state[0] = 10.0f;

    // Ток не генерирует — eCB записывается в cleft через simulation_host.cpp step 3
    return 0.0f;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_ECB_SYNTHESIS, ecb_synthesis_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"eCB Synthesis (2-AG)","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":86,"name":"eCB Synthesis (2-AG)","category":"STDP Cascade","description":"Endocannabinoid synthesis (2-AG). Ca2+-dependent (Hill n=2) via PLCbeta/DAGLalpha. 2-AG released retrogradely into synaptic cleft. Degradation by MAGL.","release":false}]})BIOCORE";
}
