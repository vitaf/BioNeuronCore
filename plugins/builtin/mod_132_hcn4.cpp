// HCN4: медленный Ih (τ~250ms). Пейсмейкерные клетки сердца, таламус.
// Тип: MOD_HCN4 (132). Сильно cAMP-модулируемый.
#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>
static double hcn4_impl(Modifier* m, Segment* s, float dt) {
    return hcn_generic_current(m, s, dt, HCN4_VHALF_MV, HCN4_TAU_OPEN_MS, HCN4_GAMMA_PS);
}
extern "C" int biocore_plugin_init(){register_modifier_current(MOD_HCN4,hcn4_impl);return 1;}
extern "C" const char* biocore_plugin_desc(){return R"BIOCORE({"name":"HCN4","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":132,"name":"HCN4","category":"PIR","description":"Slow Ih (tau~250ms). Cardiac pacemaker/thalmic isoform.","release":false}]})BIOCORE";}
