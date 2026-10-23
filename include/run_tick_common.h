// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// run_tick_common.h — ЕДИНЫЕ КОНСТАНТЫ И ХЕЛПЕРЫ для ВСЕХ путей (CPU, CUDA, Intel SYCL).
// Подключается во все simulation_host.cpp, kernels_cuda.cu, kernels_part*.sycl.
// Изменения здесь применяются ко всем бекендам автоматически.

#pragma once
#include "types.h"
#include "kernel_dispatch.h"  // для BIOCORE_HOST_DEVICE
#include "units.h"            // MM_TO_UM и др. единицы
#include <cmath>

// ═══════════════════════════════════════════════════════════════
// V_m CLAMP (единый для всех путей)
// ═══════════════════════════════════════════════════════════════

#define VM_CLAMP_MIN  -120.0f  // био: ниже EK≈-90mV нейрон не уходит (было -200)
#define VM_CLAMP_MAX   150.0f

// ═══════════════════════════════════════════════════════════════
// SYNAPTIC RELEASE
// ═══════════════════════════════════════════════════════════════

#define SPIKE_THRESHOLD  -40.0f
#define RELEASE_REFRACTORY  2

// ═══════════════════════════════════════════════════════════════
// GAP JUNCTION / ELECTRICAL SYNAPSE
// ═══════════════════════════════════════════════════════════════

#define GJ_FAST_TAU     5.0f
#define GJ_VJ_HALF      40.0f
#define GJ_VJ_SLOPE     5.0f
#define GJ_SLOW_TAU     100.0f
#define GJ_CA_HALF      0.0003f
#define GJ_RESIDUAL     0.1f
#define GJ_CX36_FLOOR   0.01f
// Cx36 — низкопроводящий нейрональный коннексин, унитарная проводимость ~14 пС
// (Srinivas 1999 J Neurosci 19:9848: 10–15 пС; Teubner 2000 J Membr Biol 176:249: 14.3±0.8 пС).
#define CX36_GAMMA_PS   14.0f
// Cx43 — высокопроводящий глиальный коннексин, унитарная проводимость ~100 пС
// (Veenstra 1995 Circ Res 77:1156: 60–120 пС; главное состояние 100 пС).
#define CX43_GAMMA_PS   100.0f
// Cx43 Vj-гейт (инактивация трансмембранным напряжением): в отличие от Cx36, Cx43
// закрывается при |Vj| > ~40 мВ (Boltzmann). Moreno 1994; Bennett & Zukin 2004.
#define CX43_VJ_HALF_MV  40.0f
#define CX43_VJ_SLOPE_MV 5.0f

// Синаптотагмин-1 (Ca²⁺-сенсор выброса): Hill n=4, Kd=10 µM (глобальный Ca²⁺ масштаб).
// EC₅₀≈20 µM (микродомен), Hill≈3.6 — см. notes/modifier_refs/CAV21_140.md.
#define SYT_KD_UM    10.0f
#define SYT_HILL      4.0f

// Пресинаптический микродомен Ca²⁺ (бутон): Cav2.1 накачивает Ca²⁺ в крошечный объём
// (~0.1 мкм³), PMCA/буфер убирают. Объём бутона << сомы → рост Ca²⁺ до 10–100 µM при спайке.
#define BOUTON_VOLUME_UM3     0.1f   // объём пресинаптического бутона (мкм³)
#define BOUTON_CA_TAU_MS      5.0f   // спад Ca²⁺ в бутоне (PMCA + эндогенный буфер)
#define BOUTON_CA_BASELINE_MM 0.0001f // базовый [Ca²⁺] бутона = 100 нМ
// #9: осевая проводимость аксон↔бутон для делителя напряжения (пС-эквивалент, нормирована
// к density×γ). g_axial≈300 при Kv1.1 density=9/γ=10 (g_K=90) даёт ~23% аттенюацию пика ПД (Neuron 2014 ~30%).
#define BOUTON_G_AXIAL_PS      300.0f

// Шипик (постсинаптический микродомен): объём головки ~0.05 мкм³, Ca²⁺ спадает медленнее
// бутона (шейка шипика ограничивает диффузию в дендритный ствол) — τ ~50 мс.
#define SPINE_VOLUME_UM3       0.05f   // объём головки шипика (мкм³)
#define SPINE_CA_TAU_MS        50.0f   // спад Ca²⁺ в шипике (шейка замедляет диффузию)
#define SPINE_CA_BASELINE_MM   0.0001f // базовый [Ca²⁺] шипика = 100 нМ
#define SPINE_CA_FRACTION      0.1f    // Ca²⁺-проницаемая фракция постсинаптического тока (NMDA ~10%)
#define SPINE_CA_BUFFER_CAPACITY 100.0f // κ_B — буферная ёмкость шипика (Neher 1998: ~20–200). Свободный Ca²⁺ = тотальный/κ_B (иначе свободный Ca²⁺ 247 µM вместо ~1–5 µM — runaway)

// Эффективная проводимость gap junction (SINGLE-SOURCE, CPU+SYCL+CUDA).
// g_eff(нС) = Σ density × connexon_count × P_open × γ_connexon × (1 − m_slow).
// Поддерживает Cx36 (нейрональный, γ=14 пС) и Cx43 (глиальный, γ=100 пС) — homotypic
// (dIN↔dIN = Cx36, глия↔глия = Cx43). Cx36 — наименее Vj-зависимый коннексин (Srinivas 1999,
// Teubner 2000): быстрый Vj-гейт почти отсутствует. Cx43 — Vj-зависимый: закрывается при
// |Vj| > ~40 мВ (Boltzmann, Moreno 1994). Общий медленный Ca²⁺/CaM-гейт — m_slow.
BIOCORE_HOST_DEVICE static inline float gap_junction_g_eff(const Segment* pre, const Segment* post,
                                                           float m_slow, float Vj) {
    uint8_t pre_mc = pre->modifier_count;
    if (pre_mc > MAX_MODIFIERS_PER_SEGMENT) pre_mc = 0;
    uint8_t post_mc = post->modifier_count;
    if (post_mc > MAX_MODIFIERS_PER_SEGMENT) post_mc = 0;

    float g_gap_ps = 0.0f;  // суммарная проводимость в пС
    for (int cx = 0; cx < 2; cx++) {
        uint8_t cx_type = (cx == 0) ? MOD_CONNEXIN_CX36 : MOD_CONNEXIN_CX43;
        float gamma = (cx == 0) ? CX36_GAMMA_PS : CX43_GAMMA_PS;
        float density = 0.0f;
        float f = 1.0f;
        for (uint8_t j = 0; j < pre_mc; j++)
            if (pre->modifiers[j].type == cx_type) {
                density = pre->modifiers[j].density;
                f *= pre->modifiers[j].state[0] * pre->modifiers[j].state[4];
            }
        for (uint8_t j = 0; j < post_mc; j++)
            if (post->modifiers[j].type == cx_type)
                f *= post->modifiers[j].state[0] * post->modifiers[j].state[4];
        // Cx43: Vj-гейт (инактивация) — только для глиального коннексина (Cx36 почти без Vj-гейта).
        if (cx == 1) {
            float vj_abs = Vj < 0.0f ? -Vj : Vj;
            float f_vj = 1.0f / (1.0f + exp((vj_abs - CX43_VJ_HALF_MV) / CX43_VJ_SLOPE_MV));
            f *= f_vj;
        }
        if (f < GJ_CX36_FLOOR) f = GJ_CX36_FLOOR;
        g_gap_ps += density * f * gamma;
    }
    return g_gap_ps * 1e-3f * (1.0f - m_slow);  // пС → нС
}

// Медленный Ca²⁺/CaM-гейт Cx36 (single-source): Hill n=2, τ=100ms.
// ca_pre/ca_post — внутриклеточный Ca²⁺ (мМ) pre/post. Возвращает обновлённый m_slow ∈ [0,1].
BIOCORE_HOST_DEVICE static inline float gap_junction_cam_update(float m_slow,
                                                                float ca_pre, float ca_post, float dt) {
    float ca_avg = 0.5f * (ca_pre + ca_post);
    float cam_act = (ca_avg * ca_avg) / (ca_avg * ca_avg + GJ_CA_HALF * GJ_CA_HALF);
    m_slow += (cam_act - m_slow) * dt / GJ_SLOW_TAU;
    if (m_slow < 0.0f) m_slow = 0.0f;
    if (m_slow > 1.0f) m_slow = 1.0f;
    return m_slow;
}

// ═══════════════════════════════════════════════════════════════
// STDP / PLASTICITY
// ═══════════════════════════════════════════════════════════════

#define PLAST_TAU_THETA    300000.0f
#define PLAST_LTP_RATE     0.0003f
#define PLAST_LTD_RATE     0.0001f
#define PLAST_THETA_MIN    0.01f
#define PLAST_ECB_RATE     0.1f
#define PLAST_ECB_CONSUME  0.9f

// ═══════════════════════════════════════════════════════════════
// MEMBRANE
// ═══════════════════════════════════════════════════════════════

#define C_M_MIN  1e-4f

// ═══════════════════════════════════════════════════════════════
// DIFFUSION
// ═══════════════════════════════════════════════════════════════

#define DIFF_AREA_MIN   1e-6f
#define DIFF_DIST_MIN   0.1f
#define DIFF_VOL_MIN    1e-6f
#define DIFF_DC_MIN     1e-12f
#define DIFF_DTEMP_MIN  0.001f
#define DIFF_DTXN_MIN   1e-9f

// EC/IC volume ratio (биологически: EC space ~20% объёма мозга)
#define EC_IC_VOLUME_RATIO 50.0f

// Коэффициенты диффузии (мкм²/мс) — Источник: Kushmerick & Podolsky (1969), 37°C
#define DIFF_NA   1.33f
#define DIFF_K    1.96f
#define DIFF_CA   0.22f
#define DIFF_CL   2.03f
#define DIFF_MG   0.10f
#define DIFF_A    0.0f    // A⁻ — неподвижный макроанион (не диффундирует)
#define DIFF_H    9.31f   // H⁺ — протон (самый быстрый ион, D≈9.3 мкм²/мс)
#define DIFF_ATP  0.14f
#define DIFF_HEAT 0.0014f

// ═══════════════════════════════════════════════════════════════
// НЕРНСТ-ПЛАНК (электродиффузия) — единые константы (CPU + GPU)
// J_i = −D_i·∇C_i − D_i·z_i·(F/RT)·C_i·∇V  (диффузия + электромиграция).
// Сопротивление НЕ задаётся — возникает из подвижности (Эйнштейн μ=D·z·F/RT).
// См. notes/modifier_refs/ELECTRODIFFUSION.md.
// ═══════════════════════════════════════════════════════════════

// F/(R·T) при 37°C (310.15 K) [мВ⁻¹]: 96485/(8.314·310.15)·1000 = 37.42 В⁻¹ = 0.03742 мВ⁻¹
#define NP_F_RT_MV  0.03742f

// Валентности ионов (ION_NA, ION_K, ION_CA, ION_CL, ION_MG)
#define NP_Z_NA   1.0f
#define NP_Z_K    1.0f
#define NP_Z_CA   2.0f
#define NP_Z_CL  -1.0f
#define NP_Z_MG   2.0f
#define NP_Z_A   -1.0f   // A⁻ — неподвижный макроанион
#define NP_Z_H    1.0f   // H⁺

// Конверсия заряд-флюкса → ток [пА]:
// flux_i [мМ·мкм³] (перенос за dt); 1 мМ·мкм³ = 10⁻¹⁸ моль.
// I[пА] = z_i·F·(flux_i·10⁻¹⁸ моль)/(dt·10⁻³ с) · 10¹² = z_i·F·flux_i·10⁻³/dt.
// F·10⁻³ = 96.485. (Было 0.096485 = F·10⁻⁶ — баг ×1000; верное 96.485 жёстко → сабстеппинг в шаге 2.5.)
#define NP_CHARGE_TO_PA  96.485f

// Число подшагов внутриклеточного Нернст-Планка (жёсткая электромиграция):
// dt_sub = dt/NP_SUBSTEPS. τ_AIS ≈ 0.01 мс при dt=0.1 → N≥10 для устойчивости forward-Euler.
#define NP_SUBSTEPS  200

// ═══════════════════════════════════════════════════════════════
// SYNAPTIC WEIGHTS
// ═══════════════════════════════════════════════════════════════

#define WEIGHT_MIN  0.01f
#define WEIGHT_MAX  10.0f
#define WEIGHT_NAN_DEFAULT  0.5f

// ═══════════════════════════════════════════════════════════════
// INLINE HELPERS (работают на CPU и GPU)
// ═══════════════════════════════════════════════════════════════

// Вычислить геометрическую площадь мембраны (µm²)
// Сфера: SOMA(1), PRESYNAPSE(3), POSTSYNAPSE(4). Цилиндр: всё остальное.
BIOCORE_HOST_DEVICE static inline float segment_membrane_area(const Segment* s) {
    return (s->segment_type == 1 || s->segment_type == 3 || s->segment_type == 4)
        ? 4.0f * PI * s->radius * s->radius   // sphere (soma, bouton, spine head)
        : 2.0f * PI * s->radius * s->length;  // cylinder (dendrite, axon, neck)
}

// Вычислить ёмкость мембраны (pF)
BIOCORE_HOST_DEVICE static inline float segment_capacitance(const Segment* s) {
    float area = segment_membrane_area(s);
    float C_m = area * C_SPECIFIC_PF_PER_UM2;
    if (C_m < C_M_MIN) C_m = C_M_MIN;
    for (uint8_t j = 0; j < s->modifier_count && j < 16; j++) {
        if (s->modifiers[j].type == MOD_MYELIN) {
            float gr = s->modifiers[j].state[0];
            if (gr <= 0.0f || gr >= 1.0f) gr = 0.6f;  // default g-ratio (толстый миелин)
            // число слоёв ∝ (1/g_ratio − 1), C_m ∝ 1/толщина → factor: 0.6→100, 0.8→37.5
            float factor = 100.0f * (1.0f / gr - 1.0f) / (1.0f / 0.6f - 1.0f);
            C_m /= factor;
            break;
        }
    }
    return C_m;
}

// Установить effective_area в state[7] модификатора
BIOCORE_HOST_DEVICE static inline void set_modifier_effective_area(Modifier* m, const Segment* s) {
    m->state[7] = (s->segment_type == 1 || s->segment_type == 3 || s->segment_type == 4)
        ? 4.0f * PI * s->radius * s->radius   // sphere
        : 2.0f * PI * s->radius * s->length;  // cylinder
}

// Тип нейромедиатора по типу постсинаптического рецептора
BIOCORE_HOST_DEVICE static inline uint8_t nt_type_from_receptor_type(uint8_t rt) {
    if (rt == MOD_NT_RECEPTOR_GABAA || rt == MOD_NT_RECEPTOR_GABAB) return NT_GABA;
    if (rt == MOD_NT_RECEPTOR_GLYCINE) return NT_GLYCINE;
    if (rt == MOD_NT_RECEPTOR_5HT3)   return NT_SEROTONIN;
    if (rt == MOD_NT_RECEPTOR_NICOTINIC) return NT_ACETYLCHOLINE;
    if (rt == MOD_NT_RECEPTOR_D1 || rt == MOD_NT_RECEPTOR_D2) return NT_DOPAMINE;
    return NT_GLUTAMATE;
}

// NaN-safe V_m clamp (единый для всех путей)
BIOCORE_HOST_DEVICE static inline void clamp_V_m(Segment* s) {
    if (s->V_m != s->V_m) { s->V_m = -65.0f; return; }  // NaN → resting
    if (s->V_m < VM_CLAMP_MIN) s->V_m = VM_CLAMP_MIN;
    if (s->V_m > VM_CLAMP_MAX) s->V_m = VM_CLAMP_MAX;
}

// Единая формула объёма сегмента (Single Source — все пути берут отсюда).
// Сома (1), бутон (3), шипик (4) — сфера; аксон/дендрит/AIS — цилиндр (полный объём).
// Свободная цитоплазма ~70–80% → доля 1.0 (исключённый объём органелл отдельно не моделируем).
BIOCORE_HOST_DEVICE static inline double segment_volume_compute(const Segment* s) {
    if (s->segment_type == 1 || s->segment_type == 3 || s->segment_type == 4)
        return (4.0 / 3.0) * PI * (double)s->radius * (double)s->radius * (double)s->radius;  // сома/бутон/шипик — сфера
    return PI * (double)s->radius * (double)s->radius * (double)s->length;  // аксон/дендрит/AIS — цилиндр
}

// ═══════════════════════════════════════════════════════════════
// Единый GHK (Goldman-Hodgkin-Katz) — CPU + GPU
// Идентичен plugin_helpers.h, вынесен для доступности в SYCL/CUDA коде
// ═══════════════════════════════════════════════════════════════

// ✅VERIFIED (учебник, test_ghk_reference): GHK-ток J = P·z²·F²/(RT)·V·(c_in−c_out·e)/(1−e)
// Возвращаем з²·F²/(RT)·V·(...): моновалентный z=1, дивалентный z=2 (встроено ниже).
// double — точность для ионов (K_i=140 мМ, transport ~1e-5 мМ терялся в float32).
BIOCORE_HOST_DEVICE static inline double ghk_monovalent(double V, double c_in, double c_out) {
    double V_norm = FARADAY_CONST * V * 0.001 / (GAS_CONST * CELSIUS_37);
    if (fabs(V_norm) < 1e-6)
        return FARADAY_CONST * (c_in - c_out);  // V=0 предел: z·F·(c_i−c_o), z=1
    double k = FARADAY_CONST / (GAS_CONST * CELSIUS_37);  // F/(RT)
    if (V_norm > 20.0)
        return k * FARADAY_CONST * V * 0.001 * c_in;
    if (V_norm < -20.0)
        return k * FARADAY_CONST * V * 0.001 * c_out;
    return k * FARADAY_CONST * V * 0.001 *
           (c_in - c_out * exp(-V_norm)) / (1.0 - exp(-V_norm));
}

BIOCORE_HOST_DEVICE static inline double ghk_divalent(double V, double c_in, double c_out) {
    double V_norm = 2.0 * FARADAY_CONST * V * 0.001 / (GAS_CONST * CELSIUS_37);
    if (fabs(V_norm) < 1e-6)
        return 2.0 * FARADAY_CONST * (c_in - c_out);  // V=0 предел: z·F·(c_i−c_o), z=2
    double k = 4.0 * FARADAY_CONST / (GAS_CONST * CELSIUS_37);  // z²·F/(RT), z=2
    if (V_norm > 20.0)
        return k * FARADAY_CONST * V * 0.001 * c_in;
    if (V_norm < -20.0)
        return k * FARADAY_CONST * V * 0.001 * c_out;
    return k * FARADAY_CONST * V * 0.001 *
           (c_in - c_out * exp(-V_norm)) / (1.0 - exp(-V_norm));
}

BIOCORE_HOST_DEVICE static inline double ghk_anion(double V, double c_in, double c_out) {
    // Cl⁻: заряд -1, GHK симметричен K⁺ но c_out ↔ c_in (поток противоположен)
    double V_norm = FARADAY_CONST * V * 0.001 / (GAS_CONST * CELSIUS_37);
    if (fabs(V_norm) < 1e-6)
        return FARADAY_CONST * (c_out - c_in);  // V=0 предел
    double k = FARADAY_CONST / (GAS_CONST * CELSIUS_37);  // F/(RT)
    if (V_norm > 20.0)
        return k * FARADAY_CONST * V * 0.001 * c_out;
    if (V_norm < -20.0)
        return k * FARADAY_CONST * V * 0.001 * c_in;
    return k * FARADAY_CONST * V * 0.001 *
           (c_out - c_in * exp(-V_norm)) / (1.0 - exp(-V_norm));
}
