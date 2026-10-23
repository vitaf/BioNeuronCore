// RyR: рианодиновый рецептор. CICR — Ca²⁺-индуцированный Ca²⁺-выброс из ER.
// Тип: MOD_RYR (128). BION v10 — activated.
#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double ryr_current_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — RyR (Keizer & Levine 1996). CICR, динамическая адаптация w.
    double w = m->state[0];   // адаптация (0..1)

    float Ca = s->intra_ions[ION_CA];
    if (Ca < 1e-9f) Ca = 1e-9f;
    float ER = s->ER_Ca2;
    if (ER < 0.0f) ER = 0.0f;

    // Keizer-Levine гейтинг: P_open = w · (1+(c/K_b)³)/(1+(K_a/c)⁴+(c/K_b)³).
    float c_Kb = Ca / RYR_K_B_M_M;
    float c_Kb2 = c_Kb * c_Kb;
    float c_Kb3 = c_Kb2 * c_Kb;
    float Ka_c = RYR_K_A_M_M / Ca;
    float Ka_c4 = Ka_c * Ka_c * Ka_c * Ka_c;

    float w_inf = (1.0f + Ka_c4 + c_Kb3) / (1.0f + 1.0f / RYR_K_C + Ka_c4 + c_Kb2);
    if (w <= 0.0f) w = w_inf;   // инициализация steady-state

    float P_open = w * (1.0f + c_Kb3) / (1.0f + Ka_c4 + c_Kb3);

    // Адаптация w.
    float tau_w = RYR_TAU_W_MS * w_inf;
    if (tau_w < 1.0f) tau_w = 1.0f;
    w += (w_inf - w) * dt / tau_w;
    if (w < 0.0f) w = 0.0f;
    if (w > 1.0f) w = 1.0f;

    // Выброс Ca²⁺ из ЭПР в цитозоль (линейный градиент [Ca]ER − [Ca]).
    float drive = ER - Ca;
    if (drive < 0.0f) drive = 0.0f;
    double dconc = RYR_RELEASE_RATE_S * m->density * P_open * drive * dt * MS_TO_S;

    // Масса сохраняется.
    float er_avail = ER - 1e-9f;
    if (er_avail < 0.0f) er_avail = 0.0f;
    float actual = (dconc < er_avail) ? dconc : er_avail;
    s->ER_Ca2 -= actual;
    s->intra_ions[ION_CA] += actual;
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    if (s->ER_Ca2 < 0.0f) s->ER_Ca2 = 0.0f;

    m->state[0] = w;
    return 0.0f;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_RYR, ryr_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"RyR","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":128,"name":"RyR","category":"Ca2+ homeostasis","description":"Ryanodine receptor: CICR from ER. Hill n=3, bell-shaped Ca2+-dependence.","release":false}]})BIOCORE";
}
