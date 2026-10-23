// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#include "types.h"
#include "modifier_interface.h"
#include "kernel_dispatch.h"
#include "run_tick_common.h"
#include "modifier_helpers.h"
#include <cmath>

// ==================== EXTERN: функции из hh_biological.cpp ====================
extern double ca_current(Modifier* m, Segment* s, float dt);
extern double cl_current(Modifier* m, Segment* s, float dt);
extern double metabotropic_current(Modifier* m, Segment* s, float dt);
extern double myelin_modulator(Modifier* m, Segment* s, float dt);
extern double synaptic_release_current(Modifier* m, Segment* s, float dt);
extern double physical_sensor_current(Modifier* m, Segment* s, float dt);
extern double nak_pump_current(Modifier* m, Segment* s, float dt);
extern double nak_pump_a3_current(Modifier* m, Segment* s, float dt);
extern double hunger_sensor_current(Modifier* m, Segment* s, float dt);
extern double light_sensor_current(Modifier* m, Segment* s, float dt);
extern double pir_ttype_current(Modifier* m, Segment* s, float dt);
extern double pir_hcn_current(Modifier* m, Segment* s, float dt);  // [DEPRECATED] бывш. PIR_HCN, оставлен для обратной совместимости

// ==================== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ====================
// segment_area, seg_volume, effective_area, charge_per_mole: единый источник в modifier_helpers.h
// backward-compat: seg_volume → segment_volume
#define seg_volume(s) segment_volume(s)

// ghk_monovalent/ghk_divalent/ghk_anion: единая реализация в run_tick_common.h

// effective_area_mod — plugin-специфичный хелпер (без сегмента)
static inline BIOCORE_HOST_DEVICE float effective_area_mod(Modifier* m) {
    if (m->state[7] > 0.0f) return m->state[7];
    return 1.0f;
}


// ==================== ИОНОТРОПНЫЕ РЕЦЕПТОРЫ (9) ====================

BIOCORE_DEVICE_CALLABLE double ampa_current(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — AMPA (GluA2-содержащий, Traynelis 2010): P_Na:P_K ≈ 1:1, E_rev ≈ 0 мВ, P_Ca=0.
    return ionotropic_ghk_current(m, s, dt, AMPA_GAMMA_PS, AMPA_EC50_REL, AMPA_TAU_RISE_MS, AMPA_TAU_DECAY_MS,
                                  NT_GLUTAMATE, AMPA_P_NA_RATIO, AMPA_P_K_RATIO, AMPA_P_CA_RATIO, false, AMPA_TAU_DESENS_MS);
}

BIOCORE_DEVICE_CALLABLE double nmda_current(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — NMDA GluN1/GluN2A (взрослый, Mayer & Westbrook 1987): P_Ca/P_Na ≈ 10.6, P_K/P_Na ≈ 1. Mg²⁺-блок (Woodhull) + CDI (Ca²⁺/CaM).
    double I = ionotropic_ghk_current(m, s, dt, NMDA_GAMMA_PS, NMDA_EC50_REL, NMDA_TAU_RISE_MS, NMDA_TAU_DECAY_MS,
                                     NT_GLUTAMATE, NMDA_P_NA_RATIO, NMDA_P_K_RATIO, NMDA_P_CA_RATIO, false, NMDA_TAU_DESENS_MS);
    // Mg²⁺ блок: 1/(1 + ([Mg]/K_0)·exp(−δ·z·F·V/RT)), [Mg]=1 мМ.
    float mg_block = 1.0f / (1.0f + (1.0f / NMDA_MG_K0_MM) * exp(-NMDA_MG_COEF_PER_MV * s->V_m));
    return I * mg_block * nmda_cdi_coagonist(m, s, dt);
}

BIOCORE_DEVICE_CALLABLE double nmda2b_current(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — NMDA GluN1/GluN2B (эмбриональный, Xenopus dIN): медленная деактивация
    // τ_decay 150 мс → длинные NMDA-EPSP (Roberts & Tunstall 1990). Тот же GHK Na/K/Ca + Mg²⁺-блок + CDI.
    double I = ionotropic_ghk_current(m, s, dt, NMDA_GAMMA_PS, NMDA_EC50_REL, NMDA2B_TAU_RISE_MS, NMDA2B_TAU_DECAY_MS,
                                     NT_GLUTAMATE, NMDA_P_NA_RATIO, NMDA_P_K_RATIO, NMDA_P_CA_RATIO, false, NMDA_TAU_DESENS_MS);
    float mg_block = 1.0f / (1.0f + (1.0f / NMDA_MG_K0_MM) * exp(-NMDA_MG_COEF_PER_MV * s->V_m));
    return I * mg_block * nmda_cdi_coagonist(m, s, dt);
}

BIOCORE_DEVICE_CALLABLE double kainate_current(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — Kainate (GluK2(R), edited): P_Na:P_K ≈ 1:1, P_Ca=0.
    return ionotropic_ghk_current(m, s, dt, KAINATE_GAMMA_PS, KAINATE_EC50_REL, KAINATE_TAU_RISE_MS, KAINATE_TAU_DECAY_MS,
                                  NT_GLUTAMATE, KAINATE_P_NA_RATIO, KAINATE_P_K_RATIO, KAINATE_P_CA_RATIO, false, KAINATE_TAU_DESENS_MS);
}

BIOCORE_DEVICE_CALLABLE double gabaa_current(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — GABAA: Cl⁻-анионный канал (тормозной, E_rev=E_Cl).
    return ionotropic_ghk_current(m, s, dt, GABAA_GAMMA_PS, GABAA_EC50_REL, GABAA_TAU_RISE_MS, GABAA_TAU_DECAY_MS,
                                  NT_GABA, 0.0f, 0.0f, 0.0f, true, GABAA_TAU_DESENS_MS);
}

BIOCORE_DEVICE_CALLABLE double glycine_current(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — Glycine: Cl⁻-анионный канал (очень быстрый, тормозной).
    return ionotropic_ghk_current(m, s, dt, GLYCINE_GAMMA_PS, GLYCINE_EC50_REL, GLYCINE_TAU_RISE_MS, GLYCINE_TAU_DECAY_MS,
                                  NT_GLYCINE, 0.0f, 0.0f, 0.0f, true, GLYCINE_TAU_DESENS_MS);
}

BIOCORE_DEVICE_CALLABLE double nicotinic_current(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — Nicotinic α4β2 (высокоаффинный, низкий Ca²⁺).
    return ionotropic_ghk_current(m, s, dt, NICOTINIC_A4B2_GAMMA_PS, NICOTINIC_A4B2_EC50_REL, NICOTINIC_A4B2_TAU_RISE_MS, NICOTINIC_A4B2_TAU_DECAY_MS,
                                  NT_ACETYLCHOLINE, NICOTINIC_A4B2_P_NA_RATIO, NICOTINIC_A4B2_P_K_RATIO, NICOTINIC_A4B2_P_CA_RATIO, false, NICOTINIC_A4B2_TAU_DESENS_MS);
}

BIOCORE_DEVICE_CALLABLE double nicotinic_alpha7_current(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — Nicotinic α7 (быстрый, высокий Ca²⁺, P_Ca/P_Na≈6).
    return ionotropic_ghk_current(m, s, dt, NICOTINIC_A7_GAMMA_PS, NICOTINIC_A7_EC50_REL, NICOTINIC_A7_TAU_RISE_MS, NICOTINIC_A7_TAU_DECAY_MS,
                                  NT_ACETYLCHOLINE, NICOTINIC_A7_P_NA_RATIO, NICOTINIC_A7_P_K_RATIO, NICOTINIC_A7_P_CA_RATIO, false, NICOTINIC_A7_TAU_DESENS_MS);
}

BIOCORE_DEVICE_CALLABLE double ht3_current(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — 5HT3: неселективный катионный (P_K≈P_Na), медленная десенситизация.
    return ionotropic_ghk_current(m, s, dt, HT3_GAMMA_PS, HT3_EC50_REL, HT3_TAU_RISE_MS, HT3_TAU_DECAY_MS,
                                  NT_SEROTONIN, HT3_P_NA_RATIO, HT3_P_K_RATIO, HT3_P_CA_RATIO, false, HT3_TAU_DESENS_MS);
}

BIOCORE_DEVICE_CALLABLE double p2x_current(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — P2X3: ATP-зависимый катионный (P_K≈P_Na, P_Ca/P_Na≈2.5), быстрая десенситизация.
    return ionotropic_ghk_current(m, s, dt, P2X_GAMMA_PS, P2X_EC50_REL, P2X_TAU_RISE_MS, P2X_TAU_DECAY_MS,
                                  NT_ATP, P2X_P_NA_RATIO, P2X_P_K_RATIO, P2X_P_CA_RATIO, false, P2X_TAU_DESENS_MS);
}

BIOCORE_DEVICE_CALLABLE double p2x2_current(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — P2X2: ATP-зависимый катионный, медленная десенситизация (~5с).
    return ionotropic_ghk_current(m, s, dt, P2X_GAMMA_PS, P2X2_EC50_REL, P2X2_TAU_RISE_MS, P2X2_TAU_DECAY_MS,
                                  NT_ATP, P2X2_P_NA_RATIO, P2X2_P_K_RATIO, P2X2_P_CA_RATIO, false, P2X2_TAU_DESENS_MS);
}

BIOCORE_DEVICE_CALLABLE double trpv1_current(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — TRPV1 (Caterina 1997): температурный (>43°C) + капсаициновый сенсор, P_Ca/P_Na≈6.
    float area = effective_area(m, s);
    float V = s->V_m;
    double vol = segment_volume(s);
    double cpm = charge_per_mole(vol);

    // Температурная активация (Boltzmann вокруг 43°C).
    float f_T = 1.0f / (1.0f + exp((TRPV1_T_THRESHOLD_C - s->temperature) / TRPV1_T_SLOPE_C));

    // Капсаиновая активация.
    float cap = s->neurotransmitter[NT_CAPSAICIN];
    if (cap < 0.0f) cap = 0.0f;
    float f_cap = cap / (cap + TRPV1_EC50_CAP_REL);

    // Медленная активация r + десенситизация d (двухстадийная).
    double r = m->state[0];
    float p_inf = f_T + f_cap - f_T * f_cap; // ИЛИ: тепло или капсаицин
    float r_inf = p_inf;
    r += (r_inf - r) * dt / TRPV1_TAU_ACT_MS;
    if (r < 0.0f) r = 0.0f; if (r > 1.0f) r = 1.0f;
    m->state[0] = r;

    double d = m->state[1];
    float d_inf = p_inf;
    d += (d_inf - d) * dt / TRPV1_TAU_DESENS_MS;
    if (d < 0.0f) d = 0.0f; if (d > 1.0f) d = 1.0f;
    m->state[1] = d;

    float open_prob = r * (1.0f - d);

    // GHK-ток (Na/K/Ca, наружу-положительный) — одномолекулярная P_single.
    double P_Na, P_K, P_Ca;
    ghk_partial_P(TRPV1_GAMMA_PS, TRPV1_P_NA_RATIO, TRPV1_P_K_RATIO, TRPV1_P_CA_RATIO, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double I_Na = P_Na * ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]) * N * open_prob * A_TO_PA;
    float I_K  = (float)(P_K  * ghk_monovalent(V, s->intra_ions[ION_K],  s->extra_ions[ION_K])  * N * open_prob * A_TO_PA);
    double I_Ca = P_Ca * ghk_divalent(V,  s->intra_ions[ION_CA], s->extra_ions[ION_CA]) * N * open_prob * A_TO_PA;

    s->intra_ions[ION_NA] -= (I_Na * dt) / cpm;
    s->intra_ions[ION_K]  -= (I_K  * dt) / cpm;
    s->intra_ions[ION_CA] -= (I_Ca * dt) / (2.0f * cpm);
    clamp_ions(s->intra_ions, ION_COUNT);

    return -(I_Na + I_K + I_Ca);  // внутрь-положительный (деполяризующий)
}

// ==================== МЕТАБОТРОПНЫЕ Gi/o (24 рецептора) ====================

BIOCORE_DEVICE_CALLABLE double gabab_current(Modifier* m, Segment* s, float dt)   { return gi_o_ghk_effect(m, s, dt, NT_GABA); }
BIOCORE_DEVICE_CALLABLE double ht1a_current(Modifier* m, Segment* s, float dt)    { return gi_o_ghk_effect(m, s, dt, NT_SEROTONIN); }
BIOCORE_DEVICE_CALLABLE double ht1b_current(Modifier* m, Segment* s, float dt)    { return gi_o_ghk_effect(m, s, dt, NT_SEROTONIN); }
BIOCORE_DEVICE_CALLABLE double ht1d_current(Modifier* m, Segment* s, float dt)    { return gi_o_ghk_effect(m, s, dt, NT_SEROTONIN); }
BIOCORE_DEVICE_CALLABLE double ht1f_current(Modifier* m, Segment* s, float dt)    { return gi_o_ghk_effect(m, s, dt, NT_SEROTONIN); }
BIOCORE_DEVICE_CALLABLE double d2_current(Modifier* m, Segment* s, float dt)      { return gi_o_ghk_effect(m, s, dt, NT_DOPAMINE); }
BIOCORE_DEVICE_CALLABLE double d3_current(Modifier* m, Segment* s, float dt)      { return gi_o_ghk_effect(m, s, dt, NT_DOPAMINE); }
BIOCORE_DEVICE_CALLABLE double d4_current(Modifier* m, Segment* s, float dt)      { return gi_o_ghk_effect(m, s, dt, NT_DOPAMINE); }
BIOCORE_DEVICE_CALLABLE double m2_current(Modifier* m, Segment* s, float dt)      { return gi_o_ghk_effect(m, s, dt, NT_ACETYLCHOLINE); }
BIOCORE_DEVICE_CALLABLE double m4_current(Modifier* m, Segment* s, float dt)      { return gi_o_ghk_effect(m, s, dt, NT_ACETYLCHOLINE); }
BIOCORE_DEVICE_CALLABLE double a1_current(Modifier* m, Segment* s, float dt)      { return gi_o_ghk_effect(m, s, dt, NT_ADENOSINE); }
BIOCORE_DEVICE_CALLABLE double a3_current(Modifier* m, Segment* s, float dt)      { return gi_o_ghk_effect(m, s, dt, NT_ADENOSINE); }
BIOCORE_DEVICE_CALLABLE double mglur2_current(Modifier* m, Segment* s, float dt)  { return gi_o_ghk_effect(m, s, dt, NT_GLUTAMATE); }
BIOCORE_DEVICE_CALLABLE double mglur3_current(Modifier* m, Segment* s, float dt)  { return gi_o_ghk_effect(m, s, dt, NT_GLUTAMATE); }
BIOCORE_DEVICE_CALLABLE double mglur4_current(Modifier* m, Segment* s, float dt)  { return gi_o_ghk_effect(m, s, dt, NT_GLUTAMATE); }
BIOCORE_DEVICE_CALLABLE double mglur6_current(Modifier* m, Segment* s, float dt)  { return gi_o_ghk_effect(m, s, dt, NT_GLUTAMATE); }
BIOCORE_DEVICE_CALLABLE double mglur7_current(Modifier* m, Segment* s, float dt)  { return gi_o_ghk_effect(m, s, dt, NT_GLUTAMATE); }
BIOCORE_DEVICE_CALLABLE double mglur8_current(Modifier* m, Segment* s, float dt)  { return gi_o_ghk_effect(m, s, dt, NT_GLUTAMATE); }
BIOCORE_DEVICE_CALLABLE double alpha2_current(Modifier* m, Segment* s, float dt)  { return gi_o_ghk_effect(m, s, dt, NT_NORADRENALINE); }
BIOCORE_DEVICE_CALLABLE double cb1_current(Modifier* m, Segment* s, float dt)     { return gi_o_ghk_effect(m, s, dt, NT_ENDOCANNABINOID); }
BIOCORE_DEVICE_CALLABLE double cb2_current(Modifier* m, Segment* s, float dt)     { return gi_o_ghk_effect(m, s, dt, NT_ENDOCANNABINOID); }
BIOCORE_DEVICE_CALLABLE double h3_current(Modifier* m, Segment* s, float dt)      { return gi_o_ghk_effect(m, s, dt, NT_HISTAMINE); }
BIOCORE_DEVICE_CALLABLE double mt1_current(Modifier* m, Segment* s, float dt)     { return gi_o_ghk_effect(m, s, dt, NT_MELATONIN); }
BIOCORE_DEVICE_CALLABLE double mt2_current(Modifier* m, Segment* s, float dt)     { return gi_o_ghk_effect(m, s, dt, NT_MELATONIN); }

// ==================== МЕТАБОТРОПНЫЕ Gs (11 рецепторов) ====================

BIOCORE_DEVICE_CALLABLE double ht4_current(Modifier* m, Segment* s, float dt)   { return gs_camp_signal(m, s, dt, NT_SEROTONIN); }
BIOCORE_DEVICE_CALLABLE double ht6_current(Modifier* m, Segment* s, float dt)   { return gs_camp_signal(m, s, dt, NT_SEROTONIN); }
BIOCORE_DEVICE_CALLABLE double ht7_current(Modifier* m, Segment* s, float dt)   { return gs_camp_signal(m, s, dt, NT_SEROTONIN); }
BIOCORE_DEVICE_CALLABLE double d1_current(Modifier* m, Segment* s, float dt)    { return gs_camp_signal(m, s, dt, NT_DOPAMINE); }
BIOCORE_DEVICE_CALLABLE double d5_current(Modifier* m, Segment* s, float dt)    { return gs_camp_signal(m, s, dt, NT_DOPAMINE); }
BIOCORE_DEVICE_CALLABLE double a2a_current(Modifier* m, Segment* s, float dt)   { return gs_camp_signal(m, s, dt, NT_ADENOSINE); }
BIOCORE_DEVICE_CALLABLE double a2b_current(Modifier* m, Segment* s, float dt)   { return gs_camp_signal(m, s, dt, NT_ADENOSINE); }
BIOCORE_DEVICE_CALLABLE double beta1_current(Modifier* m, Segment* s, float dt) { return gs_camp_signal(m, s, dt, NT_NORADRENALINE); }
BIOCORE_DEVICE_CALLABLE double beta2_current(Modifier* m, Segment* s, float dt) { return gs_camp_signal(m, s, dt, NT_NORADRENALINE); }
BIOCORE_DEVICE_CALLABLE double h2_current(Modifier* m, Segment* s, float dt)    { return gs_camp_signal(m, s, dt, NT_HISTAMINE); }
BIOCORE_DEVICE_CALLABLE double taar1_current(Modifier* m, Segment* s, float dt) { return gs_camp_signal(m, s, dt, NT_DOPAMINE); }

// ==================== МЕТАБОТРОПНЫЕ Gq/11 (11 рецепторов) ====================

BIOCORE_DEVICE_CALLABLE double ht2a_current(Modifier* m, Segment* s, float dt)     { return gq_signal(m, s, dt, NT_SEROTONIN); }
BIOCORE_DEVICE_CALLABLE double ht2b_current(Modifier* m, Segment* s, float dt)     { return gq_signal(m, s, dt, NT_SEROTONIN); }
BIOCORE_DEVICE_CALLABLE double ht2c_current(Modifier* m, Segment* s, float dt)     { return gq_signal(m, s, dt, NT_SEROTONIN); }
BIOCORE_DEVICE_CALLABLE double alpha1a_current(Modifier* m, Segment* s, float dt)  { return gq_signal(m, s, dt, NT_NORADRENALINE); }
BIOCORE_DEVICE_CALLABLE double alpha1b_current(Modifier* m, Segment* s, float dt)  { return gq_signal(m, s, dt, NT_NORADRENALINE); }
BIOCORE_DEVICE_CALLABLE double m1_current(Modifier* m, Segment* s, float dt)       { return gq_signal(m, s, dt, NT_ACETYLCHOLINE); }
BIOCORE_DEVICE_CALLABLE double m3_current(Modifier* m, Segment* s, float dt)       { return gq_signal(m, s, dt, NT_ACETYLCHOLINE); }
BIOCORE_DEVICE_CALLABLE double m5_current(Modifier* m, Segment* s, float dt)       { return gq_signal(m, s, dt, NT_ACETYLCHOLINE); }
BIOCORE_DEVICE_CALLABLE double mglur1_current(Modifier* m, Segment* s, float dt)   { return gq_signal(m, s, dt, NT_GLUTAMATE); }
BIOCORE_DEVICE_CALLABLE double mglur5_current(Modifier* m, Segment* s, float dt)   { return gq_signal(m, s, dt, NT_GLUTAMATE); }
BIOCORE_DEVICE_CALLABLE double h1_current(Modifier* m, Segment* s, float dt)       { return gq_signal(m, s, dt, NT_HISTAMINE); }
BIOCORE_DEVICE_CALLABLE double mrgpr_current(Modifier* m, Segment* s, float dt)     { return mrgpr_signal(m, s, dt); }  // лиганд state[4], не NT

// ==================== NB-РЕЦЕПТОРЫ (Neuroblox-совместимые) ====================

// NB_MoradiFullNMDAR (63): NMDA (Mayer & Westbrook 1987) + Mg²⁺-блок (Woodhull).
// ✅ ДОСТОВЕРНО — Moradi A/B/C/g/z + Q10 + spk_proxy заменены на честный ionotropic + Mg-блок
// (single-source с типом 9). Читает neurotransmitter[NT_GLUTAMATE], не V_m-суррогат.
BIOCORE_DEVICE_CALLABLE double nb_nmda_moradi_full_current(Modifier* m, Segment* s, float dt) {
    double I = ionotropic_ghk_current(m, s, dt, NMDA_GAMMA_PS, NMDA_EC50_REL, NMDA_TAU_RISE_MS, NMDA_TAU_DECAY_MS,
                                     NT_GLUTAMATE, NMDA_P_NA_RATIO, NMDA_P_K_RATIO, NMDA_P_CA_RATIO, false, NMDA_TAU_DESENS_MS);
    float mg = s->extra_ions[ION_MG];
    if (mg < 0.01f) mg = 1.0f;
    float mg_block = 1.0f / (1.0f + (mg / NMDA_MG_K0_MM) * exp(-NMDA_MG_COEF_PER_MV * s->V_m));
    return I * mg_block;
}

// NB_AMPA_STA (64): AMPA (GluA2, Traynelis 2010). STA-oscillator/spk-суррогат убраны.
// ✅ ДОСТОВЕРНО — честный ionotropic, читает neurotransmitter[NT_GLUTAMATE].
// Short-term plasticity (STA) — отдельная будущая задача, не здесь.
BIOCORE_DEVICE_CALLABLE double nb_ampa_sta_current(Modifier* m, Segment* s, float dt) {
    return ionotropic_ghk_current(m, s, dt, AMPA_GAMMA_PS, AMPA_EC50_REL, AMPA_TAU_RISE_MS, AMPA_TAU_DECAY_MS,
                                  NT_GLUTAMATE, AMPA_P_NA_RATIO, AMPA_P_K_RATIO, AMPA_P_CA_RATIO, false, AMPA_TAU_DESENS_MS);
}

// NB_D1_Modulator (65): Hill-occupancy DA→D1, M_NMDA1
BIOCORE_DEVICE_CALLABLE double nb_d1_modulator_current(Modifier* m, Segment* s, float dt) {
    float DA = s->neurotransmitter[NT_DOPAMINE];
    double phi1 = m->state[0];
    double p_k = (m->state[2] > 0.0f) ? m->state[2] : 0.3f;
    float DA_n = pow(DA, 1.0f);
    float K_n = pow(p_k, 1.0f);
    float phi1_inf = DA_n / (DA_n + K_n);
    phi1 += (phi1_inf - phi1) * dt / 100.0f;
    if (phi1 < 0.0f) phi1 = 0.0f;
    if (phi1 > 1.0f) phi1 = 1.0f;
    double p_ampl = (m->state[3] > 0.0f) ? m->state[3] : 0.5f;
    m->state[0] = phi1;
    m->state[1] = 1.0f + p_ampl * phi1;
    return 0.0f;
}

// NB_D2_Modulator (66): Hill-occupancy DA→D2, M_AMPA2
BIOCORE_DEVICE_CALLABLE double nb_d2_modulator_current(Modifier* m, Segment* s, float dt) {
    float DA = s->neurotransmitter[NT_DOPAMINE];
    double phi2 = m->state[0];
    double p_k = (m->state[2] > 0.0f) ? m->state[2] : 0.3f;
    float DA_n = pow(DA, 1.0f);
    float K_n = pow(p_k, 1.0f);
    float phi2_inf = DA_n / (DA_n + K_n);
    phi2 += (phi2_inf - phi2) * dt / 100.0f;
    if (phi2 < 0.0f) phi2 = 0.0f;
    if (phi2 > 1.0f) phi2 = 1.0f;
    double p_suppr = (m->state[3] > 0.0f) ? m->state[3] : 0.3f;
    m->state[0] = phi2;
    m->state[1] = 1.0f - p_suppr * phi2;
    return 0.0f;
}

// NB_Alpha7ER_nAChR (67): α7 nAChR (Dani & Bertrand 2007) — быстрый, высокий Ca²⁺.
// ✅ ДОСТОВЕРНО — Марков-гейтинг/ER Ca-store/хардкод E_Ca заменены на честный ionotropic
// (single-source с типом α7=97). Читает neurotransmitter[NT_ACETYLCHOLINE].
BIOCORE_DEVICE_CALLABLE double nb_alpha7_er_nachr_current(Modifier* m, Segment* s, float dt) {
    return ionotropic_ghk_current(m, s, dt, NICOTINIC_A7_GAMMA_PS, NICOTINIC_A7_EC50_REL, NICOTINIC_A7_TAU_RISE_MS, NICOTINIC_A7_TAU_DECAY_MS,
                                  NT_ACETYLCHOLINE, NICOTINIC_A7_P_NA_RATIO, NICOTINIC_A7_P_K_RATIO, NICOTINIC_A7_P_CA_RATIO, false, NICOTINIC_A7_TAU_DESENS_MS);
}

// NB_Beta2_nAChR (68): α4β2 nAChR (Dani & Bertrand 2007) — высокоаффинный, медленная десенс.
// ✅ ДОСТОВЕРНО — Марков/Ник-десенс заменены на честный ionotropic (single-source с типом 13).
// Читает neurotransmitter[NT_ACETYLCHOLINE].
BIOCORE_DEVICE_CALLABLE double nb_beta2_nachr_current(Modifier* m, Segment* s, float dt) {
    return ionotropic_ghk_current(m, s, dt, NICOTINIC_A4B2_GAMMA_PS, NICOTINIC_A4B2_EC50_REL, NICOTINIC_A4B2_TAU_RISE_MS, NICOTINIC_A4B2_TAU_DECAY_MS,
                                  NT_ACETYLCHOLINE, NICOTINIC_A4B2_P_NA_RATIO, NICOTINIC_A4B2_P_K_RATIO, NICOTINIC_A4B2_P_CA_RATIO, false, NICOTINIC_A4B2_TAU_DESENS_MS);
}

// NB_CaTRPM4R (69): TRPM4 — Ca²⁺-активируемый НЕселективный катионный канал (Na⁺/K⁺).
// ✅ ДОСТОВЕРНО — CCh-суррогат + фиктивный I_Ca заменены на честный Ca²⁺-гейт Hill n=2 + GHK Na/K
// (single-source хелпер trpm4_cation_current). Агонист — внутриклеточный Ca²⁺, не нейромедиатор.
BIOCORE_DEVICE_CALLABLE double nb_ca_trpm4r_current(Modifier* m, Segment* s, float dt) {
    return trpm4_cation_current(m, s, dt, TRPM4_EC50_CA_UM, TRPM4_HILL, TRPM4_TAU_MS);
}

// NB_MuscarinicR (70): M1-подобный метаботропный ACh-рецептор (Caulfield & Birdsall 1998).
// ✅ ДОСТОВЕРНО — I_NCM (ACh→Ca→g суррогат) заменён на честный Gq-каскад (single-source с m1/m3/m5).
// M1/M3/M5 → Gq/11 → PLC → IP3/DAG. Читает neurotransmitter[NT_ACETYLCHOLINE].
BIOCORE_DEVICE_CALLABLE double nb_muscarinic_r_current(Modifier* m, Segment* s, float dt) {
    return gq_signal(m, s, dt, NT_ACETYLCHOLINE);
}

// NB_AMPA_Destexhe (71): AMPA (GluA2, Traynelis 2010). Damped oscillator + spk-суррогат убраны.
// ✅ ДОСТОВЕРНО — честный ionotropic (single-source с типом 8), читает neurotransmitter[NT_GLUTAMATE].
BIOCORE_DEVICE_CALLABLE double nb_ampa_destexhe_current(Modifier* m, Segment* s, float dt) {
    return ionotropic_ghk_current(m, s, dt, AMPA_GAMMA_PS, AMPA_EC50_REL, AMPA_TAU_RISE_MS, AMPA_TAU_DECAY_MS,
                                  NT_GLUTAMATE, AMPA_P_NA_RATIO, AMPA_P_K_RATIO, AMPA_P_CA_RATIO, false, AMPA_TAU_DESENS_MS);
}

// NB_NMDA_Destexhe (72): NMDA (Mayer & Westbrook 1987) + Mg²⁺-блок. spk-суррогат убран.
// ✅ ДОСТОВЕРНО — честный ionotropic + Mg-блок (single-source с типом 9), читает neurotransmitter[NT_GLUTAMATE].
BIOCORE_DEVICE_CALLABLE double nb_nmda_destexhe_current(Modifier* m, Segment* s, float dt) {
    double I = ionotropic_ghk_current(m, s, dt, NMDA_GAMMA_PS, NMDA_EC50_REL, NMDA_TAU_RISE_MS, NMDA_TAU_DECAY_MS,
                                     NT_GLUTAMATE, NMDA_P_NA_RATIO, NMDA_P_K_RATIO, NMDA_P_CA_RATIO, false, NMDA_TAU_DESENS_MS);
    float mg = s->extra_ions[ION_MG];
    if (mg < 0.01f) mg = 1.0f;
    float mg_block = 1.0f / (1.0f + (mg / NMDA_MG_K0_MM) * exp(-NMDA_MG_COEF_PER_MV * s->V_m));
    return I * mg_block;
}

// NB_GABAA_Destexhe (73): GABAA — Cl⁻-анионный канал (тормозной, E_rev=E_Cl). spk-суррогат убран.
// ✅ ДОСТОВЕРНО — честный ionotropic anion (single-source с типом 11), читает neurotransmitter[NT_GABA].
BIOCORE_DEVICE_CALLABLE double nb_gabaa_destexhe_current(Modifier* m, Segment* s, float dt) {
    return ionotropic_ghk_current(m, s, dt, GABAA_GAMMA_PS, GABAA_EC50_REL, GABAA_TAU_RISE_MS, GABAA_TAU_DECAY_MS,
                                  NT_GABA, 0.0f, 0.0f, 0.0f, true, GABAA_TAU_DESENS_MS);
}

// NB_GABAB_Destexhe (74): GABAB — метаботропный Gi/o → Gβγ → GIRK (Kir3.x). spk-суррогат убран.
// ✅ ДОСТОВЕРНО — честный gi_o_ghk_effect (single-source с типом 17), читает neurotransmitter[NT_GABA].
BIOCORE_DEVICE_CALLABLE double nb_gabab_destexhe_current(Modifier* m, Segment* s, float dt) {
    return gi_o_ghk_effect(m, s, dt, NT_GABA);
}

// NB_MsnNMDAR (75): NMDA (Mayer & Westbrook 1987) + Mg²⁺-блок + D1-модуляция (стриатум MSN).
// ✅ ДОСТОВЕРНО — spk-суррогат/Moradi-кинетика заменены на честный ionotropic + Mg-блок + D1-усиление
// (mod_scale = M_NMDA1 из D1-модулятора на сегменте). Читает neurotransmitter[NT_GLUTAMATE].
BIOCORE_DEVICE_CALLABLE double nb_msn_nmdar_current(Modifier* m, Segment* s, float dt) {
    float M_NMDA1 = 1.0f;
    for (uint8_t j = 0; j < s->modifier_count; j++) {
        if (s->modifiers[j].type == MOD_NB_D1_MODULATOR) {
            M_NMDA1 = s->modifiers[j].state[1];
            if (M_NMDA1 < 0.0f) M_NMDA1 = 0.0f;
            break;
        }
    }
    double I = ionotropic_ghk_current(m, s, dt, NMDA_GAMMA_PS, NMDA_EC50_REL, NMDA_TAU_RISE_MS, NMDA_TAU_DECAY_MS,
                                     NT_GLUTAMATE, NMDA_P_NA_RATIO, NMDA_P_K_RATIO, NMDA_P_CA_RATIO, false, NMDA_TAU_DESENS_MS, 0.0f, M_NMDA1);
    float mg = s->extra_ions[ION_MG];
    if (mg < 0.01f) mg = 1.0f;
    float mg_block = 1.0f / (1.0f + (mg / NMDA_MG_K0_MM) * exp(-NMDA_MG_COEF_PER_MV * s->V_m));
    return I * mg_block;
}

// NB_MsnAMPAR (76): AMPA (GluA2, Traynelis 2010) + D2-модуляция (стриатум MSN).
// ✅ ДОСТОВЕРНО — damped oscillator/spk-суррогат заменены на честный ionotropic + D2-подавление
// (mod_scale = M_AMPA2 из D2-модулятора на сегменте). Читает neurotransmitter[NT_GLUTAMATE].
BIOCORE_DEVICE_CALLABLE double nb_msn_ampar_current(Modifier* m, Segment* s, float dt) {
    float M_AMPA2 = 1.0f;
    for (uint8_t j = 0; j < s->modifier_count; j++) {
        if (s->modifiers[j].type == MOD_NB_D2_MODULATOR) {
            M_AMPA2 = s->modifiers[j].state[1];
            if (M_AMPA2 < 0.0f) M_AMPA2 = 0.0f;
            break;
        }
    }
    return ionotropic_ghk_current(m, s, dt, AMPA_GAMMA_PS, AMPA_EC50_REL, AMPA_TAU_RISE_MS, AMPA_TAU_DECAY_MS,
                                  NT_GLUTAMATE, AMPA_P_NA_RATIO, AMPA_P_K_RATIO, AMPA_P_CA_RATIO, false, AMPA_TAU_DESENS_MS, 0.0f, M_AMPA2);
}

// NB_SomaATP (77): митохондриальный синтез ATP
BIOCORE_DEVICE_CALLABLE double nb_soma_atp_current(Modifier* m, Segment* s, float dt) {
    double nadh = m->state[0];
    float atp = s->ATP_level;
    float inhibition = 1.0f - atp / 5.0f;
    if (inhibition < 0.0f) inhibition = 0.0f;
    nadh += (0.001f - nadh * 0.01f) * dt;
    if (nadh < 0.0f) nadh = 0.0f;
    if (nadh > 1.0f) nadh = 1.0f;
    double p_synth_rate = (m->state[1] > 0.0f) ? m->state[1] : 0.01f;
    s->ATP_level += p_synth_rate * nadh * inhibition * m->density * dt;
    if (s->ATP_level > 5.0f) s->ATP_level = 5.0f;
    s->temperature += 0.01f * nadh * inhibition * 0.5f * dt * 0.001f;
    if (s->temperature > 42.0f) s->temperature = 42.0f;
    m->state[0] = nadh;
    return 0.0f;
}

// NB_SomaThermal (78): термальная регуляция клетки
BIOCORE_DEVICE_CALLABLE double nb_soma_thermal_current(Modifier* m, Segment* s, float dt) {
    double heat_stress = m->state[0];
    float T = s->temperature;
    double p_tau_thermal = (m->state[1] > 0.0f) ? m->state[1] : 5000.0f;
    T += (37.0f - T) * dt / p_tau_thermal;
    if (T < 30.0f) T = 30.0f;
    if (T > 42.0f) T = 42.0f;
    if (T > 39.0f) heat_stress += (T - 39.0f) * dt * 0.0001f;
    else heat_stress -= heat_stress * dt * 0.001f;
    if (heat_stress < 0.0f) heat_stress = 0.0f;
    if (heat_stress > 1.0f) heat_stress = 1.0f;
    s->temperature = T;
    m->state[0] = heat_stress;
    return 0.0f;
}

// NB_CREB_Transcription (79): ядерный CREB
BIOCORE_DEVICE_CALLABLE double nb_creb_transcription_current(Modifier* m, Segment* s, float dt) {
    float Ca_i = s->intra_ions[ION_CA];
    double pCREB = m->state[0], late = m->state[1], struct_pl = m->state[2];
    double p_Ca_Kd = (m->state[3] > 0.0f) ? m->state[3] : 0.5f;
    float Ca_n = Ca_i * Ca_i, K_n = p_Ca_Kd * p_Ca_Kd;
    float CaMK = Ca_n / (Ca_n + K_n);
    pCREB     += (CaMK * (1.0f - pCREB) / 5000.0f - 0.0001f * pCREB) * dt;
    late      += (pCREB * (1.0f - late) / 30000.0f - late * 0.00001f) * dt;
    struct_pl += (late * (1.0f - struct_pl) / 300000.0f - struct_pl * 0.000001f) * dt;
    if (pCREB < 0.0f) pCREB = 0.0f;
    if (pCREB > 1.0f) pCREB = 1.0f;
    if (late < 0.0f) late = 0.0f;
    if (late > 1.0f) late = 1.0f;
    if (struct_pl < 0.0f) struct_pl = 0.0f;
    if (struct_pl > 1.0f) struct_pl = 1.0f;
    m->state[0] = pCREB; m->state[1] = late; m->state[2] = struct_pl;
    s->transcription_signal = 0.5f * pCREB + 0.3f * late + 0.2f * struct_pl;
    return 0.0f;
}

// ==================== ПРЕСИНАПТИЧЕСКИЕ МОДУЛЯТОРЫ ====================

// Gi/o модулятор: снижает выброс
static BIOCORE_HOST_DEVICE float modulate_release_gi(Modifier* m, float* cleft_nt, float dt, int nt_index) {
    float nt = cleft_nt[nt_index];
    float activation = nt / (nt + 0.5f);
    m->state[0] += (activation - m->state[0]) * dt / 50.0f;
    double scale = m->density * effective_area_mod(m);
    return 1.0f - 0.7f * m->state[0] * scale;
}

// Gs модулятор: повышает выброс
static BIOCORE_HOST_DEVICE float modulate_release_gs(Modifier* m, float* cleft_nt, float dt, int nt_index) {
    float nt = cleft_nt[nt_index];
    float activation = nt / (nt + 0.5f);
    m->state[0] += (activation - m->state[0]) * dt / 30.0f;
    double scale = m->density * effective_area_mod(m);
    return 1.0f + 0.5f * m->state[0] * scale;
}

// Ионотропный модулятор: повышает выброс
static BIOCORE_HOST_DEVICE float modulate_release_ionotropic(Modifier* m, float* cleft_nt, float dt, int nt_index) {
    float nt = cleft_nt[nt_index];
    float activation = nt / (nt + 0.5f);
    (void)dt;
    double scale = m->density * effective_area_mod(m);
    return 1.0f + 0.3f * activation * scale;
}

BIOCORE_DEVICE_CALLABLE float ht1a_modulate_release(Modifier* m, float* cleft_nt, float dt) {
    return modulate_release_gi(m, cleft_nt, dt, NT_SEROTONIN);
}
BIOCORE_DEVICE_CALLABLE float ht1b_modulate_release(Modifier* m, float* cleft_nt, float dt) {
    return modulate_release_gi(m, cleft_nt, dt, NT_SEROTONIN);
}
BIOCORE_DEVICE_CALLABLE float d2_modulate_release(Modifier* m, float* cleft_nt, float dt) {
    return modulate_release_gi(m, cleft_nt, dt, NT_DOPAMINE);
}
BIOCORE_DEVICE_CALLABLE float a2a_modulate_release(Modifier* m, float* cleft_nt, float dt) {
    return modulate_release_gs(m, cleft_nt, dt, NT_ADENOSINE);
}
BIOCORE_DEVICE_CALLABLE float cb1_modulate_release(Modifier* m, float* cleft_nt, float dt) {
    return modulate_release_gi(m, cleft_nt, dt, NT_ENDOCANNABINOID);
}
BIOCORE_DEVICE_CALLABLE float h3_modulate_release(Modifier* m, float* cleft_nt, float dt) {
    return modulate_release_gi(m, cleft_nt, dt, NT_HISTAMINE);
}
BIOCORE_DEVICE_CALLABLE float nicotinic_modulate_release(Modifier* m, float* cleft_nt, float dt) {
    return modulate_release_ionotropic(m, cleft_nt, dt, NT_ACETYLCHOLINE);
}
BIOCORE_DEVICE_CALLABLE float a1_modulate_release(Modifier* m, float* cleft_nt, float dt) {
    return modulate_release_gi(m, cleft_nt, dt, NT_ADENOSINE);
}
BIOCORE_DEVICE_CALLABLE float d1_modulate_release(Modifier* m, float* cleft_nt, float dt) {
    return modulate_release_gs(m, cleft_nt, dt, NT_DOPAMINE);
}
BIOCORE_DEVICE_CALLABLE float mglur2_modulate_release(Modifier* m, float* cleft_nt, float dt) {
    return modulate_release_gi(m, cleft_nt, dt, NT_GLUTAMATE);
}
BIOCORE_DEVICE_CALLABLE float mglur3_modulate_release(Modifier* m, float* cleft_nt, float dt) {
    return modulate_release_gi(m, cleft_nt, dt, NT_GLUTAMATE);
}
BIOCORE_DEVICE_CALLABLE float alpha2_modulate_release(Modifier* m, float* cleft_nt, float dt) {
    return modulate_release_gi(m, cleft_nt, dt, NT_NORADRENALINE);
}

// ==================== ИНИЦИАЛИЗАЦИЯ БИБЛИОТЕК ====================

void init_biological_library() {
    // Обнуление всех слотов
    for (int i = 0; i < MOD_TYPE_COUNT; i++) {
        biological_library[i] = nullptr;
        biological_modulators[i] = nullptr;
    }

    // Базовые модификаторы (из Итерации 1)
    biological_library[MOD_CAV13]            = ca_current;
    biological_library[MOD_IONOTROPIC_CL]    = cl_current;
    biological_library[MOD_METABOTROPIC]     = metabotropic_current;
    biological_library[MOD_MYELIN]           = myelin_modulator;
    biological_library[MOD_SYNAPTIC_RELEASE] = synaptic_release_current;
    biological_library[MOD_PHYSICAL_SENSOR]  = physical_sensor_current;
    biological_library[MOD_PUMP_NAK]         = nak_pump_current;
    biological_library[MOD_PUMP_NAK_A3]      = nak_pump_a3_current;

    // Ионотропные рецепторы (9)
    biological_library[MOD_NT_RECEPTOR_AMPA]      = ampa_current;
    biological_library[MOD_NT_RECEPTOR_NMDA]      = nmda_current;
    biological_library[MOD_NT_RECEPTOR_KAINATE]   = kainate_current;
    biological_library[MOD_NT_RECEPTOR_GABAA]     = gabaa_current;
    biological_library[MOD_NT_RECEPTOR_GLYCINE]   = glycine_current;
    biological_library[MOD_NT_RECEPTOR_NICOTINIC] = nicotinic_current;
    biological_library[MOD_NT_RECEPTOR_5HT3]      = ht3_current;
    biological_library[MOD_NT_RECEPTOR_P2X]       = p2x_current;
    biological_library[MOD_NT_RECEPTOR_TRPV1]     = trpv1_current;

    // Gi/o (24)
    biological_library[MOD_NT_RECEPTOR_GABAB]   = gabab_current;
    biological_library[MOD_NT_RECEPTOR_5HT1A]   = ht1a_current;
    biological_library[MOD_NT_RECEPTOR_5HT1B]   = ht1b_current;
    biological_library[MOD_NT_RECEPTOR_5HT1D]   = ht1d_current;
    biological_library[MOD_NT_RECEPTOR_5HT1F]   = ht1f_current;
    biological_library[MOD_NT_RECEPTOR_D2]      = d2_current;
    biological_library[MOD_NT_RECEPTOR_D3]      = d3_current;
    biological_library[MOD_NT_RECEPTOR_D4]      = d4_current;
    biological_library[MOD_NT_RECEPTOR_M2]      = m2_current;
    biological_library[MOD_NT_RECEPTOR_M4]      = m4_current;
    biological_library[MOD_NT_RECEPTOR_A1]      = a1_current;
    biological_library[MOD_NT_RECEPTOR_A3]      = a3_current;
    biological_library[MOD_NT_RECEPTOR_MGLUR2]  = mglur2_current;
    biological_library[MOD_NT_RECEPTOR_MGLUR3]  = mglur3_current;
    biological_library[MOD_NT_RECEPTOR_MGLUR4]  = mglur4_current;
    biological_library[MOD_NT_RECEPTOR_MGLUR6]  = mglur6_current;
    biological_library[MOD_NT_RECEPTOR_MGLUR7]  = mglur7_current;
    biological_library[MOD_NT_RECEPTOR_MGLUR8]  = mglur8_current;
    biological_library[MOD_NT_RECEPTOR_ALPHA2]  = alpha2_current;
    biological_library[MOD_NT_RECEPTOR_CB1]     = cb1_current;
    biological_library[MOD_NT_RECEPTOR_CB2]     = cb2_current;
    biological_library[MOD_NT_RECEPTOR_H3]      = h3_current;
    biological_library[MOD_NT_RECEPTOR_MT1]     = mt1_current;
    biological_library[MOD_NT_RECEPTOR_MT2]     = mt2_current;

    // Gs (11)
    biological_library[MOD_NT_RECEPTOR_5HT4]  = ht4_current;
    biological_library[MOD_NT_RECEPTOR_5HT6]  = ht6_current;
    biological_library[MOD_NT_RECEPTOR_5HT7]  = ht7_current;
    biological_library[MOD_NT_RECEPTOR_D1]    = d1_current;
    biological_library[MOD_NT_RECEPTOR_D5]    = d5_current;
    biological_library[MOD_NT_RECEPTOR_A2A]   = a2a_current;
    biological_library[MOD_NT_RECEPTOR_A2B]   = a2b_current;
    biological_library[MOD_NT_RECEPTOR_BETA1] = beta1_current;
    biological_library[MOD_NT_RECEPTOR_BETA2] = beta2_current;
    biological_library[MOD_NT_RECEPTOR_H2]    = h2_current;
    biological_library[MOD_NT_RECEPTOR_TAAR1] = taar1_current;

    // Gq/11 (11)
    biological_library[MOD_NT_RECEPTOR_5HT2A]   = ht2a_current;
    biological_library[MOD_NT_RECEPTOR_5HT2B]   = ht2b_current;
    biological_library[MOD_NT_RECEPTOR_5HT2C]   = ht2c_current;
    biological_library[MOD_NT_RECEPTOR_ALPHA1A] = alpha1a_current;
    biological_library[MOD_NT_RECEPTOR_ALPHA1B] = alpha1b_current;
    biological_library[MOD_NT_RECEPTOR_M1]      = m1_current;
    biological_library[MOD_NT_RECEPTOR_M3]      = m3_current;
    biological_library[MOD_NT_RECEPTOR_M5]      = m5_current;
    biological_library[MOD_NT_RECEPTOR_MGLUR1]  = mglur1_current;
    biological_library[MOD_NT_RECEPTOR_MGLUR5]  = mglur5_current;
    biological_library[MOD_NT_RECEPTOR_H1]      = h1_current;

    // NB-рецепторы (14)
    biological_library[MOD_NB_NMDA_MORADI_FULL] = nb_nmda_moradi_full_current;
    biological_library[MOD_NB_AMPA_STA]         = nb_ampa_sta_current;
    biological_library[MOD_NB_D1_MODULATOR]     = nb_d1_modulator_current;
    biological_library[MOD_NB_D2_MODULATOR]     = nb_d2_modulator_current;
    biological_library[MOD_NB_ALPHA7_ER_NACHR]  = nb_alpha7_er_nachr_current;
    biological_library[MOD_NB_BETA2_NACHR]      = nb_beta2_nachr_current;
    biological_library[MOD_NB_CA_TRPM4R]        = nb_ca_trpm4r_current;
    biological_library[MOD_NB_MUSCARINIC_R]     = nb_muscarinic_r_current;
    biological_library[MOD_NB_AMPA_DESTEXHE]    = nb_ampa_destexhe_current;
    biological_library[MOD_NB_NMDA_DESTEXHE]    = nb_nmda_destexhe_current;
    biological_library[MOD_NB_GABAA_DESTEXHE]   = nb_gabaa_destexhe_current;
    biological_library[MOD_NB_GABAB_DESTEXHE]   = nb_gabab_destexhe_current;
    biological_library[MOD_NB_MSN_NMDAR]        = nb_msn_nmdar_current;
    biological_library[MOD_NB_MSN_AMPAR]        = nb_msn_ampar_current;
    biological_library[MOD_NB_SOMA_ATP]          = nb_soma_atp_current;
    biological_library[MOD_NB_SOMA_THERMAL]      = nb_soma_thermal_current;
    biological_library[MOD_NB_CREB_TRANSCRIPTION] = nb_creb_transcription_current;
    biological_library[MOD_CAMKII]                 = camkii_current;
    biological_library[MOD_CALCINEURIN]            = calcineurin_current;
    biological_library[MOD_CA_L_TYPE]              = ca_l_type_current;
    biological_library[MOD_KV42]                   = kv42_current;
    biological_library[MOD_KV43]                   = kv43_current;
    biological_library[MOD_KV32]                   = kv32_current;
    biological_library[MOD_KV33]                   = kv33_current;
    biological_library[MOD_KV22]                   = kv22_current;
    biological_library[MOD_KV14]                   = kv14_current;
    biological_library[MOD_KV16]                   = kv16_current;
    biological_library[MOD_KIR22]                  = kir22_current;
    biological_library[MOD_KCA31]                  = kca31_current;
    biological_library[MOD_TRPC3]                  = trpc3_current;
    biological_library[MOD_TRPM7]                  = trpm7_current;
    biological_library[MOD_ECB_SYNTHESIS]          = ecb_synthesis_current;
    biological_library[MOD_MAPK_ERK]               = mapk_erk_current;
    biological_library[MOD_IKK_NFKB]               = ikk_nfkb_current;
    biological_library[MOD_MTORC1]                 = mtorc1_current;
    biological_library[MOD_PROTEIN_SYNTH]          = protein_synth_current;
    biological_library[MOD_COFILIN]                = cofilin_current;
    biological_library[MOD_MMP9]                   = mmp9_current;
    biological_library[MOD_HUNGER_SENSOR]             = hunger_sensor_current;
    biological_library[MOD_LIGHT_SENSOR]              = light_sensor_current;
    biological_library[MOD_PIR_TTYPE_CALCIUM]         = pir_ttype_current;
    biological_library[MOD_CAV33]                     = cav33_current;
    biological_library[MOD_CAV31]                     = cav31_current;
    biological_library[MOD_CAV32]                     = cav32_current;
    biological_library[MOD_NICOTINIC_ALPHA7]          = nicotinic_alpha7_current;

    // Ca²⁺/K⁺/Cl⁻ гомеостаз (98-102)
    biological_library[MOD_PMCA]           = pmca_current;
    biological_library[MOD_NCX]            = ncx_current;
    biological_library[MOD_KCC2]           = kcc2_current;
    biological_library[MOD_NKCC1]          = nkcc1_current;
    biological_library[MOD_KV_DELAYED_RECTIFIER] = kv_delayed_rectifier_current;
    biological_library[MOD_KV21]          = kv21_current;
    biological_library[MOD_CA_LEAK]        = ca_leak_current;
    biological_library[MOD_CA_BUFFER]      = ca_buffer_current;

    // Ионные каналы ритмогенеза (103-107)
    biological_library[MOD_SK_CHANNEL]     = sk_channel_current;
    biological_library[MOD_BK_CHANNEL]     = bk_channel_current;
    biological_library[MOD_NAP_CHANNEL]    = nap_channel_current;
    biological_library[MOD_NAP_TADPOLE]    = nap_tadpole_current;
    biological_library[MOD_KIR_CHANNEL]    = kir_channel_current;
    biological_library[MOD_K2P_LEAK]       = k2p_leak_current;

    // Обоняние (108) — имеет GPU fallback
    biological_library[MOD_OLFACTORY_RECEPTOR] = olfactory_receptor_current;
    biological_library[MOD_THERMAL_NOISE]   = thermal_noise_current;
    biological_library[MOD_KV7_MCURRENT]    = kv7_mcurrent_current;
    biological_library[MOD_KV31]            = kv31_current;
    biological_library[MOD_TASK]            = task_current;
    biological_library[MOD_CAN]             = can_current;
    biological_library[MOD_PKD2L1]          = pkd2l1_current;
    biological_library[MOD_PP1_INHIBITOR_DARPP32] = pp1_inhibitor_darpp32_current;
    biological_library[MOD_CONNEXIN_CX36] = connexin_cx36_current;
    biological_library[MOD_CONNEXIN_CX43] = connexin_cx43_current;
    biological_library[MOD_VIBRATION_SENSOR] = vibration_sensor_current;
    biological_library[MOD_OTOLITH_HAIR_CELL]   = otolith_hair_cell_current;
    biological_library[MOD_SEMICIRCULAR_CUPULA] = semicircular_cupula_current;
    biological_library[MOD_ELECTRORECEPTOR_AMPULLARY] = electroreceptor_ampullary_current;
    biological_library[MOD_MAGNETITE_MECHANORECEPTOR] = magnetite_mechanoreceptor_current;
    biological_library[MOD_CRYPTOCHROME_MAGNETIC]     = cryptochrome_magnetic_current;
    biological_library[MOD_KNA_CHANNEL]              = kna_channel_current;
    biological_library[MOD_GLIAL_KIR41]      = glial_kir41_current;
    biological_library[MOD_GLIAL_NAK_PUMP]   = glial_nak_pump_current;
    biological_library[MOD_AXON_NAV16]       = nav16_current;
    biological_library[MOD_AXON_NAV12]       = nav12_current;
    biological_library[MOD_NAV12DEV]         = nav12dev_current;
    biological_library[MOD_NAV11]            = nav11_current;
    biological_library[MOD_CAV21]            = cav21_current;
    biological_library[MOD_CAV14]            = cav14_current;
    biological_library[MOD_CABP4]            = cabp4_current;
    biological_library[MOD_CACC]             = cacc_current;
    biological_library[MOD_GLAST]            = glast_current;
    biological_library[MOD_GLYT1]            = glyt1_current;
    biological_library[MOD_GAT3]             = gat3_current;
    biological_library[MOD_EAAT3]            = eaat3_current;
    biological_library[MOD_GLYT2]            = glyt2_current;
    biological_library[MOD_GAT1]             = gat1_current;
    biological_library[MOD_AQP4]             = aqp4_current;
    biological_library[MOD_CILIA]            = cilia_current;
    biological_library[MOD_SERCA]            = serca_current;
    biological_library[MOD_IP3R]             = ip3r_current;
    biological_library[MOD_RYR]              = ryr_current;
    biological_library[MOD_HCN1]             = hcn1_current;
    biological_library[MOD_HCN2]             = hcn2_current;
    biological_library[MOD_HCN3]             = hcn3_current;
    biological_library[MOD_HCN4]             = hcn4_current;
    biological_library[MOD_NALCN]            = nalcn_current;
    biological_library[MOD_P2X2]             = p2x2_current;
    biological_library[MOD_CAMP_PKA]         = camp_pka_current;
    biological_library[MOD_PLC_IP3_DAG]      = plc_ip3_dag_current;
    biological_library[MOD_PKC]              = pkc_current;
    biological_library[MOD_MRGPR]            = mrgpr_current;

    // Пресинаптические модуляторы (12)
    biological_modulators[MOD_NT_RECEPTOR_5HT1A]     = ht1a_modulate_release;
    biological_modulators[MOD_NT_RECEPTOR_5HT1B]     = ht1b_modulate_release;
    biological_modulators[MOD_NT_RECEPTOR_D2]        = d2_modulate_release;
    biological_modulators[MOD_NT_RECEPTOR_A1]        = a1_modulate_release;
    biological_modulators[MOD_NT_RECEPTOR_A2A]       = a2a_modulate_release;
    biological_modulators[MOD_NT_RECEPTOR_D1]        = d1_modulate_release;
    biological_modulators[MOD_NT_RECEPTOR_MGLUR2]    = mglur2_modulate_release;
    biological_modulators[MOD_NT_RECEPTOR_MGLUR3]    = mglur3_modulate_release;
    biological_modulators[MOD_NT_RECEPTOR_CB1]       = cb1_modulate_release;
    biological_modulators[MOD_NT_RECEPTOR_H3]        = h3_modulate_release;
    biological_modulators[MOD_NT_RECEPTOR_NICOTINIC] = nicotinic_modulate_release;
    biological_modulators[MOD_NT_RECEPTOR_ALPHA2]    = alpha2_modulate_release;

    active_library = biological_library;
    active_modulators = biological_modulators;
}
