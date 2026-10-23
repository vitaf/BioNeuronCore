// HCN3: промежуточный Ih (τ~50ms).
// Тип: MOD_HCN3 (131).
#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>
static double hcn3_impl(Modifier* m, Segment* s, float dt) {
    return hcn_generic_current(m, s, dt, HCN3_VHALF_MV, HCN3_TAU_OPEN_MS, HCN3_GAMMA_PS);
}
extern "C" int biocore_plugin_init(){register_modifier_current(MOD_HCN3,hcn3_impl);return 1;}
extern "C" const char* biocore_plugin_desc(){return R"BIOCORE({"name":"HCN3","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":131,"name":"HCN3","category":"PIR","description":"Intermediate Ih (tau~50ms).","release":false}]})BIOCORE";}
