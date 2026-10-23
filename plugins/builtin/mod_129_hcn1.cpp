// HCN1: быстрый Ih (τ~25ms). Изоформа головастиковых dIN.
// Тип: MOD_HCN1 (129). Био: Li,Roberts,Soffe 2010; Zhang&Sillar 2012.
#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double hcn1_current_impl(Modifier* m, Segment* s, float dt) {
    return hcn_generic_current(m, s, dt, HCN1_VHALF_MV, HCN1_TAU_OPEN_MS, HCN1_GAMMA_PS);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_HCN1, hcn1_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"HCN1 (fast Ih)","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":129,"name":"HCN1","category":"PIR","description":"Fast Ih (tau~25ms). Tadpole dIN isoform.","release":false}]})BIOCORE";
}
