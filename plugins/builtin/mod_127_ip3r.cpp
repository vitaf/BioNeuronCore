// IP3R: IP3-управляемый Ca²⁺-канал ЭПР. Высвобождает Ca²⁺ из ER в цитозоль.
// Тип: MOD_IP3R (127). BION v10 — activated.
#include "plugin_helpers.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include <cmath>

static double ip3r_current_impl(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — IP3R (Li & Rinzel 1994). Динамическая инактивация h, честный каскад Gq→PLC→IP3.
    double IP3 = m->state[0];   // IP3 (локальный второй мессенджер, мМ)
    double h = m->state[1];   // инактивация (0..1)

    // Gq→PLC→IP3: глутамат → mGluR → PLC → IP3 (честный каскад, не суррогат).
    float glu = s->neurotransmitter[NT_GLUTAMATE];
    if (glu < 0.0f) glu = 0.0f;
    float mGluR = glu / (glu + IP3R_EC50_MGLUR_MM);
    IP3 += (IP3R_K_IP3_PROD_MS * mGluR - IP3 / IP3R_TAU_IP3_MS) * dt;
    if (IP3 < 0.0f) IP3 = 0.0f;

    float Ca = s->intra_ions[ION_CA];
    if (Ca < 1e-9f) Ca = 1e-9f;
    float ER = s->ER_Ca2;
    if (ER < 0.0f) ER = 0.0f;

    // Li-Rinzel гейтинг: P_open = m∞³ · n∞³ · h³.
    float m_inf = IP3 / (IP3 + IP3R_K_IP3_M_M);
    float n_inf = Ca / (Ca + IP3R_K_ACT_M_M);
    float h_inf = IP3R_K_INH_M_M / (IP3R_K_INH_M_M + Ca);
    if (h <= 0.0f) h = h_inf;   // инициализация steady-state
    h += (h_inf - h) * dt / IP3R_TAU_H_MS;
    if (h < 0.0f) h = 0.0f;
    if (h > 1.0f) h = 1.0f;

    float m3 = m_inf * m_inf * m_inf;
    float n3 = n_inf * n_inf * n_inf;
    float h3 = h * h * h;
    float P_open = m3 * n3 * h3;

    // Выброс Ca²⁺ из ЭПР в цитозоль (линейный градиент [Ca]ER − [Ca]).
    float drive = ER - Ca;
    if (drive < 0.0f) drive = 0.0f;
    double dconc = IP3R_RELEASE_RATE_S * m->density * P_open * drive * dt * MS_TO_S;

    // Масса сохраняется: не выпускаем больше, чем есть в ЭПР.
    float er_avail = ER - 1e-9f;
    if (er_avail < 0.0f) er_avail = 0.0f;
    float actual = (dconc < er_avail) ? dconc : er_avail;
    s->ER_Ca2 -= actual;
    s->intra_ions[ION_CA] += actual;
    plugin_clamp_ions(s->intra_ions, ION_COUNT);
    if (s->ER_Ca2 < 0.0f) s->ER_Ca2 = 0.0f;

    m->state[0] = IP3;
    m->state[1] = h;
    return 0.0f;
}

extern "C" int biocore_plugin_init() {
    register_modifier_current(MOD_IP3R, ip3r_current_impl);
    return 1;
}

extern "C" const char* biocore_plugin_desc() {
    return R"BIOCORE({"name":"IP3R","version":"1.0.0","author":"BioNeuronCore","modifiers":[{"id":127,"name":"IP3R","category":"Ca2+ homeostasis","description":"IP3 receptor: ER Ca2+ release channel. Bell-shaped Ca2+-dependence.","release":false}]})BIOCORE";
}
