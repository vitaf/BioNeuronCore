// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// modifier_helpers.h — ЕДИНЫЕ хелперы для всех модификаторов (CPU + GPU + плагины)
// Этот файл — ЕДИНСТВЕННЫЙ источник формул Нернста, GHK, объёма, площади и clamp.
// Включается из: plugin_helpers.h, hh_biological.cpp, receptors_biological.cpp.
// Концепция: [[concept-single-source]] — нет дублирования формул.

#pragma once
#include "types.h"
#include "run_tick_common.h"  // ghk_monovalent/divalent/anion, segment_volume_compute
#include "units.h"
#include <cmath>
#include <cstdio>

// SYCL/CUDA-совместимость (run_tick_common.h уже определяет, но на всякий случай)
#ifndef BIOCORE_HOST_DEVICE
#if defined(__CUDACC__)
#define BIOCORE_HOST_DEVICE __host__ __device__
#else
#define BIOCORE_HOST_DEVICE
#endif
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// НЕРНСТ — равновесный потенциал (мВ)
// ═══════════════════════════════════════════════════════════════════════════════

static inline BIOCORE_HOST_DEVICE float nernst(float intra, float extra, float valence) {
    if (intra < 1e-9f) intra = 1e-9f;  // 1 нМ — физиол. Ca_i=100нМ не клампится
    if (extra < 1e-9f) extra = 1e-9f;
    return (GAS_CONST * CELSIUS_37) / (valence * FARADAY_CONST) * log(extra / intra) * 1000.0f;
}

// Защита знаменателя GHK от fp32 underflow (используется в модификаторах)
static inline BIOCORE_HOST_DEVICE float safe_ghk_denom(float x) {
    float d = 1.0f - exp(-x);
    if (fabsf(d) < 1e-7f) return (x > 0.0f) ? 1e-7f : -1e-7f;
    return d;
}

// ghk_monovalent/ghk_divalent/ghk_anion — единая реализация в run_tick_common.h (CPU+GPU)

// Одноканальная проницаемость P_single [м³/с] = γ·RT/(z²F²·c̄) — chord-conductance (Hille).
// γ в пС, c̄ в мМ (≡ моль/м³). Честный одномолекулярный перевод (без суррогатного c_ref=150).
// План: notes/PLAN_single_molecule_ghk.md (этап 1).
static inline BIOCORE_HOST_DEVICE double ghk_single_channel_P(float gamma_pS, float z, float cbar_mM) {
    double gamma_S = gamma_pS * PS_TO_S;                       // пС → С
    double z2F2 = z * z * FARADAY_CONST * FARADAY_CONST;       // z²F²
    double RT = GAS_CONST * CELSIUS_37;                        // [Дж/моль]
    return gamma_S * RT / (z2F2 * cbar_mM);                    // [м³/с]
}

// NALCN (неселективный катионный leak): парциальные проницаемости [м³/с].
// γ (пС) — суммарная single-channel проводимость; раскладывается по
// P_Na:P_K:P_Ca = 1.3:1.2:0.5 (Lu 2007 WT), вес иона w_i = f_i·z_i²·c̄_i / Σ.
static inline BIOCORE_HOST_DEVICE void nalcn_partial_P(float gamma_pS,
                                                       double* P_Na, double* P_K, double* P_Ca) {
    const double fNa = 1.3, fK = 1.2, fCa = 0.5;
    double w = fNa*GHK_CBAR_NA_MM + fK*GHK_CBAR_K_MM + fCa*4.0*GHK_CBAR_CA_MM;
    *P_Na = ghk_single_channel_P((float)(gamma_pS * fNa*GHK_CBAR_NA_MM / w), 1.0f, GHK_CBAR_NA_MM);
    *P_K  = ghk_single_channel_P((float)(gamma_pS * fK*GHK_CBAR_K_MM  / w), 1.0f, GHK_CBAR_K_MM);
    *P_Ca = ghk_single_channel_P((float)(gamma_pS * fCa*4.0*GHK_CBAR_CA_MM / w), 2.0f, GHK_CBAR_CA_MM);
}

// Универсальное разложение γ (пС) неселективного канала на парциальные P [м³/с]
// по относительным проницаемостям fNa:fK:fCa (P_Ca/P_Na = fCa/fNa, chord-conductance).
static inline BIOCORE_HOST_DEVICE void ghk_partial_P(float gamma_pS, float fNa, float fK, float fCa,
                                                     double* P_Na, double* P_K, double* P_Ca) {
    double w = fNa*GHK_CBAR_NA_MM + fK*GHK_CBAR_K_MM + fCa*4.0*GHK_CBAR_CA_MM;
    *P_Na = ghk_single_channel_P((float)(gamma_pS * fNa*GHK_CBAR_NA_MM / w), 1.0f, GHK_CBAR_NA_MM);
    *P_K  = ghk_single_channel_P((float)(gamma_pS * fK*GHK_CBAR_K_MM  / w), 1.0f, GHK_CBAR_K_MM);
    *P_Ca = ghk_single_channel_P((float)(gamma_pS * fCa*4.0*GHK_CBAR_CA_MM / w), 2.0f, GHK_CBAR_CA_MM);
}

// ═══════════════════════════════════════════════════════════════════════════════
// ЕДИНЫЙ GHK-ток в SI: P[см/с] × flux[C/m³] × area[м²] → I[пА].
// c в моль/м³ (= мМ), V в мВ, area в µm², open_prob — гейтинг (m, m³h, ...).
// Единственные множители — точные переводы единиц (CM_TO_M, UM2_TO_M2, A_TO_PA).
// ═══════════════════════════════════════════════════════════════════════════════
static inline BIOCORE_HOST_DEVICE double ghk_current_pA(
    float P_cm_s, float z, float V_mV, double c_in_mol_m3, double c_out_mol_m3,
    float area_um2, float open_prob)
{
    double flux = (z >= 2.0f) ? ghk_divalent(V_mV, c_in_mol_m3, c_out_mol_m3)
                             : ghk_monovalent(V_mV, c_in_mol_m3, c_out_mol_m3);
    // [см/с→м/с] · [C/m³] · [µm²→м²] · [А→пА]
    return (P_cm_s * CM_TO_M) * flux * (area_um2 * UM2_TO_M2) * A_TO_PA * open_prob;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ГЕОМЕТРИЯ СЕГМЕНТА
// ═══════════════════════════════════════════════════════════════════════════════

// Площадь поверхности (μm²): сома → сфера (4πr²), остальные → цилиндр (2πrL)
static inline BIOCORE_HOST_DEVICE float segment_area(const Segment* s) {
    if (s->segment_type == 1 || s->segment_type == 3 || s->segment_type == 4)
        return 4.0f * PI * s->radius * s->radius;  // сома/бутон/шипик
    return 2.0f * PI * s->radius * s->length;       // дендрит/аксон
}

// Объём сегмента — единый источник: segment_volume_compute (run_tick_common.h).
static inline BIOCORE_HOST_DEVICE double segment_volume(const Segment* s) {
    return segment_volume_compute(s);
}

// Эффективная площадь для расчёта проводимости мембранных белков
static inline BIOCORE_HOST_DEVICE float effective_area(const Modifier* m, const Segment* s) {
    if (m->state[7] > 0.0f) return m->state[7];
    return segment_area(s);
}

// Количество мембранного белка: density × area (для каналов/рецепторов/насосов)
static inline BIOCORE_HOST_DEVICE float membrane_protein_amount(const Modifier* m, const Segment* s) {
    return m->density * effective_area(m, s);
}

// Количество фермента/сигнального белка: density × volume (для энзимов, factors)
// Используется для типов 82-92 (STDP cascade, consolidation): концентрация × объём = молекулы
static inline BIOCORE_HOST_DEVICE float enzyme_amount(const Modifier* m, const Segment* s) {
    return m->density * segment_volume(s);
}

// Калибровка проницаемости GHK-каналов: 1e-15 (НЕ magic-число).
// Выведено из лабораторной проводимости TREK-1 (~100 пС → P≈1e-7 см/с при 0.05 мС/см²).
// Заряд на моль для пересчёта тока в изменение концентрации
static inline BIOCORE_HOST_DEVICE double charge_per_mole(double volume) {
    return FARADAY_CONST * volume / 1000.0;
}

static inline BIOCORE_HOST_DEVICE float hh_vtrap(float x, float y) {
    if (fabsf(x / y) < 1e-6f) return y * (1.0f - x / (2.0f * y));
    return x / (1.0f - exp(-x / y));
}

// ═══════════════════════════════════════════════════════════════════════════════
// NAV16 (тип 141) — аксональный Na⁺ (Nav1.6/SCN8A), transient m³h.
// ✅ ДОСТОВЕРНО — PNAS 2018 HH-модель: repetitive firing (не глубокая инактивация dIN).
// m: α=0.182·(V+43)/(1−exp(−(V+43)/6)), β=−0.124·(V+43)/(1−exp((V+43)/6)).
// h: h_inf=1/(1+exp((V+72)/6.2)), α=0.024·(V+50)/(1−exp(−(V+50)/5)),
//    β=−0.0091·(V+75)/(1−exp((V+75)/5)). GHK-парадигма (γ=20.7 пС), как NaP_105.
// См. notes/modifier_refs/NAV16_141.md.
// ═══════════════════════════════════════════════════════════════════════════════
#define NAV16_GAMMA_PS  20.7f  // single-channel Nav1.6 (пС)

static inline BIOCORE_HOST_DEVICE float nav16_am(float V) { return 0.182f * hh_vtrap(V + 43.0f, 6.0f); }
static inline BIOCORE_HOST_DEVICE float nav16_bm(float V) { return 0.124f * hh_vtrap(-(V + 43.0f), 6.0f); }
static inline BIOCORE_HOST_DEVICE float nav16_ah(float V) { return 0.024f * hh_vtrap(V + 50.0f, 5.0f); }
static inline BIOCORE_HOST_DEVICE float nav16_bh(float V) { return 0.0091f * hh_vtrap(-(V + 75.0f), 5.0f); }

static inline BIOCORE_HOST_DEVICE float nav16_m_inf(float V) {
    float a = nav16_am(V), b = nav16_bm(V);
    return a / (a + b);
}
static inline BIOCORE_HOST_DEVICE float nav16_m_tau(float V) {
    return 1.0f / (nav16_am(V) + nav16_bm(V));
}
static inline BIOCORE_HOST_DEVICE float nav16_h_inf(float V) {
    return 1.0f / (1.0f + exp((V + 72.0f) / 6.2f));
}
static inline BIOCORE_HOST_DEVICE float nav16_h_tau(float V) {
    return 1.0f / (nav16_ah(V) + nav16_bh(V));
}

// ═══════════════════════════════════════════════════════════════════════════════
// NAV12 (тип 142) — аксональный Na⁺ Nav1.2 (SCN2A), transient m³h.
// ✅ Тадпольный спинальный Na-изоформ (ранние нейроны Xenopus экспрессируют Nav1.2,
// НЕ Nav1.6). Активация V½≈−23 мВ, инактивация V½≈−63.5 мВ (JGP 2022, Rush 2005).
// Деполяризованнее Nav1.6 (−43/−72) → доступен при менее гиперполяризованном покое.
// GHK-парадигма (γ≈20 пС). См. notes/modifier_refs/NAV12_142.md.
#define NAV12_GAMMA_PS  20.8f  // single-channel Nav1.2 (пС, Ben-Shalom 2017: 20.8±1.8)

static inline BIOCORE_HOST_DEVICE float nav12_am(float V) { return 0.182f * hh_vtrap(V + 23.0f, 6.0f); }
static inline BIOCORE_HOST_DEVICE float nav12_bm(float V) { return 0.124f * hh_vtrap(-(V + 23.0f), 6.0f); }
static inline BIOCORE_HOST_DEVICE float nav12_ah(float V) { return 0.024f * hh_vtrap(V + 41.5f, 5.0f); }
static inline BIOCORE_HOST_DEVICE float nav12_bh(float V) { return 0.0091f * hh_vtrap(-(V + 66.5f), 5.0f); }

static inline BIOCORE_HOST_DEVICE float nav12_m_inf(float V) {
    float a = nav12_am(V), b = nav12_bm(V);
    return a / (a + b);
}
static inline BIOCORE_HOST_DEVICE float nav12_m_tau(float V) {
    return 1.0f / (nav12_am(V) + nav12_bm(V));
}
static inline BIOCORE_HOST_DEVICE float nav12_h_inf(float V) {
    return 1.0f / (1.0f + exp((V + 63.5f) / 6.2f));
}
static inline BIOCORE_HOST_DEVICE float nav12_h_tau(float V) {
    return 1.0f / (nav12_ah(V) + nav12_bh(V));
}

// ═══════════════════════════════════════════════════════════════════════════════
// NAV11 (тип 199) — Nav1.1 (SCN1A) transient Na⁺, repetitive (PV⁺-интернейроны).
// ✅ γ=17 пС (Vanoye 2006, PMID 16380441). V½_акт=−30, V½_инакт=−65 (нейроны, PMC4220023).
// Быстрый однофазный recovery (repetitive) — НЕ slow 500 мс (в отличие от NAV12DEV single-spike).
#define NAV11_GAMMA_PS  17.0f
static inline BIOCORE_HOST_DEVICE float nav11_am(float V) { return 0.182f * hh_vtrap(V + 30.0f, 6.5f); }
static inline BIOCORE_HOST_DEVICE float nav11_bm(float V) { return 0.124f * hh_vtrap(-(V + 30.0f), 6.5f); }
static inline BIOCORE_HOST_DEVICE float nav11_ah(float V) { return 0.024f * hh_vtrap(V + 45.0f, 5.0f); }
static inline BIOCORE_HOST_DEVICE float nav11_bh(float V) { return 0.0091f * hh_vtrap(-(V + 65.0f), 5.0f); }
static inline BIOCORE_HOST_DEVICE float nav11_m_inf(float V) {
    float a = nav11_am(V), b = nav11_bm(V);
    return a / (a + b);
}
static inline BIOCORE_HOST_DEVICE float nav11_m_tau(float V) {
    return 1.0f / (nav11_am(V) + nav11_bm(V));
}
static inline BIOCORE_HOST_DEVICE float nav11_h_inf(float V) {
    return 1.0f / (1.0f + exp((V + 65.0f) / 5.0f));
}
static inline BIOCORE_HOST_DEVICE float nav11_h_tau(float V) {
    return 1.0f / (nav11_ah(V) + nav11_bh(V));
}

// ═══════════════════════════════════════════════════════════════════════════════
// NAV12DEV (тип 193) — тадпольный аксональный Na⁺ (Nav1.2 developmental, Scn2a).
// ✅ V½_акт=−16 (Xenopus, Lockery & Spitzer 1988) + V½_инакт=−45 (баланс: single-spike +
// RB→dIN кик). O'Dowd 1988: НЕТ developmental-сдвига V½_inact. Прежняя оценка −35 — НЕВЕРНА.
// GHK (γ=20.8 пС). См. notes/modifier_refs/NAV12DEV_193.md.
#define NAV12DEV_GAMMA_PS 20.8f  // single-channel Nav1.2 developmental (пС, Ben-Shalom 2017)
// Двухфазный recovery h (single-spike dIN, Li 2015 + O'Dowd 1988): после деполяризации h
// восстанавливается двумя компонентами — fast (τ~мс) + slow (τ~сотни мс). Slow-доля держит Na
// частично инактивированным → обрыв повторного спайка. ⚠️ Доля/τ — ОЦЕНКА (уточнить O'Dowd 1988).
#define NAV12DEV_SLOW_FRAC    0.7f     // доля медленной (slow) компоненты recovery h
#define NAV12DEV_SLOW_TAU_MS  500.0f   // τ медленного recovery h (мс)

// m-кинетика: V½_акт=−16 (Xenopus). Отдельная от Nav1.2 (V½_акт=−23, млекопитающий).
static inline BIOCORE_HOST_DEVICE float nav12dev_am(float V) { return 0.182f * hh_vtrap(V + 16.0f, 6.0f); }
static inline BIOCORE_HOST_DEVICE float nav12dev_bm(float V) { return 0.124f * hh_vtrap(-(V + 16.0f), 6.0f); }
static inline BIOCORE_HOST_DEVICE float nav12dev_m_inf(float V) {
    float a = nav12dev_am(V), b = nav12dev_bm(V);
    return a / (a + b);
}
static inline BIOCORE_HOST_DEVICE float nav12dev_m_tau(float V) {
    return 1.0f / (nav12dev_am(V) + nav12dev_bm(V));
}

static inline BIOCORE_HOST_DEVICE float nav12dev_h_inf(float V, float vhalf = 45.0f) {
    return 1.0f / (1.0f + exp((V + vhalf) / 6.2f));  // V½_инакт (мВ), default −45 (баланс single-spike + RB→dIN кик)
}

// ═══════════════════════════════════════════════════════════════════════════════
// CAV21 (тип 140) — пресинаптический Ca²⁺-канал P/Q-type (CACNA1A).
// ✅ ДОСТОВЕРНО — триггер выброса нейромедиатора. Быстрая активация (τ_m≈1 мс)
// + медленная инактивация (τ_h≈300 мс). Gambeta 2021 (V½_акт=−15, V½_инакт=−50);
// modal gating (τ_инакт 202–615 мс); γ 9–19 пС (Пуркинье/α1A). См. notes/modifier_refs/CAV21_140.md.
// ═══════════════════════════════════════════════════════════════════════════════
#define CAV21_GAMMA_PS     15.0f   // P/Q-type γ (9–19 пС, берём 15)
#define CAV21_VHALF_ACT   -15.0f   // V½ активации (Gambeta 2021)
#define CAV21_K_ACT         6.0f
#define CAV21_TAU_M_MS      1.0f   // быстрая активация (0.4–1.1 мс)
#define CAV21_VHALF_INACT -50.0f   // V½ инактивации (Gambeta 2021)
#define CAV21_K_INACT       8.0f
#define CAV21_TAU_H_MS    300.0f   // медленная инактивация (202–615 мс)
#define CAV21_CDI_KD_UM     3.0f   // слабая Ca²⁺-зав. инактивация
// τ фиксированы (τ_m=1 мс, τ_h=300 мс) — V-зависимость Cav2.1 слабая: τ_m весь в 0.4–1.1 мс,
// а τ_h (202–615 мс) на масштабе спайка (1–2 мс) не влияет на выброс в бутон. Канал слабо
// исследован на явные τ_m(V)/τ_h(V). Возможный (НЕ откалиброванный лабораторно) вариант —
// bell-shaped: τ(V) = C + 1/(exp((V−V1)/k1) + exp((V−V2)/k2)), под диапазон τ_m 0.4–1.1 мс /
// τ_h 202–615 мс. Требует лабораторной сверки (McRory 2004; Inchauspe 2004; Li/Bischofberger/Jonas 2007),
// прежде чем заменять фиксированные τ.

static inline BIOCORE_HOST_DEVICE float cav21_m_inf(float V) {
    return 1.0f / (1.0f + exp(-(V - CAV21_VHALF_ACT) / (CAV21_K_ACT + 1e-6f)));
}
static inline BIOCORE_HOST_DEVICE float cav21_h_inf(float V, float Ca_uM) {
    float h = 1.0f / (1.0f + exp((V - CAV21_VHALF_INACT) / CAV21_K_INACT));
    h *= 1.0f / (1.0f + Ca_uM / CAV21_CDI_KD_UM);
    return h;
}

// ═══════════════════════════════════════════════════════════════════════════════
// CAV14 (тип 144) — Cav1.4 (CACNA1F) L-type Ca²⁺, фоторецептор.
// ✅ ДОСТОВЕРНО — γ=1–4 пС (Doering 2005), БЕЗ CDI (ICDI-домен), медленная VDI.
// ⚠️ НЕ чётко — V½_bare=−15 мВ (Ca²⁺, полноразмерный; разброс −10…−17 в зависимости
// от носителя/протокола/сплайс-варианта), k=7, τ_m=2 мс (по аналогии с L-type).
// CaBP4 (145) связывает IQ-мотив → снимает ингибицию ICDI/CTM → ΔV½=−10 мВ (Shaltiel 2012).
// Доп. эффект — reduced VDI (вторичный; на потенциалах фоторецептора −45…−60 мВ пренебрежим,
// поэтому h≡1; Park 2014 JBC 289:31262, обзор 2012 Front Mol Neurosci 5:26).
// См. notes/modifier_refs/CAV14_144.md.
// ═══════════════════════════════════════════════════════════════════════════════
#define CAV14_GAMMA_PS      4.0f   // γ (1–4 пС, берём 4; Doering 2005)
#define CAV14_VHALF_BARE  -15.0f   // V½ голого (Ca²⁺, полн.) — НЕ чётко (−10…−17)
#define CAV14_K_ACT         7.0f   // slope — НЕ чётко (по аналогии с L-type)
#define CAV14_TAU_M_MS      2.0f   // активация — НЕ чётко (τ_open≈3 мс, Doering)
#define CAV14_CABP4_DVHALF -10.0f  // сдвиг V½ от CaBP4 (Shaltiel 2012) — твёрдо

static inline BIOCORE_HOST_DEVICE float cav14_m_inf(float V, float Vhalf) {
    return 1.0f / (1.0f + exp(-(V - Vhalf) / (CAV14_K_ACT + 1e-6f)));
}

// ═══════════════════════════════════════════════════════════════════════════════
// CABP4 (тип 145) — Ca²⁺-binding protein 4, модулятор Cav1.4 (НЕ канал).
// ✅ ДОСТОВЕРНО — связывает IQ-мотив Cav1.4, снимает ингибицию ICDI/CTM (Shaltiel 2012).
// См. notes/modifier_refs/CABP4_145.md.
// ═══════════════════════════════════════════════════════════════════════════════
#define CABP4_TAU_BIND_MS   1.0f   // белок-белок связывание — оценка, НЕ чётко

// Доля Cav1.4 в сегменте, связанная CaBP4 (0…1). 0 — если CaBP4 нет в составе.
BIOCORE_HOST_DEVICE static inline float segment_cabp4_bound(const Segment* s) {
    uint8_t mc = s->modifier_count;
    if (mc > MAX_MODIFIERS_PER_SEGMENT) mc = 0;
    for (uint8_t j = 0; j < mc; j++)
        if (s->modifiers[j].type == MOD_CABP4)
            return s->modifiers[j].state[0];
    return 0.0f;
}

// ═══════════════════════════════════════════════════════════════════════════════
// CACC (тип 146) — CaCC/ANO2 (TMEM16B), Ca²⁺-активируемый Cl⁻-канал.
// ✅ ДОСТОВЕРНО — Ca²⁺ EC50=4 µM, Hill n=2 (Cenedese 2011); Cl⁻-селективен (ghk_anion).
// ⚠️ НЕ чётко — γ=2 пС (структурная оценка: димерная scramblase-подобная пора, слабо
// селективная → малая γ; TMEM16B unitary conductance в литературе НЕ сообщается,
// гомолог TMEM16A ~8 пС). V-зависимость НЕ моделируется (вторичная).
// См. notes/modifier_refs/CACC_146.md.
// ═══════════════════════════════════════════════════════════════════════════════
#define CACC_GAMMA_PS      2.0f   // γ — структурная оценка, НЕ чётко
#define CACC_EC50_UM       4.0f   // Ca²⁺ EC50 (Cenedese 2011: 4.6 µM @ −50 мВ, 2.8 µM @ +50 мВ)
#define CACC_HILL          2.0f   // Hill 1–2 (Cenedese 2011)
#define CACC_TAU_MS        2.0f   // активация (оценка)

static inline BIOCORE_HOST_DEVICE float cacc_m_inf(float ca_um) {
    float ca_n = pow(ca_um, CACC_HILL);
    float ec50_n = pow(CACC_EC50_UM, CACC_HILL);
    return ca_n / (ca_n + ec50_n);
}

// ═══════════════════════════════════════════════════════════════════════════════
// GLAST (тип 147) — GLAST/GLT-1 (EAAT1/2), глиальный глутаматный транспортер.
// ✅ ДОСТОВЕРНО — стехиометрия 3 Na⁺ + 1 H⁺ + 1 глутамат (внутрь), 1 K⁺ (наружу),
// чистый +2 заряда (Zerangue & Kavanaugh 1996); K_m глутамата 10–20 µM; оборот ~16 с⁻¹ (GLAST).
// Заменяет tau_clearance: глутамат клирится глиальным захватом (Michaelis-Menten).
// ⚠️ I_max — калибровка (этап 6). См. notes/modifier_refs/GLAST_147.md.
// ═══════════════════════════════════════════════════════════════════════════════
#define GLAST_K_M_REL          10.0f   // K_m глутамата 15 µM → отн.ед. (1.0 = NMDA EC50 ≈ 1.5 µM)
#define GLAST_IMAX_UA_PER_CM2   2.0f   // I_max транспортера (µA/см²) — калибровка
#define GLAST_GLU_REL_PER_MM  667.0f   // 1 мМ глутамата = 667 отн.ед. (NMDA EC50 ≈ 1.5 µM)

static inline BIOCORE_HOST_DEVICE float glast_saturation(float glu_rel) {
    return glu_rel / (glu_rel + GLAST_K_M_REL);  // Michaelis-Menten 0..1 (отн.ед.)
}

// ═══════════════════════════════════════════════════════════════════════════════
// GLYT1 (тип 148) — глиальный транспортер глицина (SLC6A9).
// ✅ ДОСТОВЕРНО — 2 Na⁺ + 1 Cl⁻ + глицин (внутрь), net +1 (Erdem 2019 JGP 151:1035);
// глицин EC50 ≈ 44.7 µM. ⚠️ K_m в отн.ед. — калибровка (задача #20).
// ═══════════════════════════════════════════════════════════════════════════════
#define GLYT1_K_M_REL          30.0f   // глицин K_m 45 µM → отн.ед. (калибровка)
#define GLYT1_IMAX_UA_PER_CM2   2.0f   // калибровка
#define GLYT1_GLY_REL_PER_MM  667.0f   // 1 мМ глицина → отн.ед. (общий перевод, калибровка)

// ═══════════════════════════════════════════════════════════════════════════════
// GAT3 (тип 149) — глиальный транспортер GABA (SLC6A11).
// ✅ ДОСТОВЕРНО — 1 GABA : 2 Na⁺ : 1 Cl⁻ (внутрь), net +1; GABA K_m ≈ 0.8 µM (высокое сродство).
// ⚠️ K_m в отн.ед. — калибровка.
// ═══════════════════════════════════════════════════════════════════════════════
#define GAT3_K_M_REL            0.5f   // GABA K_m 0.8 µM → отн.ед. (калибровка)
#define GAT3_IMAX_UA_PER_CM2    2.0f   // калибровка
#define GAT3_GABA_REL_PER_MM  667.0f   // 1 мМ GABA → отн.ед. (калибровка)

// ═══════════════════════════════════════════════════════════════════════════════
// EAAT3 (тип 183) — EAAC1 (SLC1A1), нейрональный глутаматный транспортер.
// ✅ ДОСТОВЕРНО — стехиометрия 3 Na⁺ + 1 H⁺ + 1 глутамат (внутрь), 1 K⁺ (наружу),
// чистый +2 (Zerangue & Kavanaugh 1996). Нейрональный пресинаптический/соматический reuptake.
// ⚠️ K_m глутамата ~28–30 µM (EAAC1, Zerangue & Kavanaugh 1996) → отн.ед. I_max — калибровка.
// ═══════════════════════════════════════════════════════════════════════════════
#define EAAT3_K_M_REL          20.0f   // глутамат K_m ~30 µM → отн.ед. (1.0 = NMDA EC50 ≈ 1.5 µM)
#define EAAT3_IMAX_UA_PER_CM2   2.0f   // I_max транспортера (µA/см²) — калибровка
#define EAAT3_GLU_REL_PER_MM  667.0f   // 1 мМ глутамата = 667 отн.ед.

static inline BIOCORE_HOST_DEVICE float eaat3_saturation(float glu_rel) {
    return glu_rel / (glu_rel + EAAT3_K_M_REL);  // Michaelis-Menten 0..1 (отн.ед.)
}

// ═══════════════════════════════════════════════════════════════════════════════
// GLYT2 (тип 184) — глициновый транспортер 2 (SLC6A5), нейрональный.
// ✅ ДОСТОВЕРНО — 2 Na⁺ + 1 Cl⁻ + глицин (внутрь), net +1. Высокое сродство к глицину
// (K_m ~26 µM, ниже чем у GlyT1) — пресинаптический reuptake глицинергических нейронов.
// ═══════════════════════════════════════════════════════════════════════════════
#define GLYT2_K_M_REL          55.7f   // глицин EC50 ~83.5 µM → отн.ед. (Erdem 2019 J Gen Physiol)
#define GLYT2_IMAX_UA_PER_CM2   2.0f   // калибровка
#define GLYT2_GLY_REL_PER_MM  667.0f   // 1 мМ глицина → отн.ед. (общий перевод, калибровка)

// ═══════════════════════════════════════════════════════════════════════════════
// GAT1 (тип 185) — GABA транспортер 1 (SLC6A1), нейрональный.
// ✅ ДОСТОВЕРНО — 1 GABA : 2 Na⁺ : 1 Cl⁻ (внутрь), net +1; GABA K_m ≈ 5–10 µM (ниже
// сродства чем GAT3 ~0.8 µM). Пресинаптический reuptake GABA-ергических нейронов.
// ═══════════════════════════════════════════════════════════════════════════════
#define GAT1_K_M_REL            4.7f   // GABA K_m ~7 µM → отн.ед. (калибровка)
#define GAT1_IMAX_UA_PER_CM2    2.0f   // калибровка
#define GAT1_GABA_REL_PER_MM  667.0f   // 1 мМ GABA → отн.ед. (калибровка)

// ── AQP4 (аквапорин-4) — водный канал ──
#define AQP4_PF_CM3_S        24e-14f  // single-channel водная проницаемость (см³/с, Yang & Verkman 1997, PMID 9195910)
#define AQP4_OSMOTIC_SCALE   1e-6f    // mean-field масштаб осмотического потока (калибровка этап 6)

// ── #32+#40: цилиарный генератор → скорость CSF (мкм/с) ──
// КАК СДЕЛАНО: реснички эпендимы бьют ~12 Гц (биодиапазон лягушки 5–20 Гц) → скалярная
// СКОРОСТЬ 0..10 мкм/с (zebrafish центральный канал ±4.8 мкм/с, Thouvenin 2020). state[2] = фаза (мс).
// Скалярная скорость (без направления/сдвига) — приближение.
// КАК ДОЛЖНО БЫТЬ: гидродинамическое поле (вектор скорости + сдвиг на ресничках), требует
// геометрии вентрикулярной поверхности (в Studio) — отдельная задача. См. notes/modifier_refs/CSF_FLOW.md.
static inline BIOCORE_HOST_DEVICE float csf_ciliary_flow(Modifier* m, float dt) {
    m->state[2] += dt;
    float phase = m->state[2] * 0.012f * 2.0f * 3.14159265f;  // 12 Гц (5–20 Гц лягушка)
    return 5.0f * (1.0f + sinf(phase));  // скорость CSF 0..10 мкм/с
}

// Порог полу-активации PKD2L1 по скорости потока (мкм/с). КАЛИБРОВКА: в литературе точного
// порога сдвига нет (перекрывается с Piezo1) — оценка по порядку величины скорости CSF.
#define PKD2L1_FLOW_HALF_UM_S  2.0f

// ── Общий механизм Na⁺-сопряжённого NT-транспортера (глиального И нейронального) ──
// Единый хелпер для GLAST/GlyT1/GAT3 (глия) и EAAT3/GlyT2/GAT1 (нейроны) — Single Source.
// Стехиометрия: na_in Na⁺ внутрь + 1 NT внутрь + (cl_in Cl⁻ внутрь ИЛИ k_out K⁺ наружу).
// net_charge — чистый заряд/цикл (GLAST/EAAT3 +2 = 3Na−1K; GlyT1/GAT3/GlyT2/GAT1 +1 = 2Na+1Cl).
BIOCORE_HOST_DEVICE static inline float glial_nt_uptake_current(
    Modifier* m, Segment* s, float dt, int nt_index,
    float k_m_rel, float imax_ua_cm2, float rel_per_mm,
    int na_in, int cl_in, int k_out, int net_charge)
{
    float area = effective_area(m, s);
    float NT = s->neurotransmitter[nt_index];
    if (NT < 0.0f) NT = 0.0f;
    float frac = NT / (NT + k_m_rel);

    float I = imax_ua_cm2 * m->density * area * UA_PER_CM2_TO_PA * frac;  // пА

    float volume = segment_volume(s);
    double cpm = FARADAY_CONST * volume / 1000.0;
    float transport = I * dt / ((float)net_charge * cpm);

    float nt_avail = NT / rel_per_mm;
    float na_avail = s->extra_ions[ION_NA] * EXTRA_VOLUME_RATIO / (float)na_in;
    if (transport > nt_avail) transport = nt_avail;
    if (transport > na_avail) transport = na_avail;
    if (k_out && transport > s->intra_ions[ION_K]) transport = s->intra_ions[ION_K];
    if (transport < 0.0f) transport = 0.0f;

    s->intra_ions[ION_NA] += (float)na_in * transport;
    s->extra_ions[ION_NA] -= (float)na_in * transport / EXTRA_VOLUME_RATIO;
    if (cl_in) {
        s->intra_ions[ION_CL] += (float)cl_in * transport;
        s->extra_ions[ION_CL] -= (float)cl_in * transport / EXTRA_VOLUME_RATIO;
    }
    if (k_out) {
        s->intra_ions[ION_K] -= (float)k_out * transport;
        s->extra_ions[ION_K] += (float)k_out * transport / EXTRA_VOLUME_RATIO;
    }
    s->neurotransmitter[nt_index] -= transport * rel_per_mm;
    if (s->neurotransmitter[nt_index] < 0.0f) s->neurotransmitter[nt_index] = 0.0f;

    return I;  // входящий ток (деполяризующий)
}

// ═══════════════════════════════════════════════════════════════════════════════
// Kv4.2 (A-type) — V-зависимая инактивация/де-инактивация (τ_h).
// A-type: быстрая N-type инактивация при деполяризации (~11–19 мс) + медленная
// де-инактивация (recovery) при гиперполяризации (~131–201 мс, closed-state recovery).
// τ_h(V) — сигмоидальный переход быстрый↔медленный (Birnbaum 2004; Jerng 2004).
// ═══════════════════════════════════════════════════════════════════════════════
#define KV42_TAU_H_FAST_MS   15.0f   // инактивация при деполяр. (11–19 мс)
#define KV42_TAU_H_SLOW_MS  170.0f   // де-инактивация при гиперполяр. (131–201 мс)
#define KV42_V_REC_HALF_MV  -60.0f   // переход инакт.↔де-инакт.
#define KV42_K_REC_MV         8.0f   // крутизна перехода

static inline BIOCORE_HOST_DEVICE float kv42_h_tau(float V) {
    return KV42_TAU_H_FAST_MS
         + (KV42_TAU_H_SLOW_MS - KV42_TAU_H_FAST_MS)
           / (1.0f + exp((V - KV42_V_REC_HALF_MV) / KV42_K_REC_MV));
}

// Kv4.3 (KCND3) — A-type K⁺, быстрее инактивируется чем Kv4.2. Кинетика: Wang 2004 (15004209), Faivre 1999 (10325966).
#define KV43_VHALF_M -31.7f  // активация (ооциты, 2 мМ K⁺)
#define KV43_K_M      8.0f   // slope (варьирует 6.5–19.7 между системами; берём типичное A-type)
#define KV43_VHALF_H -47.4f  // инактивация (рекомб. HEK293)
#define KV43_K_H      5.9f
#define KV43_GAMMA_PS 4.4f  // single-channel γ (19279261)
#define KV43_TAU_M_MS   1.7f  // быстрая активация (+60 мВ: 1.71 мс)
#define KV43_TAU_H_FAST_MS 27.6f  // инактивация при деполяр. (+60 мВ)
#define KV43_TAU_H_SLOW_MS 142.0f  // де-инактивация (recovery)
#define KV43_V_REC_HALF_MV -60.0f
#define KV43_K_REC_MV        8.0f
static inline BIOCORE_HOST_DEVICE float kv43_h_tau(float V) {
    return KV43_TAU_H_FAST_MS
         + (KV43_TAU_H_SLOW_MS - KV43_TAU_H_FAST_MS)
           / (1.0f + exp((V - KV43_V_REC_HALF_MV) / KV43_K_REC_MV));
}
// Kv4.3 фармакология: 4-AP 488 µM, HpTx3 81 нМ (11598014)
#define KV43_4AP_IC50_UM     488.0f
#define KV43_HPTX3_IC50_NM    81.0f

// Kv3.2 (KCNC2) — high-threshold K⁺, non-inactivating (в отличие от Kv3.1 slow-inactivating).
// Кинетика: Clatot 2024 (38194456), γ Hernandez-Pineda 1999 (10482766).
#define KV32_VHALF_M 10.0f  // активация (7.9±1.5 мВ, HEK)
#define KV32_K_M      8.0f
#define KV32_GAMMA_PS 15.0f  // γ (14–16 пС)
#define KV32_TAU_M_MS 12.0f  // активация @+10 мВ (12.0±1.9 мс)
// Kv3.2 фармакология: TEA ~63 µM, 4-AP ~25 µM (IUPHAR/BPS 2017)
#define KV32_TEA_IC50_UM    63.0f
#define KV32_4AP_IC50_UM    25.0f

// Kv3.3 (KCNC3) — high-threshold K⁺, A-type (медленная инактивация ~200 мс, в отличие от Kv3.1/3.2 non-inactivating).
// Кинетика: Duarri 2015 (25756792), Desai 2008 (18539595), γ Fernandez 2003 (12923191).
#define KV33_VHALF_M 1.5f  // активация (+1.46±2.2 мВ, HEK)
#define KV33_K_M      8.0f
#define KV33_VHALF_H -27.9f  // инактивация (A-type, murine)
#define KV33_K_H      8.0f
#define KV33_GAMMA_PS 35.0f  // γ (32–38 пС)
#define KV33_TAU_M_MS  4.0f  // активация (точное τ_m не найдено, «быстрая»)
#define KV33_TAU_H_MS 200.0f  // инактивация (A-type медленная, ~200 мс)
// Kv3.3 фармакология: TEA 67.5 µM, 4-AP <100 µM (20357073)
#define KV33_TEA_IC50_UM    67.5f
#define KV33_4AP_IC50_UM   100.0f

// Kv2.2 (KCNB2) — медленный delayed rectifier, медленнее Kv2.1. Кинетика: Salinas 1997 (9305895), Schmalz 1998 (9612272).
#define KV22_VHALF_M 13.2f  // активация (ооциты)
#define KV22_K_M      9.0f
#define KV22_VHALF_H -16.6f  // инактивация (слабая U-type)
#define KV22_K_H      6.0f
#define KV22_GAMMA_PS 14.5f  // γ (14–15 пС)
#define KV22_TAU_M_MS 32.0f  // активация (медленнее Kv2.1 18.5 мс)
#define KV22_TAU_H_MS 2000.0f  // слабая/медленная инактивация
// Kv2.2 фармакология: TEA 2.6 мМ, 4-AP 1.5 мМ (9612272)
#define KV22_TEA_IC50_MM     2.6f
#define KV22_4AP_IC50_MM     1.5f

// Kv1.4 (KCNA4) — A-type K⁺, N-type инактивация (N-концевой ball-and-chain). Быстрая инакт + медленный recovery.
// Кинетика: Zemel 2018 (30127716), Bett 2012 (23062336).
#define KV14_VHALF_M -21.7f  // активация (ооциты)
#define KV14_K_M      8.0f
#define KV14_VHALF_H -51.8f  // инактивация (N-type, hKv1.4)
#define KV14_K_H      3.2f
#define KV14_GAMMA_PS 9.3f  // γ
#define KV14_TAU_M_MS  2.0f  // быстрая активация
#define KV14_TAU_H_FAST_MS 50.0f  // N-type инактивация при деполяр.
#define KV14_TAU_H_SLOW_MS 1500.0f  // медленный recovery (~1–2 с, use-dependent)
#define KV14_V_REC_HALF_MV -80.0f
#define KV14_K_REC_MV       10.0f
static inline BIOCORE_HOST_DEVICE float kv14_h_tau(float V) {
    return KV14_TAU_H_FAST_MS
         + (KV14_TAU_H_SLOW_MS - KV14_TAU_H_FAST_MS)
           / (1.0f + exp((V - KV14_V_REC_HALF_MV) / KV14_K_REC_MV));
}
// Kv1.4 фармакология: 4-AP ~13 µM (чувствителен), TEA >100 мМ (нечувствителен — Val вместо Thr)
#define KV14_4AP_IC50_UM    13.0f
#define KV14_TEA_IC50_MM   100.0f

// Kv1.6 (KCNA6) — delayed rectifier, non-inactivating (NIP-домен препятствует N-инактивации).
// Кинетика: Salpietro 2023 (36318112), Kirsch 1991 (1993474).
#define KV16_VHALF_M -8.1f  // активация (WT, 250 мс)
#define KV16_K_M      13.0f
#define KV16_GAMMA_PS 8.2f  // γ
#define KV16_TAU_M_MS 20.0f  // активация ~20 мс @0 мВ
// Kv1.6 фармакология: α-DTX 25 нМ, TEA 1.7 мМ, 4-AP 0.3 мМ (нечувствителен к charybdotoxin)
#define KV16_TEA_IC50_MM    1.7f
#define KV16_4AP_IC50_MM    0.3f

// Kv3.4 (KCNC4) — V-зависимая кинетика A-type (Rudy et al., EMBO Mol Med 2014 suppl. Table D5).
// τ_m(V) = 40 − 37.7/(1+exp(−V/28)); τ_h(V) = 12 + 165.6·exp(−V/11.2).
// τ_h расходится при глубокой гиперполяр. (recovery) → cap 2000 мс (числ. стабильность;
// точная recovery τ Kv3.4 — нужна лит-ра).
#define KV34_TAU_M_BASE_MS   40.0f
#define KV34_TAU_M_AMP_MS    37.7f
#define KV34_TAU_M_K_MV      28.0f
#define KV34_TAU_H_BASE_MS   12.0f
#define KV34_TAU_H_AMP_MS   165.6f
#define KV34_TAU_H_K_MV      11.2f
#define KV34_TAU_H_MAX_MS  2000.0f

static inline BIOCORE_HOST_DEVICE float kv34_m_tau(float V) {
    return KV34_TAU_M_BASE_MS - KV34_TAU_M_AMP_MS / (1.0f + exp(-V / KV34_TAU_M_K_MV));
}

static inline BIOCORE_HOST_DEVICE float kv34_h_tau(float V) {
    float tau = KV34_TAU_H_BASE_MS + KV34_TAU_H_AMP_MS * exp(-V / KV34_TAU_H_K_MV);
    if (tau > KV34_TAU_H_MAX_MS) tau = KV34_TAU_H_MAX_MS;
    return tau;
}

// Kv2.1 (KCNB1) — медленный delayed rectifier (I_Ks), single-spike (Winlove & Roberts 2011/2012).
// Активация m⁴ (V½=0, k=9.4 — Alexander 2017); U-type инактивация (Klemic 1998).
#define KV21_GAMMA_PS          8.0f    // γ при физ. K⁺ (PMC2151531: g_out 5.9–7.9 пС)
#define KV21_VHALF_M_MV        0.0f    // V½ активации (Alexander 2017 IUPHAR)
#define KV21_K_M_MV            9.4f
#define KV21_TAU_M_FAST_MS    15.0f    // τ_m @ деполяр. (Alexander: 15.7 @+60)
#define KV21_TAU_M_SLOW_MS    73.0f    // τ_m @ гиперполяр. (деактивация τ₂, Alexander)
#define KV21_TAU_M_VHALF_MV    0.0f
#define KV21_TAU_M_K_MV       10.0f
// U-type инактивация: левая ветвь −26.5 (Alexander), правая +15 (ОЦЕНКА — Klemic качественно:
// «max инактивация при ~0 мВ, open-state resistant»), residual 0.2 (Gunay 2008 ModelDB 267669 h0).
#define KV21_VHALF_H_LEFT_MV  -26.5f
#define KV21_K_H_LEFT_MV        5.2f
#define KV21_VHALF_H_RIGHT_MV  15.0f  // ОЦЕНКА (нужна точная лит-ра)
#define KV21_K_H_RIGHT_MV      10.0f  // ОЦЕНКА
#define KV21_H_MIN              0.2f   // residual (Gunay h0)
#define KV21_TAU_H_SLOW_MS   4000.0f   // развитие @0 (Klemic τ≈4 с)
#define KV21_TAU_H_FAST_MS    300.0f   // recovery @−140 (Klemic τ≈0.27 с)
#define KV21_TAU_H_VHALF_MV   -80.0f
#define KV21_TAU_H_K_MV       15.0f
#define KV21_TEA_IC50_MM       10.0f   // TEA поро-блок (IUPHAR/BPS pIC50=2)

static inline BIOCORE_HOST_DEVICE float kv21_m_inf(float V) {
    return 1.0f / (1.0f + exp(-(V - KV21_VHALF_M_MV) / KV21_K_M_MV));
}
static inline BIOCORE_HOST_DEVICE float kv21_m_tau(float V) {
    return KV21_TAU_M_FAST_MS + (KV21_TAU_M_SLOW_MS - KV21_TAU_M_FAST_MS)
           / (1.0f + exp((V - KV21_TAU_M_VHALF_MV) / KV21_TAU_M_K_MV));
}
// U-type: h_inf = 1 − (1−h_min)·left·right; left=инактивация (растёт с V), right=восстановление (убывает с V).
static inline BIOCORE_HOST_DEVICE float kv21_h_inf(float V) {
    float left  = 1.0f / (1.0f + exp(-(V - KV21_VHALF_H_LEFT_MV) / KV21_K_H_LEFT_MV));
    float right = 1.0f / (1.0f + exp((V - KV21_VHALF_H_RIGHT_MV) / KV21_K_H_RIGHT_MV));
    return 1.0f - (1.0f - KV21_H_MIN) * left * right;
}
static inline BIOCORE_HOST_DEVICE float kv21_h_tau(float V) {
    return KV21_TAU_H_SLOW_MS + (KV21_TAU_H_FAST_MS - KV21_TAU_H_SLOW_MS)
           / (1.0f + exp((V - KV21_TAU_H_VHALF_MV) / KV21_TAU_H_K_MV));
}

// Cav1.2 (CACNA1C) L-type — раздельная VDI/CDI (полная биология, 2026-09-22).
// VDI (voltage-dependent) — медленная, V½=−37.3; CDI (Ca²⁺-dependent) — быстрая, Kd=1 µM.
#define CAV12_TAU_VDI_MS  150.0f   // быстрая VDI-компонента (Catterall 2005 ~190 мс; native «tens мс»)
#define CAV12_TAU_CDI_MS   35.0f   // CDI 32–41 мс (Ca²⁺-зависимая, кальмодулин)
#define CAV12_CDI_KD_UM     1.0f   // Kd CDI (Ca²⁺/кальмодулин, µM)

// Cav2.3 (CACNA1E) R-type — двухкомпонентная инактивация (полная биология, 2026-09-22).
// fast const ~20–30 мс (Bernatchez 2001, fractional ~0.65 WT) + slow ~90–105 мс (WT).
#define CAV23_TAU_H_FAST_MS   25.0f
#define CAV23_TAU_H_SLOW_MS  100.0f
#define CAV23_FAST_WEIGHT      0.65f  // fractional fast-компонента (WT Cav2.3)

// T-Ca (Cav3.2) — V-зависимая инактивация/де-инактивация (τ_h), как Kv4.2.
// LVA: быстрая инактивация при деполяр. (~10–20 мс) + медленная де-инактивация
// (recovery) при гиперполяр. (~100–300 мс). (Perez-Reyes 2003; Chemin 2002).
#define TCA_TAU_H_FAST_MS   15.0f   // инактивация при деполяр.
#define TCA_TAU_H_SLOW_MS  150.0f   // де-инактивация при гиперполяр.
#define TCA_V_REC_HALF_MV  -65.0f   // переход инакт.↔де-инакт.
#define TCA_K_REC_MV         8.0f

static inline BIOCORE_HOST_DEVICE float tca_h_tau(float V) {
    return TCA_TAU_H_FAST_MS
         + (TCA_TAU_H_SLOW_MS - TCA_TAU_H_FAST_MS)
           / (1.0f + exp((V - TCA_V_REC_HALF_MV) / TCA_K_REC_MV));
}

// Cav3.3 (CACNA1I) — самая медленная T-Ca. Кинетика: Lee 1999 J Neurosci 19:1912 + Klöckner 1999.
#define CAV33_TAU_H_FAST_MS  97.0f   // деполяр. инактивация (Kozlov 1999)
#define CAV33_TAU_H_SLOW_MS 400.0f   // гиперполяр. восстановление (Lee 1999: recovery >3× α1G; компромисс 300–450)
#define CAV33_V_REC_HALF_MV -65.0f
#define CAV33_K_REC_MV        8.0f

static inline BIOCORE_HOST_DEVICE float cav33_h_tau(float V) {
    return CAV33_TAU_H_FAST_MS
         + (CAV33_TAU_H_SLOW_MS - CAV33_TAU_H_FAST_MS)
           / (1.0f + exp((V - CAV33_V_REC_HALF_MV) / CAV33_K_REC_MV));
}

// Cav3.3 τ_m(V) — bell-shape (деактивация быстрая, активация медленная):
// деактивация 1.25 мс @ −100 мВ (Lee 1999, в 6× медленнее α1E), пик 30 мс @ −20 мВ (Klöckner 1999).
#define CAV33_TAU_M_DEACT_MS   1.25f
#define CAV33_TAU_M_PEAK_MS   30.0f
#define CAV33_TAU_M_V_PEAK_MV -20.0f
#define CAV33_TAU_M_SIGMA_MV   22.0f  // 2σ² = 968

static inline BIOCORE_HOST_DEVICE float cav33_m_tau(float V) {
    float dv = V - CAV33_TAU_M_V_PEAK_MV;
    float g = exp(-(dv * dv) / (2.0f * CAV33_TAU_M_SIGMA_MV * CAV33_TAU_M_SIGMA_MV));
    return CAV33_TAU_M_DEACT_MS + (CAV33_TAU_M_PEAK_MS - CAV33_TAU_M_DEACT_MS) * g;
}

// Cav3.3 фармакология (Concise Guide to Pharmacology 2023/24, PMC11339754):
#define CAV33_NI_IC50_UM        200.0f  // Ni²⁺ (µМ; Cav3.3 слабочувствителен, Cav3.2 ~13 µМ)
#define CAV33_MIBEFRADIL_IC50_UM  1.6f  // мибефрадил (µМ, −110 мВ)
#define CAV33_TTAP2_IC50_NM      82.0f  // TTA-P2 (нМ, pIC₅₀ 7.09)
#define CAV33_Z944_IC50_NM      100.0f  // Z944 (нМ, pIC₅₀ 7)

// Cav3.1 (CACNA1G/α1G) — T-type Ca²⁺, быстрее Cav3.3. Кинетика: Perez-Reyes 2003, PMC2483336.
#define CAV31_VHALF_M -45.0f  // активация (α1G, 2 мМ Ba²⁺ −46.5 / human −45.2)
#define CAV31_K_M      5.5f
#define CAV31_VHALF_H -77.0f  // инактивация (human −76.9)
#define CAV31_K_H      6.0f
#define CAV31_GAMMA_PS 7.5f  // single-channel γ (α1G slope conductance, Ca²⁺/Ba²⁺)
#define CAV31_TAU_H_FAST_MS  25.0f   // деполяр. инактивация (быстрее Cav3.3 97мс)
#define CAV31_TAU_H_SLOW_MS 250.0f   // гиперполяр. восстановление
#define CAV31_V_REC_HALF_MV -65.0f
#define CAV31_K_REC_MV        8.0f
static inline BIOCORE_HOST_DEVICE float cav31_h_tau(float V) {
    return CAV31_TAU_H_FAST_MS
         + (CAV31_TAU_H_SLOW_MS - CAV31_TAU_H_FAST_MS)
           / (1.0f + exp((V - CAV31_V_REC_HALF_MV) / CAV31_K_REC_MV));
}
#define CAV31_TAU_M_DEACT_MS   1.25f
#define CAV31_TAU_M_PEAK_MS   15.0f
#define CAV31_TAU_M_V_PEAK_MV -20.0f
#define CAV31_TAU_M_SIGMA_MV   22.0f
static inline BIOCORE_HOST_DEVICE float cav31_m_tau(float V) {
    float dv = V - CAV31_TAU_M_V_PEAK_MV;
    float g = exp(-(dv * dv) / (2.0f * CAV31_TAU_M_SIGMA_MV * CAV31_TAU_M_SIGMA_MV));
    return CAV31_TAU_M_DEACT_MS + (CAV31_TAU_M_PEAK_MS - CAV31_TAU_M_DEACT_MS) * g;
}
// Cav3.1 фармакология: Ni²⁺ 250 µM (отличает от Cav3.2 ~13µM, Cav3.3 ~216µM)
#define CAV31_NI_IC50_UM        250.0f
#define CAV31_MIBEFRADIL_IC50_UM  1.6f
#define CAV31_TTAP2_IC50_NM      82.0f
#define CAV31_Z944_IC50_NM      100.0f

// Cav3.2 (CACNA1H/α1H) — T-type Ca²⁺, самая Ni²⁺-чувствительная. Кинетика: Cribbs 1998 (Circ Res 83:103).
#define CAV32_VHALF_M -46.0f  // активация (α1H, Ba²⁺)
#define CAV32_K_M      6.0f
#define CAV32_VHALF_H -72.0f  // инактивация (α1H; Cav3.2 деполярнее Cav3.1 −77)
#define CAV32_K_H      6.0f
#define CAV32_GAMMA_PS 9.0f  // single-channel γ (α1H, Perez-Reyes 2003)
#define CAV32_TAU_H_FAST_MS  16.0f   // деполяр. инактивация (быстрее Cav3.1 25мс)
#define CAV32_TAU_H_SLOW_MS 180.0f   // гиперполяр. восстановление
#define CAV32_V_REC_HALF_MV -65.0f
#define CAV32_K_REC_MV        8.0f
static inline BIOCORE_HOST_DEVICE float cav32_h_tau(float V) {
    return CAV32_TAU_H_FAST_MS
         + (CAV32_TAU_H_SLOW_MS - CAV32_TAU_H_FAST_MS)
           / (1.0f + exp((V - CAV32_V_REC_HALF_MV) / CAV32_K_REC_MV));
}
#define CAV32_TAU_M_DEACT_MS   1.25f
#define CAV32_TAU_M_PEAK_MS   15.0f
#define CAV32_TAU_M_V_PEAK_MV -20.0f
#define CAV32_TAU_M_SIGMA_MV   22.0f
static inline BIOCORE_HOST_DEVICE float cav32_m_tau(float V) {
    float dv = V - CAV32_TAU_M_V_PEAK_MV;
    float g = exp(-(dv * dv) / (2.0f * CAV32_TAU_M_SIGMA_MV * CAV32_TAU_M_SIGMA_MV));
    return CAV32_TAU_M_DEACT_MS + (CAV32_TAU_M_PEAK_MS - CAV32_TAU_M_DEACT_MS) * g;
}
// Cav3.2 фармакология: Ni²⁺ 13 µM (отличает от Cav3.1 250µM, Cav3.3 ~216µM)
#define CAV32_NI_IC50_UM        13.0f
#define CAV32_MIBEFRADIL_IC50_UM  3.0f
#define CAV32_TTAP2_IC50_NM      82.0f
#define CAV32_Z944_IC50_NM      100.0f

// Пресинаптический микродомен Ca²⁺ (бутон, single-source CPU+SYCL+CUDA).
// #9: аттенюация пресинаптического ПД выводится из ЛОКАЛЬНЫХ каналов бутона (Kv1 реполяризация
// Foust 2011; Na усиление Engel & Jonas 2005) через делитель напряжения — НЕ геометрия.
// V_bouton = (g_axial·V_axon + g_K·E_K + g_Na·E_Na) / (g_axial + g_K + g_Na).
// Каналы бутона читаются из pre_autoreceptors (K: Kv1.1/1.2/Kv_DR/Kv3.1/Kv7/K2P — аттенюация;
// Na: Nav1.6/1.2/HH-Na — усиление). Cav2.1 гейтится V_bouton (m_inf/h_inf квазистатически).

// γ single-channel (пС) локальных каналов бутона — для делителя напряжения.
static inline BIOCORE_HOST_DEVICE float bouton_channel_gamma(uint8_t t) {
    switch (t) {
        case MOD_KV11:                 return 10.0f;    // Kv1.1 (γ 9–14)
        case MOD_KV12:                 return 16.0f;    // Kv1.2 (γ 14–18)
        case MOD_KV_DELAYED_RECTIFIER: return 11.0f;    // Kv1.1 (KCNA1) (γ 9–14)
        case MOD_KV31:                 return 27.0f;    // Kv3.1
        case MOD_KV7_MCURRENT:         return 7.0f;     // Kv7 KCNQ2/3
        case MOD_K2P_LEAK:             return 100.0f;   // TREK-1
        case MOD_AXON_NAV16:           return NAV16_GAMMA_PS;
        case MOD_AXON_NAV12:           return NAV12_GAMMA_PS;
        default:                       return 0.0f;
    }
}
static inline BIOCORE_HOST_DEVICE bool bouton_is_Na_channel(uint8_t t) {
    return t == MOD_AXON_NAV16 || t == MOD_AXON_NAV12;
}

BIOCORE_HOST_DEVICE static inline void bouton_ca_step(const Segment* pre, Synapse* syn, float dt) {
    // Локальные проводимости бутона (pre_autoreceptors): g_K (аттенюация), g_Na (усиление).
    float g_K = 0.0f, g_Na = 0.0f;
    for (uint8_t j = 0; j < syn->pre_autoreceptor_count; j++) {
        uint8_t t = syn->pre_autoreceptors[j].type;
        float g = syn->pre_autoreceptors[j].density * bouton_channel_gamma(t);
        if (bouton_is_Na_channel(t)) g_Na += g;
        else g_K += g;
    }

    // Делитель напряжения: пресинаптический V из баланса локальных проводимостей.
    float V_bouton = pre->V_m;  // по умолчанию — изопотенциален аксону (инвазия)
    if (g_K > 0.0f || g_Na > 0.0f) {
        float E_K  = nernst(pre->intra_ions[ION_K],  pre->extra_ions[ION_K],  1.0f);
        float E_Na = nernst(pre->intra_ions[ION_NA], pre->extra_ions[ION_NA], 1.0f);
        V_bouton = (BOUTON_G_AXIAL_PS * pre->V_m + g_K * E_K + g_Na * E_Na)
                 / (BOUTON_G_AXIAL_PS + g_K + g_Na);
    }

    // Cav2.1 (Ca²⁺-вход): m/h динамически пересчитываются к V_bouton (τ_m=1 мс, τ_h=300 мс).
    float cav21_dens = 0.0f, m = 0.0f, h = 0.0f;
    uint8_t mc = pre->modifier_count;
    if (mc > MAX_MODIFIERS_PER_SEGMENT) mc = 0;
    for (uint8_t j = 0; j < mc; j++)
        if (pre->modifiers[j].type == MOD_CAV21) {
            cav21_dens = pre->modifiers[j].density;
            float m_inf = cav21_m_inf(V_bouton);
            float h_inf = cav21_h_inf(V_bouton, syn->bouton_Ca * MM_TO_UM);
            m = m_inf;  // активация быстрая (τ_m=1 мс) → квазистатически
            h = h_inf + (pre->modifiers[j].state[1] - h_inf) * exp(-dt / CAV21_TAU_H_MS);  // инактивация медленная → динамически
        }
    float cav21_open = m * m * h;   // m²·h

    double P_single = ghk_single_channel_P(CAV21_GAMMA_PS, 2.0f, GHK_CBAR_CA_MM);
    double flux = ghk_divalent(V_bouton, syn->bouton_Ca, pre->extra_ions[ION_CA]);
    double I_A = P_single * flux * (cav21_dens * syn->pre_area) * cav21_open;
    float I_Ca = (float)(I_A * A_TO_PA);
    syn->bouton_Ca -= I_Ca * dt * 1000.0f / (2.0f * FARADAY_CONST * BOUTON_VOLUME_UM3);
    syn->bouton_Ca += (BOUTON_CA_BASELINE_MM - syn->bouton_Ca) * dt / BOUTON_CA_TAU_MS;
    if (syn->bouton_Ca < 0.0f) syn->bouton_Ca = 0.0f;
}

// Постсинаптический шипик (микродомен): Ca²⁺ входит через Ca²⁺-проницаемые рецепторы
// (NMDA/Ca-AMPA), локально (≠ дендритный ствол), спадает медленнее бутона (шейка шипика).
BIOCORE_HOST_DEVICE static inline void spine_ca_step(Synapse* syn, float I_syn, float dt) {
    float I_abs = I_syn < 0.0f ? -I_syn : I_syn;  // |I_syn| (device-совместимо, без fabsf)
    float I_Ca = I_abs * SPINE_CA_FRACTION;  // Ca²⁺-фракция постсинаптического тока (NMDA ~10%)
    float influx = I_Ca * dt * 1000.0f / (2.0f * FARADAY_CONST * SPINE_VOLUME_UM3 * SPINE_CA_BUFFER_CAPACITY);
    syn->spine_Ca += influx;
    syn->spine_Ca += (SPINE_CA_BASELINE_MM - syn->spine_Ca) * dt / SPINE_CA_TAU_MS;
    if (syn->spine_Ca < 0.0f) syn->spine_Ca = 0.0f;
}

// Локальная пластичность шипика: CaMKII (LTP) + кальциневрин (LTD), из spine_Ca.
// Кинетика совпадает с camkii_current/calcineurin_current, но читает Ca²⁺ шипика, не сегмента.
BIOCORE_HOST_DEVICE static inline void spine_plasticity_step(Synapse* syn, float dt) {
    float Ca_uM = syn->spine_Ca * MM_TO_UM;
    // CaMKII: Hill n=4, Kd=1µM, τ=2с
    float Ca4 = Ca_uM * Ca_uM * Ca_uM * Ca_uM;
    float camkii_inf = Ca4 / (Ca4 + 1.0f + 1e-9f);   // Kd⁴ = 1⁴
    syn->spine_camkii += (camkii_inf - syn->spine_camkii) * dt / 2000.0f;
    if (syn->spine_camkii < 0.0f) syn->spine_camkii = 0.0f;
    if (syn->spine_camkii > 1.0f) syn->spine_camkii = 1.0f;
    // Кальциневрин: Hill n=2, Kd=0.05µM, τ=300мс
    float Ca2 = Ca_uM * Ca_uM;
    float calcineurin_inf = Ca2 / (Ca2 + 0.05f * 0.05f + 1e-9f);
    syn->spine_calcineurin += (calcineurin_inf - syn->spine_calcineurin) * dt / 300.0f;
    if (syn->spine_calcineurin < 0.0f) syn->spine_calcineurin = 0.0f;
    if (syn->spine_calcineurin > 1.0f) syn->spine_calcineurin = 1.0f;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Гейтинг ритмогенных GHK-каналов (steady-state Больцман) — ЕДИНЫЙ источник.
// Значения V½/k — из TEST_PLAN_MODIFIERS.md (совр. литература: Nav1.6, Cav3, HCN).
// ═══════════════════════════════════════════════════════════════════════════════

#define NAP_VHALF_M  -58.7f   // NaP (Nav1.6) persistent-активация (Mes V, Wu 2005)
#define NAP_K_M       6.0f
#define NAP_GAMMA_PS  20.7f     // Nav1.6 single-channel γ (пС)
#define NAP_PERSIST_FRAC 0.016f // persistent-фракция (1.6%, гиппокамп, 100 мс)
#define NAP_TAU_M_MS   5.0f     // τ_m активации — фикс (подпороговый NaP слабо V-зависим)

// NaP-tadpole (Nav1.2-persistent, MOD 195) — тадпольный dIN INaP (Svensson 2017, J Neurophysiol
// 118:121, PMC5494356): активация выше −40 мВ, I-V пик ~0 мВ → V½≈−25 (threshold −40 + пик 0).
// НЕ Nav1.6 (Mes V, Wu 2005: V½=−58.7) — dIN экспрессирует Nav1.2 (тадпольный изоформ).
#define NAP_TAD_VHALF_M  -40.0f
#define NAP_TAD_K_M       6.0f
#define NAP_TAD_GAMMA_PS  20.8f    // Nav1.2 single-channel γ (пС, Ben-Shalom 2017: 20.8±1.8, fluct. analysis)
#define NAP_TAD_PERSIST_FRAC 0.016f // persistent-фракция (1.6%)
#define NAP_TAD_TAU_M_MS   5.0f     // τ_m активации — фикс
// Slow inactivation NaP (persistent Na, Fleidervish & Gutnick 1996, Crill 1996):
// ограничивает persistent Na при длительной деполяризации → НЕ держит dIN деполяризованным
// (single-spike). ⚠️ V½_s/τ_s — ОЦЕНКА (уточнить по литературе NaP slow inactivation).
#define NAP_TAD_SLOW_VHALF_MV -50.0f  // slow inactivation V½
#define NAP_TAD_SLOW_K_MV      6.0f
#define NAP_TAD_SLOW_TAU_MS  500.0f   // τ slow inactivation
// τ_m фиксирован (5 мс) — подпороговый NaP (активен −60…−40 мВ) слабо V-зависим в рабочем окне.
// Канал слабо исследован на явные τ_m(V). Возможный (НЕ откалиброванный лабораторно) вариант —
// bell-shaped: τ_m(V) = C + 1/(exp((V−V1)/k1) + exp((V−V2)/k2)), под диапазон τ_m 1–10 мс
// (Magistretti & Alonso 1999). Требует лабораторной сверки перед заменой фикс τ_m.

// ═══════════════════════════════════════════════════════════════════════════════
// Kir2.1 — входящий выпрямитель: Woodhull N-сайтовый блок (Mg²⁺ + spermine)
// f_open = 1/(1 + Σ [B]^k/Π K_i(V));  K_i(V) = K_i(0)·exp(−δ_i·z_i·F·V/(RT)).
// Концентрации в мМ (≡ моль/м³). Источник: Lopatin 1995, Matsuda 1991, Huang 2014.
// ═══════════════════════════════════════════════════════════════════════════════
#define KIR_GAMMA_PS      30.0f     // Kir2.1 single-channel γ (пС)
#define KIR_MG_KD0        0.3f      // Mg²⁺ Kd(0), мМ
#define KIR_MG_DELTA      0.6f      // эл. дистанция Mg²⁺
#define KIR_MG_Z          2.0f      // заряд Mg²⁺
#define KIR_MG_I          0.5f      // [Mg²⁺]i, мМ (физиол.)
#define KIR_SPM_KD0       0.0775f   // spermine Kd(0), мМ (=77.5 µM)
#define KIR_SPM_DELTA     0.5f      // эл. дистанция spermine
#define KIR_SPM_Z         4.0f      // заряд spermine
#define KIR_SPM_I         0.001f    // [spermine]i, мМ (=1 µM)
#define KIR_SPM_SITES     3         // число сайтов spermine (long-pore)

// Фракция незаблокированных каналов (Woodhull N-сайтовый Mg²⁺/spermine блок).
static inline BIOCORE_HOST_DEVICE float kir_f_open(float V) {
    float Vn = FARADAY_CONST * V * 0.001f / (GAS_CONST * CELSIUS_37);  // F·V/(RT)
    float Kd_Mg = KIR_MG_KD0 * exp(-KIR_MG_DELTA * KIR_MG_Z * Vn);
    float mg_block = KIR_MG_I / Kd_Mg;
    float Kd_spm = KIR_SPM_KD0 * exp(-KIR_SPM_DELTA * KIR_SPM_Z * Vn);
    float r = KIR_SPM_I / Kd_spm;
    float spm_block = 0.0f, rk = r;
    for (int k = 0; k < KIR_SPM_SITES; k++) { spm_block += rk; rk *= r; }
    return 1.0f / (1.0f + mg_block + spm_block);
}

// Kir2.2 (KCNJ12) — inward rectifier, γ выше Kir2.1 (36 vs 30 пС), Ba²⁺ чувствительнее (2.8 vs 16.2 µM).
#define KIR22_GAMMA_PS      36.0f     // Kir2.2 single-channel γ (пС)
#define KIR22_BA_IC50_UM    2.8f      // Ba²⁺ IC50 (µM, −120 мВ)

// ═══════════════════════════════════════════════════════════════════════════════
// SK (KCa2.2) — малопроводящий Ca²⁺-активируемый K⁺ (mAHP). Чисто Ca²⁺-гейт (нет V).
// p_inf = Ca⁴/(Ca⁴ + Kd⁴) — Hill 4 (кальмодулин, 4 Ca²⁺-сайта).
// Источник: Hirschberg et al. 1998; семейство γ 4–14 пС, EC50 0.3–0.5 µM.
// ═══════════════════════════════════════════════════════════════════════════════
#define SK_GAMMA_PS     10.0f    // KCa2.2 single-channel γ (пС)
#define SK_KD_MM        0.0004f  // Ca²⁺ EC50 = 400 нМ = 0.4 µM = 0.0004 мМ
#define SK_HILL         4.0f     // Hill (кальмодулин)
#define SK_TAU_OPEN_MS  5.0f     // активация (мс)
#define SK_TAU_CLOSE_MS 50.0f    // деактивация (мс, mAHP decay)

// Мгновенная open-вероятность SK (steady-state Hill, чисто Ca²⁺-зависимая).
static inline BIOCORE_HOST_DEVICE float sk_p_inf(float Ca_mol_m3) {
    if (Ca_mol_m3 < 1e-9f) Ca_mol_m3 = 1e-9f;
    float r = SK_KD_MM / Ca_mol_m3;
    return 1.0f / (1.0f + pow(r, SK_HILL));
}

// KCa3.1 (KCNN4/SK4/IK) — Ca²⁺-активируемый K⁺, voltage-independent. γ=25 пС, EC50 188 нМ (Hill 3.2).
// Источник: Sforna 2018 (28875832).
#define KCA31_GAMMA_PS    25.0f
#define KCA31_KD_MM       0.000188f  // Ca²⁺ EC50 = 188 нМ
#define KCA31_HILL        3.2f
#define KCA31_TAU_OPEN_MS  5.0f
#define KCA31_TAU_CLOSE_MS 50.0f
#define KCA31_TRAM34_IC50_NM 20.0f
static inline BIOCORE_HOST_DEVICE float kca31_p_inf(float Ca_mol_m3) {
    if (Ca_mol_m3 < 1e-9f) Ca_mol_m3 = 1e-9f;
    float r = KCA31_KD_MM / Ca_mol_m3;
    return 1.0f / (1.0f + powf(r, KCA31_HILL));
}

// TRPC3 — неселективный катионный, DAG-активируемый (рецептор-управляемый).
// γ=60 пС (9298988), P_Ca:P_Na=1.62 (10381584), Pyr3 0.7 µM (19289841).
#define TRPC3_GAMMA_PS      60.0f
#define TRPC3_PCA_PNA       1.62f
#define PYR3_IC50_TRPC3     0.7f  // µM

// TRPM7 — chanzyme (канал+киназа), Mg²⁺/Ca²⁺-проницаемый, Mg²⁺-ингибируемый.
// γ=40 пС (11161216), P_Ca/P_Na=0.34 (12508053), Mg²⁺(внутр) 0.7 мМ (16533898), NS8593 1.6 µM (22242975).
#define TRPM7_GAMMA_PS      40.0f
#define TRPM7_PCA_PNA       0.34f
#define TRPM7_MG_IC50_MM    0.7f
#define NS8593_IC50_TRPM7   1.6f  // µM

// ═══════════════════════════════════════════════════════════════════════════════
// BK (KCa1.1) — Horrigan–Aldrich (2002) полный аллостерический механизм.
// 4 вольт-сенсора + 4 Ca²⁺-сайта + воротный переход C⇌O, частичная связь D/C/E.
// Po = L(1+KC+JD+JKCDE)⁴ / [ L(1+KC+JD+JKCDE)⁴ + (1+K+J+JKE)⁴ ],
// где L=L₀e^(z_L FV/RT), J=J₀e^(z_J FV/RT), K=[Ca]/Kc (Kc=44 µM, закрытый канал).
// НЕЙРОНАЛЬНЫЙ комплекс α+β4 (β4 — преобладающий нейрональный β).
// Источник: Wang, Rothberg & Brenner 2006, J. Gen. Physiol. 127:449–465,
//   Table III, best fit «α+β4_a» (chisq=2157).
// ═══════════════════════════════════════════════════════════════════════════════
#define BK_GAMMA_PS   250.0f   // KCa1.1 single-channel γ (пС)
#define BK_TAU_MS     1.0f     // релаксация open_prob (мс) — быстрый fAHP
#define BK_HA_ZJ      0.55f    // заряд вольт-сенсора (e), α+β4
#define BK_HA_ZL      0.30f    // заряд воротного перехода (e)
#define BK_HA_J0      0.104f   // сенсор при V=0 (= exp(−zJ·F·Vhc/(RT)), Vhc=110 мВ)
#define BK_HA_L0      3.7e-8f  // ворота при V=0 (β4 ↓ в 46× к α-alone)
#define BK_HA_KD_MM   0.044f   // Ca²⁺ Kc закрытого канала = 44 µM = 0.044 мМ
#define BK_HA_C       23.0f    // Ca²⁺ ↔ ворота (= Kc/Ko, Ko=1.9 µM)
#define BK_HA_D       32.0f    // сенсор ↔ ворота
#define BK_HA_E       3.7f     // Ca²⁺ ↔ сенсор

// Мгновенная open-вероятность BK (steady-state) по Horrigan–Aldrich.
// V в мВ, Ca в моль/м³ (= мМ). Возвращает Po ∈ [0,1].
static inline BIOCORE_HOST_DEVICE float bk_ha_po(float V_mV, float Ca_mol_m3) {
    float Vn = FARADAY_CONST * V_mV * MV_TO_V / (GAS_CONST * CELSIUS_37);  // F·V/(RT)
    float J = BK_HA_J0 * exp(BK_HA_ZJ * Vn);
    float L = BK_HA_L0 * exp(BK_HA_ZL * Vn);
    if (Ca_mol_m3 < 0.0f) Ca_mol_m3 = 0.0f;
    float K = Ca_mol_m3 / BK_HA_KD_MM;
    float o = 1.0f + K * BK_HA_C + J * BK_HA_D + J * K * BK_HA_C * BK_HA_D * BK_HA_E;
    float c = 1.0f + K + J + J * K * BK_HA_E;
    o *= o; o *= o;  // ^4
    c *= c; c *= c;  // ^4
    float num = L * o;
    return num / (num + c);
}

// ═══════════════════════════════════════════════════════════════════════════════
// KNa (Slack, KCNT1/Slo2.2) — Na⁺-активируемый K⁺ (slow AHP, Ca²⁺-независимый).
// p_inf = Naⁿ/(Naⁿ + EC50ⁿ) — Hill по [Na⁺]in, чисто Na⁺-гейт (Slack без S4, слабо V).
// Источник: Yuan et al. 2003, Neuron 37:765 (Slack=Slo2.2); γ≈180 пС, EC50≈41 мМ, Hill≈3.4.
// ═══════════════════════════════════════════════════════════════════════════════
#define KNA_GAMMA_PS     180.0f   // Slack single-channel γ (пС)
#define KNA_NA_EC50_MM   0.041f   // Na⁺ EC50 = 41 мМ (Yuan 2003)
#define KNA_HILL         3.4f     // Hill по Na⁺ (Yuan 2003)
#define KNA_TAU_MS       800.0f   // медленная активация (slow AHP, сотни мс)

// Мгновенная open-вероятность KNa (steady-state Hill по [Na⁺]in, моль/м³ = мМ).
static inline BIOCORE_HOST_DEVICE float kna_m_inf(float Na_mol_m3) {
    if (Na_mol_m3 < 0.0f) Na_mol_m3 = 0.0f;
    float r = Na_mol_m3 / KNA_NA_EC50_MM;
    return pow(r, KNA_HILL) / (1.0f + pow(r, KNA_HILL));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Kv Delayed Rectifier (Kv1.1/KCNA1) — быстрый voltage-gated K⁺ без инактивации.
// Одномолекулярная GHK: I = P_single·flux·(density·area)·m. m_inf = Boltzmann(V½,k).
// Источник: Stühmer 1989; Gutman 2005; PMC2838721 Table 1. V½=−30 мВ, k≈6 мВ, τ≈2 мс.
// ═══════════════════════════════════════════════════════════════════════════════
#define KDR_VHALF_MV        -30.0f   // V½ активации (Kv1.1)
#define KDR_K_MV             6.0f    // крутизна активации (Kv1.1)
#define KDR_TAU_VHALF_MS    14.6f   // τ активации при V½ (Zerr 1998: human Kv1.1)
#define KDR_TAU_K_MV        30.1f   // slope фактор τ_m(V) (Zerr 1998)
#define KDR_GAMMA_PS        12.0f   // Kv1.x delayed rectifier γ (пС, 9–14)
// τ_m фиксирован (2 мс) — Kv1.1 активация слабо V-зависима в рабочем диапазоне спайка.
// bell-shaped вариант τ_m(V) возможен, но НЕ откалиброван лабораторно (требует сверки).

static inline BIOCORE_HOST_DEVICE float kv_dr_m_inf(float V) {
    return 1.0f / (1.0f + exp(-(V - KDR_VHALF_MV) / KDR_K_MV));
}

// V-зависимая τ активации Kv1.1 (Zerr 1998: τ = τ_V½·exp(−(V−V½)/k), τ_V½=14.6 мс, k=30.1 мВ).
static inline BIOCORE_HOST_DEVICE float kv_dr_m_tau(float V, float V_half) {
    float tau = KDR_TAU_VHALF_MS * exp(-(V - V_half) / KDR_TAU_K_MV);
    if (tau < 0.3f) tau = 0.3f;    // минимум для стабильности (быстрая активация при сильной деполяризации)
    if (tau > 200.0f) tau = 200.0f;
    return tau;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Cl-утечка (CLC-2/ClC-2) — пассивная Cl⁻ проводимость, всегда открыта.
// GHK-анион: I_Cl = P[см/с]·ghk_anion(V, Cl_i, Cl_o)·area, P из γ (как K2P/NALCN).
// γ(CLC-2) ≈ 2–3 пС (малая, как HCN); g ≈ 1–5 мС/см² (фоновая Cl⁻ утечка).
// ═══════════════════════════════════════════════════════════════════════════════
#define CLC2_GAMMA_PS       2.5f     // γ CLC-2/ClC-2 (2–3 пС, структурная оценка)
#define OTOP1_GAMMA_PS      20.0f    // γ OTOP1 протонного канала (пС, placeholder — unitary не измерена, Tu 2018)
#define TCA_VHALF_M  -48.4f   // T-Ca (Cav3.2) m-активация
#define TCA_K_M       5.0f
#define TCA_VHALF_H  -75.6f   // T-Ca (Cav3.2) h-инактивация
#define TCA_K_H       6.5f
#define TCA_GAMMA_PS  7.2f    // Cav3.2 single-channel γ (пС)
#define CAV33_VHALF_M -43.8f  // Cav3.3 m-активация (самый положительный из T-Ca)
#define CAV33_K_M      6.9f   // Klöckner 1999
#define CAV33_VHALF_H -72.5f  // Cav3.3 h-инактивация
#define CAV33_K_H      6.6f   // Klöckner 1999
#define CAV33_GAMMA_PS 11.0f  // Cav3.3 single-channel γ (пС, Perez-Reyes 2003)
#define NAV13_GAMMA_PS   20.8f  // Nav1.3 γ≈20.8 пС (оценка-аналогия Nav1.2, Cummins 2001)
#define NAV17_GAMMA_PS   19.5f  // Nav1.7 γ (пС, Channelpedia ref 2271)
#define NAV18_GAMMA_PS   20.0f  // Nav1.8 γ не определена — аналогия Nav (Channelpedia)
#define NAV19_GAMMA_PS   16.0f  // Nav1.9 γ≈16 пС (preliminary single-channel, человек)
#define KIR71_GAMMA_PS    0.05f // Kir7.1 (Krapivinsky 1998 Neuron)
#define TRPC67_GAMMA_PS  35.0f  // TRPC6/7 объединён (Perez-Leighton 2011)
#define CALHM13_GAMMA_PS 25.0f  // CALHM1 (Ma 2012 PNAS)
#define ENAC_GAMMA_PS     4.5f  // ENaC (Kellenberger 2002)
#define ENAC_PO           0.5f  // ENaC конститутивный open probability
#define OTOP1_ZN_IC50_UM 190.0f // Zn²⁺ IC50 0.19 мМ (Tu 2018)

// Родопсин фотокаскад (род, MOD_ROD_PHOTOTRANSDUCTION 192):
#define RHODOPSIN_TAU_R_MS    1500.0f  // τ_R* — время жизни активир. родопсина (Whitlock & Lamb 1999)
#define RHODOPSIN_TAU_GT_MS     10.0f  // Gt (трансдуцин) активация ~130/с (Leskov 2000)
#define RHODOPSIN_TAU_PDE_MS   200.0f  // τ_PDE* (Leskov 2000)
#define RHODOPSIN_CNG_KHALF      0.3f  // CNG K1/2 норм. (20 µM / ~70 µM, Yau & Nakatani 1985)
#define RHODOPSIN_TAU_CGMP_SYN 500.0f  // GC синтез cGMP (восстановление)
#define RHODOPSIN_TAU_CGMP_HYD  50.0f  // PDE гидролиз cGMP (быстрее синтеза)
#define HCN_VHALF    -82.0f   // HCN (Ih) — гиперполяр.-активация (PIR_HCN, тип 97)
#define HCN_K         6.0f

// ═══════════════════════════════════════════════════════════════════════════════
// HCN изоформы 1-4 (129-132) — Ih: гиперполяр.-активируемый смешанный Na⁺/K⁺.
// γ subunit-specific: HCN1=0.84, HCN2=1.54, HCN3=0.54, HCN4=0.51 пС (Benndorf 2025 PNAS). P_Na/P_K ≈ 0.25.
// V½ и τ_open per-изоформа (Stieber 2005; Biel 2009, Physiol Rev 89:847).
// cAMP-модуляция (сдвиг V½ +10/+20 мВ HCN2/HCN4) НЕ моделируется — суррогат DA в коде.
// ═══════════════════════════════════════════════════════════════════════════════
#define HCN1_GAMMA_PS     0.84f    // γ (пС) — Benndorf 2025 PNAS (subunit-specific)
#define HCN2_GAMMA_PS     1.54f
#define HCN3_GAMMA_PS     0.54f
#define HCN4_GAMMA_PS     0.51f
#define HCN_P_NA_FRAC      0.2f     // P_Na (доля) — P_Na:P_K = 1:4
#define HCN_P_K_FRAC       0.8f     // P_K (доля)
#define HCN_K_MV           6.0f     // крутизна активации
#define HCN_TAU_CLOSE_MS   300.0f    // деактивация (деполяризация) — нативный Ih медленнее HCN1(30мс), даёт overshoot
#define HCN1_VHALF_MV     -69.0f    // литературный HCN1 V½ (честный): steady-деполяризация через cAMP (5-HT7→Gs→cAMP→V½ shift), не V½-хак
#define HCN2_VHALF_MV     -95.0f
#define HCN3_VHALF_MV     -77.0f
#define HCN4_VHALF_MV    -100.0f    // HCN4 — самый гиперполяризованный
#define HCN1_TAU_OPEN_MS   84.0f    // быстрый (τ при V½)
#define HCN2_TAU_OPEN_MS  557.0f
#define HCN3_TAU_OPEN_MS 1408.0f
#define HCN4_TAU_OPEN_MS 3586.0f    // самый медленный
// V-зависимость τ_open: τ(V) = τ_open·exp((V−V½)/K) при гиперполяр. — быстрее при сильной
// гиперполяризации (реальный HCN1, Stieber 2005/Biel 2009; фикс. 84 мс был залипухой — ступенька).
#define HCN_TAU_OPEN_K_MV  15.0f

// Мгновенная open-вероятность HCN (steady-state Больцман, гиперполяр.-активация).
static inline BIOCORE_HOST_DEVICE float hcn_m_inf(float V, float V_half, float k) {
    return 1.0f / (1.0f + exp((V - V_half) / k));
}

// Больцман: активация деполяризацией / инактивация / гиперполяр. активация
static inline BIOCORE_HOST_DEVICE float boltzmann_act(float V, float V_half, float k) {
    return 1.0f / (1.0f + exp(-(V - V_half) / k));
}
static inline BIOCORE_HOST_DEVICE float boltzmann_inact(float V, float V_half, float k) {
    return 1.0f / (1.0f + exp((V - V_half) / k));
}

static inline BIOCORE_HOST_DEVICE float nap_m_inf(float V) {
    return boltzmann_act(V, NAP_VHALF_M, NAP_K_M);
}
static inline BIOCORE_HOST_DEVICE float nap_tad_m_inf(float V) {
    return boltzmann_act(V, NAP_TAD_VHALF_M, NAP_TAD_K_M);
}
static inline BIOCORE_HOST_DEVICE float tca_m_inf(float V) {
    return boltzmann_act(V, TCA_VHALF_M, TCA_K_M);
}
static inline BIOCORE_HOST_DEVICE float tca_h_inf(float V) {
    return boltzmann_inact(V, TCA_VHALF_H, TCA_K_H);
}
static inline BIOCORE_HOST_DEVICE float hcn_m_inf(float V) {
    return 1.0f / (1.0f + exp((V - HCN_VHALF) / HCN_K));  // гиперполяр.-актив.
}

// ═══════════════════════════════════════════════════════════════════════════════
// ЗАЩИТА КОНЦЕНТРАЦИЙ
// ═══════════════════════════════════════════════════════════════════════════════

static inline BIOCORE_HOST_DEVICE void clamp_ions(double* arr, int count) {
    for (int i = 0; i < count; i++) {
        if (arr[i] < 1e-9) arr[i] = 1e-9;  // 1 нМ — позволяет физиол. Ca_i=100нМ
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Na/K-АТФаза: цикл Поста–Альберса (Smith & Crampin 2004, Läuger 1991, Heyse 1994)
// ═══════════════════════════════════════════════════════════════════════════════
//
// 4-стадийный кинетический цикл: E1·ATP → E1P(Na₃) → E2P → E2(K₂) → E1.
// Единственный электрогенный (V-зависимый) шаг — конформационный переход E1P ⇌ E2P,
// сопряжённый с высвобождением 3 Na⁺ наружу (перенос заряда z·δ).
// Стехиометрия 3 Na⁺ наружу : 2 K⁺ внутрь : 1 ATP возникает ЕСТЕСТВЕННО из цикла.
//
// Возвращает нормированный оборот v_norm ∈ [0,1] (доля от максимальной скорости цикла).
// Зависимость: растёт при деполяризации (электрогенность), насыщается по Na_i (Hill-3),
// K_o (Hill-2) и ATP (Michaelis-Menten). Детерминированный steady-state (mean-field).
static inline BIOCORE_HOST_DEVICE float nak_pump_vnorm(float V_mV, float Na_i_mM,
                                                        float K_o_mM, float ATP_level,
                                                        float K_m_Na = NA_K_PUMP_K_NA_MM,
                                                        float K_m_K = NA_K_PUMP_K_K_MM) {
    // Сродство по числу сайтов: Na⁺ Hill-3 (три сайта), K⁺ Hill-2 (два сайта).
    float Na3 = Na_i_mM * Na_i_mM * Na_i_mM;
    float K2  = K_o_mM * K_o_mM;
    float K_Na3 = K_m_Na * K_m_Na * K_m_Na;
    float K_K2  = K_m_K * K_m_K;
    float f_Na = Na3 / (Na3 + K_Na3);
    float f_K  = K2  / (K2  + K_K2);

    // ATP: концентрация в мМ (свойство клетки). Michaelis-Menten, K_m_ATP в мМ.
    float atp = ATP_level;
    if (atp < 0.0f) atp = 0.0f;
    float f_ATP = atp / (atp + NA_K_PUMP_K_ATP_MM);

    // Электрогенный шаг E1P ⇌ E2P: a2(V) прямо, b2(V) обратно.
    float eta = NA_K_PUMP_Z_NET * FARADAY_CONST * (V_mV * MV_TO_V) / (GAS_CONST * CELSIUS_37);
    float a2 = NA_K_PUMP_K2_E1PE2P * exp(NA_K_PUMP_DELTA * eta);
    float b2 = NA_K_PUMP_K2M_E2PE1P * exp(-(1.0f - NA_K_PUMP_DELTA) * eta);

    // Остальные шаги (необратимы в физиологическом диапазоне).
    float a1 = NA_K_PUMP_K1_PHOSPH * f_Na * f_ATP;   // E1 → E1P (фосфорилирование)
    float a3 = NA_K_PUMP_K3_DEPHOSPH * f_K;          // E2P → E2 (дефосфорилирование)
    float a4 = NA_K_PUMP_K4_E2E1;                    // E2 → E1 (высвобождение K⁺)

    // Стационарный поток цикла (решение 4-стадийной схемы Кинга–Альтмана).
    float denom = a3 / a1 + (b2 + a3) / a2 + 1.0f + a3 / a4;
    float turnover = a3 / denom;   // s⁻¹

    // Нормировка на максимальный оборот (насыщение субстратов + V → +∞).
    float k3 = NA_K_PUMP_K3_DEPHOSPH;
    float v_max = k3 / (k3 / NA_K_PUMP_K1_PHOSPH + 1.0f + k3 / NA_K_PUMP_K4_E2E1);
    return turnover / v_max;
}

// ═══════════════════════════════════════════════════════════════════════════════
// NCX (Na⁺/Ca²⁺ обменник): термодинамически согласованный реверсивный поток
// (Weber et al. 2001, Shannon et al. 2004). Стехиометрия 3 Na⁺ : 1 Ca²⁺.
// ═══════════════════════════════════════════════════════════════════════════════
//
// Положительный flux = forward (3 Na внутрь, 1 Ca наружу, нетто +1 внутрь),
// отрицательный = reverse. Знак меняется при E_NaCa (без клампа — полная обратимость).
// Аллостерическая активация Ca²⁺ — Hill-2 с K_mCaAct = 125 нМ.
// Возвращает нормированный поток (≈1 при V=0 и физиол. концентрациях — якорь I_max).
static inline BIOCORE_HOST_DEVICE float ncx_flux_norm(float V_mV, float Na_i, float Na_o,
                                                       float Ca_i, float Ca_o) {
    float V_v = V_mV * MV_TO_V;
    float FRT = FARADAY_CONST / (GAS_CONST * CELSIUS_37);   // F/(RT)
    float e_eta  = exp(NCX_ETA * FRT * V_v);               // e^(η·F·V/RT)
    float e_eta1 = exp((NCX_ETA - 1.0f) * FRT * V_v);      // e^((η−1)·F·V/RT)

    float Na_i3 = Na_i * Na_i * Na_i;
    float Na_o3 = Na_o * Na_o * Na_o;

    // forward (3 Na внутрь, 1 Ca наружу): Na_o³·Ca_i, фактор e^((η−1)·FV/RT) — растёт при гиперполяризации.
    // reverse (3 Na наружу, 1 Ca внутрь): Na_i³·Ca_o, фактор e^(η·FV/RT) — растёт при деполяризации.
    float fwd = e_eta1 * Na_o3 * Ca_i;
    float rev = e_eta  * Na_i3 * Ca_o;
    float denom = 1.0f + NCX_K_SAT * e_eta1;

    float ca_ratio = NCX_K_M_CA_ACT_MM / Ca_i;
    float Allo = 1.0f / (1.0f + ca_ratio * ca_ratio);   // Hill-2

    return Allo * (fwd - rev) / (denom * NCX_REF_FLUX_MM4);
}

// ═══════════════════════════════════════════════════════════════════════════════
// ИОНОТРОПНЫЙ РЕЦЕПТОР (GHK-версия) — единая реализация
// ═══════════════════════════════════════════════════════════════════════════════
//
// Открывает неселективный катионный канал. Соотношение проницаемостей P_Na:P_K
// определяет E_rev ЕСТЕСТВЕННО (без хардкода). Ионы двигаются напрямую через GHK.
//
// state[]: [0]=r (receptor activation 0..1)
// Параметрические переопределения (state[active_vars+0..2]):
//   state[active_vars+0] = P_max, state[active_vars+1] = EC50, state[active_vars+2] = tau_rise
static inline BIOCORE_HOST_DEVICE double ionotropic_ghk_current(Modifier* m, Segment* s, float dt,
    float gamma_ps, float EC50, float tau_rise, float tau_decay, int nt_index,
    float P_Na_ratio, float P_K_ratio, float P_Ca_ratio = 0.0f, bool is_anion = false,
    float tau_desens = 0.0f, float K_desens = 0.0f, float mod_scale = 1.0f)
{
    float area = effective_area(m, s);
    double V = s->V_m;
    double vol = segment_volume(s);
    double cpm = charge_per_mole(vol);

    float p_gamma_ps = (m->active_vars > 0 && m->state[m->active_vars] > 0.0f) ? m->state[m->active_vars] : gamma_ps;

    float nt = s->neurotransmitter[nt_index];
    // Активация r (быстрая): открытие канала, спад EPSC.
    float r_inf = nt / (nt + EC50);
    float r = m->state[0];
    float tau = (r_inf > r) ? tau_rise : tau_decay;
    m->state[0] += (r_inf - r) * dt / tau;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    // Десенситизация d (медленная): при длительном агонисте канал закрывается.
    float open_prob = m->state[0];
    if (tau_desens > 0.0f) {
        float kd = (K_desens > 0.0f) ? K_desens : EC50;
        float d = m->state[1];
        float d_inf = nt / (nt + kd);
        d += (d_inf - d) * dt / tau_desens;
        if (d < 0.0f) d = 0.0f;
        if (d > 1.0f) d = 1.0f;
        m->state[1] = d;
        open_prob *= (1.0f - d);
    }

    double N = m->density * area;
    double I_total = 0.0;

    if (is_anion) {
        double P_Cl = ghk_single_channel_P(p_gamma_ps, 1.0f, GHK_CBAR_CL_MM);
        double flux_Cl = ghk_anion(V, s->intra_ions[ION_CL], s->extra_ions[ION_CL]);
        double I_Cl = P_Cl * flux_Cl * N * open_prob * mod_scale * A_TO_PA;
        s->intra_ions[ION_CL] += (I_Cl * dt) / cpm;
        clamp_ions(s->intra_ions, ION_COUNT);
        return -I_Cl;
    } else {
        double P_Na, P_K, P_Ca;
        ghk_partial_P(p_gamma_ps, P_Na_ratio, P_K_ratio, P_Ca_ratio, &P_Na, &P_K, &P_Ca);
        double I_Na = P_Na * ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]) * N * open_prob * mod_scale * A_TO_PA;
        double I_K  = P_K  * ghk_monovalent(V, s->intra_ions[ION_K],  s->extra_ions[ION_K])  * N * open_prob * mod_scale * A_TO_PA;
        I_total = I_Na + I_K;

        s->intra_ions[ION_NA] -= (I_Na * dt) / cpm;
        s->intra_ions[ION_K]  -= (I_K  * dt) / cpm;

        if (P_Ca_ratio > 0.0f) {
            double I_Ca = P_Ca * ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]) * N * open_prob * mod_scale * A_TO_PA;
            I_total += I_Ca;
            s->intra_ions[ION_CA] -= (I_Ca * dt) / (2.0f * cpm);
        }

        clamp_ions(s->intra_ions, ION_COUNT);
        return -I_total;
    }
}

// NMDA CDI (Ca²⁺/кальмодулин-инактивация, единая для GluN2A/GluN2B) + глицин-коагонист (насыщен).
// state[2] — гейт CDI. Коагонист — константа (глицин/D-серин насыщен в синапсе, фактор ≈0.91).
// Единый источник для fallback (receptors_biological.cpp) и плагина (mod_09_nmda.cpp).
static inline BIOCORE_HOST_DEVICE double nmda_cdi_coagonist(Modifier* m, Segment* s, float dt) {
    double Ca_uM = s->intra_ions[ION_CA] * MM_TO_UM;
    double cdi_inf = 1.0 - NMDA_CDI_MAX * Ca_uM / (Ca_uM + NMDA_CDI_KD_UM);
    float tau = (cdi_inf < m->state[2]) ? NMDA_CDI_TAU_ONSET_MS : NMDA_CDI_TAU_RECOVERY_MS;
    m->state[2] += (cdi_inf - m->state[2]) * dt / tau;
    if (m->state[2] < 0.0f) m->state[2] = 0.0f;
    if (m->state[2] > 1.0f) m->state[2] = 1.0f;
    float coagonist = NMDA_COAG_GLYCINE_UM / (NMDA_COAG_GLYCINE_UM + NMDA_COAG_EC50_UM);
    return m->state[2] * coagonist;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Ca²⁺-АКТИВИРУЕМЫЙ НЕСЕЛЕКТИВНЫЙ КАТИОННЫЙ КАНАЛ (TRPM4) — единая реализация
// ═══════════════════════════════════════════════════════════════════════════════
// TRPM4 (Ullrich 2005, Launay 2002): активируется внутриклеточным Ca²⁺, моновалент-
// селективный (Na⁺/K⁺), НЕ Ca²⁺-проницаем. Гейт Hill([Ca²⁺]ᵢ); ток GHK Na/K (E_rev≈0).
static inline BIOCORE_HOST_DEVICE double trpm4_cation_current(Modifier* m, Segment* s, float dt,
    float EC50_ca_um, float hill, float tau_ms)
{
    float area = effective_area(m, s);
    double V = s->V_m;
    double vol = segment_volume(s);
    double cpm = charge_per_mole(vol);

    double ca_um = s->intra_ions[ION_CA] * MM_TO_UM;
    if (ca_um < 0.0) ca_um = 0.0;
    double ca_n = pow(ca_um, hill);
    double ec50_n = pow((double)EC50_ca_um, hill);
    double m_inf = ca_n / (ca_n + ec50_n);
    float gate = m->state[0];
    gate += (float)((m_inf - gate) * dt / tau_ms);
    if (gate < 0.0f) gate = 0.0f;
    if (gate > 1.0f) gate = 1.0f;
    m->state[0] = gate;

    double P_Na, P_K, P_Ca;
    ghk_partial_P(TRPM4_GAMMA_PS, TRPM4_P_NA_RATIO, TRPM4_P_K_RATIO, 0.0f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double I_Na = P_Na * ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]) * N * gate * A_TO_PA;
    double I_K  = P_K  * ghk_monovalent(V, s->intra_ions[ION_K],  s->extra_ions[ION_K])  * N * gate * A_TO_PA;
    double I_total = I_Na + I_K;

    s->intra_ions[ION_NA] -= (I_Na * dt) / cpm;
    s->intra_ions[ION_K]  -= (I_K  * dt) / cpm;
    clamp_ions(s->intra_ions, ION_COUNT);
    return -I_total;
}

// ═══════════════════════════════════════════════════════════════════════════════
// МЕТАБОТРОПНЫЙ Gi/o (GHK-версия) — GIRK K⁺-канал, единая реализация
// ═══════════════════════════════════════════════════════════════════════════════
static inline BIOCORE_HOST_DEVICE double gi_o_ghk_effect(Modifier* m, Segment* s, float dt, int nt_index) {
    // ✅ ДОСТОВЕРНО — Gi/o → Gβγ → GIRK (Kir3.x): инвард-ректификатор, γ≈35пС, двухфазная G-белковая
    // кинетика (активация/деактивация) + RGS-зависимая десенситизация.
    float area = effective_area(m, s);
    double V = s->V_m;
    double vol = segment_volume(s);
    double cpm = charge_per_mole(vol);

    float p_EC50 = (m->state[m->active_vars] > 0.0f) ? m->state[m->active_vars] : 1.0f;
    float p_gamma_ps = (m->state[m->active_vars + 1] > 0.0f) ? m->state[m->active_vars + 1] : GIRK_GAMMA_PS;

    float nt = s->neurotransmitter[nt_index];
    float activation = nt / (nt + p_EC50);

    // Двухфазная G-белковая кинетика: активация (быстрая) / деактивация (медленная, GTP-гидролиз).
    float r = m->state[0];
    float tau_eff = (activation > r) ? GIRK_TAU_ACT_MS : GIRK_TAU_DEACT_MS;
    r += (activation - r) * dt / tau_eff;
    if (r < 0.0f) r = 0.0f; if (r > 1.0f) r = 1.0f;
    m->state[0] = r;

    // RGS-зависимая десенситизация (state[1]): ~50-60% за 30-60с.
    float d = m->state[1];
    d += (activation - d) * dt / GIRK_TAU_DESENS_MS;
    if (d < 0.0f) d = 0.0f; if (d > 1.0f) d = 1.0f;
    m->state[1] = d;

    // GIRK = Kir3: инвард-ректификация (Woodhull), одномолекулярная P_single, open = r·(1−depth·d).
    float f_open = kir_f_open(V) * r * (1.0f - GIRK_DESENS_DEPTH * d);
    double P_single = ghk_single_channel_P(p_gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double I_A = P_single * flux * (m->density * area) * f_open;
    double I_K = I_A * A_TO_PA;

    s->intra_ions[ION_K] -= (I_K * dt) / cpm;
    clamp_ions(s->intra_ions, ION_COUNT);

    return -I_K;
}

// ═══════════════════════════════════════════════════════════════════════════════
// МЕТАБОТРОПНЫЙ Gs — рецепторная оккупация + десенситизация (СИГНАЛЬНЫЙ, без тока)
// ✅ ДОСТОВЕРНО — Gs НЕ закрывает K⁺-каналы: Gαs → AC → cAMP → PKA. Пулом владеет
// MOD_CAMP_PKA (общий на сегмент). Здесь только рецепторная динамика:
// state[0]=Gαs-активация (0-1), state[1]=десенситизация (GRK/β-аррестин). return 0.
// EC50 — относительный (шкала NT 0-1), задокументировано до задачи #20 (NT→µM).
// ═══════════════════════════════════════════════════════════════════════════════
static inline BIOCORE_HOST_DEVICE float gs_camp_signal(Modifier* m, Segment* s, float dt, int nt_index) {
    float p_EC50 = (m->state[m->active_vars] > 0.0f) ? m->state[m->active_vars] : 1.0f;
    float p_tau_desens = (m->state[m->active_vars + 1] > 0.0f) ? m->state[m->active_vars + 1] : GS_TAU_DESENS_MS;

    // Рецепторная оккупация агонистом (относительная шкала NT)
    float nt = s->neurotransmitter[nt_index];
    float occupancy = nt / (nt + p_EC50);

    // Gαs-активация (state[0]): быстрая G-белковая кинетика (GDP→GTP / GTP-гидролиз)
    float act = m->state[0];
    float tau_act = (occupancy > act) ? GS_TAU_ACT_MS : GS_TAU_DEACT_MS;
    act += (occupancy - act) * dt / tau_act;
    if (act < 0.0f) act = 0.0f; if (act > 1.0f) act = 1.0f;
    m->state[0] = act;

    // Десенситизация (state[1]): GRK фосфорилирует активированный GPCR → β-аррестин
    float d = m->state[1];
    d += (occupancy - d) * dt / p_tau_desens;
    if (d < 0.0f) d = 0.0f; if (d > 1.0f) d = 1.0f;
    m->state[1] = d;

    return 0.0f;  // сигнальный модификатор: ток не генерирует
}

// ── Кросс-модуляция: чтение общего пула cAMP/PKA (MOD_CAMP_PKA) на сегменте ──
static inline BIOCORE_HOST_DEVICE bool is_gs_receptor_type(uint8_t type) {
    return (type == MOD_NT_RECEPTOR_5HT4 || type == MOD_NT_RECEPTOR_5HT6 || type == MOD_NT_RECEPTOR_5HT7 ||
            type == MOD_NT_RECEPTOR_D1 || type == MOD_NT_RECEPTOR_D5 ||
            type == MOD_NT_RECEPTOR_A2A || type == MOD_NT_RECEPTOR_A2B ||
            type == MOD_NT_RECEPTOR_BETA1 || type == MOD_NT_RECEPTOR_BETA2 ||
            type == MOD_NT_RECEPTOR_H2 || type == MOD_NT_RECEPTOR_TAAR1);
}
static inline BIOCORE_HOST_DEVICE bool is_gi_o_receptor_type(uint8_t type) {
    return (type == MOD_NT_RECEPTOR_GABAB ||
            type == MOD_NT_RECEPTOR_5HT1A || type == MOD_NT_RECEPTOR_5HT1B ||
            type == MOD_NT_RECEPTOR_5HT1D || type == MOD_NT_RECEPTOR_5HT1F ||
            type == MOD_NT_RECEPTOR_D2 || type == MOD_NT_RECEPTOR_D3 || type == MOD_NT_RECEPTOR_D4 ||
            type == MOD_NT_RECEPTOR_M2 || type == MOD_NT_RECEPTOR_M4 ||
            type == MOD_NT_RECEPTOR_A1 || type == MOD_NT_RECEPTOR_A3 ||
            type == MOD_NT_RECEPTOR_MGLUR2 || type == MOD_NT_RECEPTOR_MGLUR3 ||
            type == MOD_NT_RECEPTOR_MGLUR4 || type == MOD_NT_RECEPTOR_MGLUR6 ||
            type == MOD_NT_RECEPTOR_MGLUR7 || type == MOD_NT_RECEPTOR_MGLUR8 ||
            type == MOD_NT_RECEPTOR_ALPHA2 ||
            type == MOD_NT_RECEPTOR_CB1 || type == MOD_NT_RECEPTOR_CB2 ||
            type == MOD_NT_RECEPTOR_H3 ||
            type == MOD_NT_RECEPTOR_MT1 || type == MOD_NT_RECEPTOR_MT2);
}
static inline BIOCORE_HOST_DEVICE bool is_gq_receptor_type(uint8_t type) {
    return (type == MOD_NT_RECEPTOR_5HT2A || type == MOD_NT_RECEPTOR_5HT2B || type == MOD_NT_RECEPTOR_5HT2C ||
            type == MOD_NT_RECEPTOR_ALPHA1A || type == MOD_NT_RECEPTOR_ALPHA1B ||
            type == MOD_NT_RECEPTOR_M1 || type == MOD_NT_RECEPTOR_M3 || type == MOD_NT_RECEPTOR_M5 ||
            type == MOD_NT_RECEPTOR_MGLUR1 || type == MOD_NT_RECEPTOR_MGLUR5 ||
            type == MOD_NT_RECEPTOR_H1 || type == MOD_MRGPR);
}
static inline BIOCORE_HOST_DEVICE float segment_camp_um(const Segment* s) {
    for (uint8_t j = 0; j < s->modifier_count; j++)
        if (s->modifiers[j].type == MOD_CAMP_PKA) return s->modifiers[j].state[0];
    return CAMP_BASAL_UM;  // нет каскада → базальный
}

// ── HCN (Ih) — ЕДИНАЯ формула для изоформ 129-132 (fallback + плагины) ──
// Гиперполяр.-активируемый смешанный Na⁺/K⁺ (γ≈1 пС, P_Na:P_K=1:4).
// Честная cAMP-модуляция (cAMP → CNBD → сдвиг V½, насыщение) + V-зависимый τ.
static inline BIOCORE_HOST_DEVICE double hcn_generic_current(Modifier* m, Segment* s, float dt,
                                                            float default_vhalf, float default_tau_open,
                                                            float default_gamma) {
    double V = s->V_m; float area = effective_area(m, s);
    float V_half  = (m->state[1]!=0)?m->state[1]:default_vhalf;
    float k       = (m->state[2]>0)?m->state[2]:HCN_K_MV;
    float gamma_ps = (m->state[3]>0)?m->state[3]:default_gamma;  // γ (пС)
    float tau_open  = (m->state[4]>0)?m->state[4]:default_tau_open;
    float tau_close = (m->state[5]>0)?m->state[5]:HCN_TAU_CLOSE_MS;
    // ✅ ДОСТОВЕРНО — HCN (Ih); менять ТОЛЬКО с согласования (алгоритм аудита модификаторов).

    // Честная cAMP-модуляция: cAMP (из MOD_CAMP_PKA) связывает CNBD → сдвиг V½ (насыщение).
    // DA-суррогат убран — дофамин действует через D1/D5→Gs→AC→cAMP (полный каскад).
    float camp = segment_camp_um(s);
    float Ve = V_half + HCN_CAMP_VHALF_SHIFT_MV * camp / (camp + HCN_CAMP_EC50_UM);
    float mv = m->state[0];
    float mi = hcn_m_inf(V, Ve, k);
    // V-зависимый τ: при гиперполяр. τ_open ускоряется (реальный HCN1), при деполяр. τ_close.
    float tau_eff = (V < Ve) ? (tau_open * exp((V - Ve) / HCN_TAU_OPEN_K_MV)) : tau_close;
    mv += (mi - mv) * dt / tau_eff;
    if (mv<0.0f) mv=0.0f; if (mv>1.0f) mv=1.0f; m->state[0]=mv;

    // Одномолекулярная GHK: γ разложена на парциальные P (P_Na:P_K = 1:4).
    double P_Na, P_K, P_Ca;
    ghk_partial_P(gamma_ps, HCN_P_NA_FRAC, HCN_P_K_FRAC, 0.0f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double flux_K = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double In = P_Na * flux_Na * N * mv * A_TO_PA;
    double Ik = P_K * flux_K * N * mv * A_TO_PA;

    // Канонический объём + задокументированный /10 (внеклетка ~10× внутриклетки).
    double vol = segment_volume(s);
    double cpm = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_NA] -= (In * dt) / cpm;
    s->intra_ions[ION_K]  -= (Ik * dt) / cpm;
    s->extra_ions[ION_NA] += (In * dt) / (cpm * EXTRA_VOLUME_RATIO);
    s->extra_ions[ION_K]  += (Ik * dt) / (cpm * EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT);
    clamp_ions(s->extra_ions, ION_COUNT);
    // Нетто Ih = Na⁺ внутрь + K⁺ наружу (сумма, НЕ разность — знак K исправлен).
    return -(In + Ik);
}

static inline BIOCORE_HOST_DEVICE float segment_pka_act(const Segment* s) {
    for (uint8_t j = 0; j < s->modifier_count; j++)
        if (s->modifiers[j].type == MOD_CAMP_PKA) return s->modifiers[j].state[1];
    return 0.0f;  // нет каскада → PKA неактивна
}
static inline BIOCORE_HOST_DEVICE float segment_ip3_mm(const Segment* s) {
    for (uint8_t j = 0; j < s->modifier_count; j++)
        if (s->modifiers[j].type == MOD_PLC_IP3_DAG) return s->modifiers[j].state[0];
    return 0.0f;  // нет каскада → IP3=0
}
static inline BIOCORE_HOST_DEVICE float segment_dag(const Segment* s) {
    for (uint8_t j = 0; j < s->modifier_count; j++)
        if (s->modifiers[j].type == MOD_PLC_IP3_DAG) return s->modifiers[j].state[1];
    return 0.0f;  // нет каскада → DAG=0
}
static inline BIOCORE_HOST_DEVICE float segment_pkc_act(const Segment* s) {
    for (uint8_t j = 0; j < s->modifier_count; j++)
        if (s->modifiers[j].type == MOD_PKC) return s->modifiers[j].state[0];
    return 0.0f;  // нет PKC → неактивна
}

// ── Шаг каскада Gs/Gi → AC → cAMP → PKA (единая реализация для fallback и плагина) ──
// Общий пул cAMP/PKA на сегмент. Gs стимулирует AC, Gi/o ингибирует (Gαi).
// AC: ATP→cAMP (Михаэлис-Ментен по ATP); PDE: cAMP→5'-AMP.
// PKA: Hill(cAMP)·(1−depth·desens). state[0]=[cAMP]µM, [1]=PKA, [2]=desens, [3..6]=overrides.
static inline BIOCORE_HOST_DEVICE float camp_pka_step(Modifier* m, Segment* s, float dt) {
    // Агрегация Gs (стимулируют AC) и Gi/o (ингибируют AC) рецепторов сегмента.
    float gs = 0.0f, gi = 0.0f;
    for (uint8_t j = 0; j < s->modifier_count; j++) {
        if (is_gs_receptor_type(s->modifiers[j].type))   gs += s->modifiers[j].state[0];
        if (is_gi_o_receptor_type(s->modifiers[j].type)) gi += s->modifiers[j].state[0];
    }
    if (gs > 1.0f) gs = 1.0f;
    if (gi > 1.0f) gi = 1.0f;

    float vmax_ac  = (m->state[3] > 0.0f) ? m->state[3] : AC_VMAX_UM_PER_S;
    float vmax_pde = (m->state[4] > 0.0f) ? m->state[4] : PDE_VMAX_UM_PER_S;
    float ec50_pka = (m->state[5] > 0.0f) ? m->state[5] : PKA_EC50_CAMP_UM;
    float hill     = (m->state[6] > 0.0f) ? m->state[6] : PKA_HILL;

    float f_atp = s->ATP_level / (s->ATP_level + AC_KM_ATP_MM);
    // Net AC drive: конститутивная + Gαs (стимулирует) − Gαi (ингибирует), clamp ≥ 0.
    float drive = AC_BASAL + gs - gi;
    if (drive < 0.0f) drive = 0.0f;
    float ac = vmax_ac * m->density * drive * f_atp;      // µM/с

    float camp = m->state[0];
    float pde = vmax_pde * m->density * camp / (camp + PDE_KM_CAMP_UM);  // µM/с
    camp += (ac - pde) * dt * 0.001f;  // dt мс → с
    if (camp < 0.0f) camp = 0.0f;
    if (camp > CAMP_CEILING_UM) camp = CAMP_CEILING_UM;
    m->state[0] = camp;

    float d = m->state[2];
    d += (gs - d) * dt / GS_TAU_DESENS_MS;
    if (d < 0.0f) d = 0.0f; if (d > 1.0f) d = 1.0f;
    m->state[2] = d;

    float camp_hill = pow(camp, hill);
    m->state[1] = camp_hill / (pow(ec50_pka, hill) + camp_hill) * (1.0f - GS_DESENS_DEPTH * d);

    return 0.0f;
}

// ── Шаг каскада Gq → PLC → IP3 + DAG (единая реализация для fallback и плагина) ──
// Общий пул IP3+DAG на сегмент. PLC гидролизует PIP2 → IP3+DAG (1:1). IP3→IP3R, DAG→PKC.
// state[0]=IP3 (мМ), state[1]=DAG (норм.), state[2]=desens, state[3..4]=overrides (Vmax IP3/DAG).
static inline BIOCORE_HOST_DEVICE float plc_ip3_dag_step(Modifier* m, Segment* s, float dt) {
    // Агрегация Gq-рецепторов сегмента (стимулируют PLC). Сумма с насыщением.
    float gq = 0.0f;
    for (uint8_t j = 0; j < s->modifier_count; j++)
        if (is_gq_receptor_type(s->modifiers[j].type)) gq += s->modifiers[j].state[0];
    if (gq > 1.0f) gq = 1.0f;

    float vmax_ip3 = (m->state[3] > 0.0f) ? m->state[3] : IP3_PLC_MM_PER_S;
    float vmax_dag = (m->state[4] > 0.0f) ? m->state[4] : DAG_PLC_PER_S;
    float drive = PLC_BASAL + gq;

    float ip3 = m->state[0];
    ip3 += (vmax_ip3 * m->density * drive) * dt * 0.001f - ip3 * dt / IP3_TAU_MS;
    if (ip3 < 0.0f) ip3 = 0.0f;
    m->state[0] = ip3;

    float dag = m->state[1];
    dag += (vmax_dag * m->density * drive) * dt * 0.001f - dag * dt / DAG_TAU_MS;
    if (dag < 0.0f) dag = 0.0f;
    m->state[1] = dag;

    float d = m->state[2];
    d += (gq - d) * dt / GQ_TAU_DESENS_MS;
    if (d < 0.0f) d = 0.0f; if (d > 1.0f) d = 1.0f;
    m->state[2] = d;

    return 0.0f;
}

// ── Шаг PKC: DAG + Ca²⁺ → активность (классическая PKC α/β/γ, нужны оба) ──
// state[0] = PKC-активность (0-1). DAG из MOD_PLC_IP3_DAG, Ca²⁺ — цитозольный.
static inline BIOCORE_HOST_DEVICE float pkc_step(Modifier* m, Segment* s, float dt) {
    (void)dt;
    float dag = segment_dag(s);
    float ca  = s->intra_ions[ION_CA];
    if (ca < 0.0f) ca = 0.0f;
    float dag_f = dag / (dag + PKC_K_DAG);
    float ca_f  = ca  / (ca  + PKC_K_CA_MM);
    m->state[0] = dag_f * ca_f;  // мультипликативно: нужны и DAG, и Ca²⁺
    return 0.0f;
}

// ═══════════════════════════════════════════════════════════════════════════════
// МЕТАБОТРОПНЫЙ Gq — рецепторная оккупация + десенситизация (СИГНАЛЬНЫЙ, без тока)
// ✅ ДОСТОВЕРНО — Gαq → PLC → IP3+DAG. Пулом владеет MOD_PLC_IP3_DAG. return 0.
// state[0]=Gαq-активация, state[1]=десенситизация. EC50 — относительный (задача #20).
// ═══════════════════════════════════════════════════════════════════════════════
static inline BIOCORE_HOST_DEVICE float gq_signal(Modifier* m, Segment* s, float dt, int nt_index) {
    float p_EC50 = (m->state[m->active_vars] > 0.0f) ? m->state[m->active_vars] : 1.0f;
    float p_tau_desens = (m->state[m->active_vars + 1] > 0.0f) ? m->state[m->active_vars + 1] : GQ_TAU_DESENS_MS;

    float nt = s->neurotransmitter[nt_index];
    float occupancy = nt / (nt + p_EC50);

    // Gαq-активация (state[0]): быстрая G-белковая кинетика (GDP→GTP / GTP-гидролиз)
    float act = m->state[0];
    float tau_act = (occupancy > act) ? GQ_TAU_ACT_MS : GQ_TAU_DEACT_MS;
    act += (occupancy - act) * dt / tau_act;
    if (act < 0.0f) act = 0.0f; if (act > 1.0f) act = 1.0f;
    m->state[0] = act;

    // Десенситизация (state[1]): GRK фосфорилирует активированный GPCR → β-аррестин
    float d = m->state[1];
    d += (occupancy - d) * dt / p_tau_desens;
    if (d < 0.0f) d = 0.0f; if (d > 1.0f) d = 1.0f;
    m->state[1] = d;

    return 0.0f;  // сигнальный: пул IP3/DAG владеет MOD_PLC_IP3_DAG
}

// ── Mrgpr (Mas-related GPCR) — метаботропный рецептор зуда/хемоноцицепции ──
// Лиганд (хлороквин/BAM8-22) из state[4] (µM, как AITC у TRPA1). Gαq-активация state[0]
// + десенситизация state[1] (GRK/β-аррестин). Пулом IP3/DAG владеет MOD_PLC_IP3_DAG
// (is_gq_receptor_type → MOD_MRGPR). Нижестоящий: IP3→IP3R→Ca²⁺→TRPA1.
static inline BIOCORE_HOST_DEVICE float mrgpr_signal(Modifier* m, Segment* s, float dt) {
    float lig = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;
    float occupancy = lig / (lig + 28.0f);  // хлороквин EC50 28 µM (Liu 2009, PMID 20004959)

    float act = m->state[0];
    float tau_act = (occupancy > act) ? GQ_TAU_ACT_MS : GQ_TAU_DEACT_MS;
    act += (occupancy - act) * dt / tau_act;
    if (act < 0.0f) act = 0.0f; if (act > 1.0f) act = 1.0f;
    m->state[0] = act;

    float d = m->state[1];
    d += (occupancy - d) * dt / GQ_TAU_DESENS_MS;
    if (d < 0.0f) d = 0.0f; if (d > 1.0f) d = 1.0f;
    m->state[1] = d;

    return 0.0f;  // сигнальный
}
