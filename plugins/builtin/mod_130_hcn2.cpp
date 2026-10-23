// HCN2: средний Ih (τ~100ms). Сердце, мозг.
// Тип: MOD_HCN2 (130).
#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double hcn2_current_impl(Modifier* m, Segment* s, float dt) {
    return hcn_generic_current(m, s, dt, HCN2_VHALF_MV, HCN2_TAU_OPEN_MS, HCN2_GAMMA_PS);
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_HCN2, hcn2_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"HCN2","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":130,"name":"HCN2","category":"PIR","description":"Medium Ih (tau~100ms). Cardiac/brain isoform.","release":false}]})BIOCORE";
}
