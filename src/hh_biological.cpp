// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#include "types.h"
#include <cstdio>
#include "modifier_interface.h"
#include "kernel_dispatch.h"
#include "run_tick_common.h"
#include "modifier_helpers.h"
#include <cmath>

// Глобальные таблицы функций
ModifierFunc biological_library[MOD_TYPE_COUNT];
ModifierFunc arbitrary_library[MOD_TYPE_COUNT];
ReleaseModulatorFunc biological_modulators[MOD_TYPE_COUNT];
ReleaseModulatorFunc arbitrary_modulators[MOD_TYPE_COUNT];

ModifierFunc* active_library = nullptr;
ReleaseModulatorFunc* active_modulators = nullptr;

// ==================== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ====================
// nernst, clamp_ions, safe_ghk_denom, ghk_*, segment_area, segment_volume,
// effective_area, charge_per_mole: единый источник в modifier_helpers.h

// backward-compat: seg_volume → segment_volume
#define seg_volume(s) segment_volume(s)

// ── Сенсорная адаптация: локальные хелперы (GPU/CPU-fallback) ────────────────

// Контр-направленная адаптация (habituation): dH/dt = (act - H)/tau
static inline BIOCORE_HOST_DEVICE void sensor_habituation_step(Modifier* m, int idx_H, float activation, float dt,
                                            float tau_hab_ms) {
    if (tau_hab_ms < 1.0f) return;
    m->state[idx_H] += (activation - m->state[idx_H]) * dt / tau_hab_ms;
    if (m->state[idx_H] < 0.0f) m->state[idx_H] = 0.0f;
    if (m->state[idx_H] > 1.0f) m->state[idx_H] = 1.0f;
}

// Со-направленная адаптация (sensitization): dS/dt = (act - S)/tau
static inline BIOCORE_HOST_DEVICE void sensor_sensitization_step(Modifier* m, int idx_S, float activation, float dt,
                                              float tau_sens_ms) {
    if (tau_sens_ms < 1.0f) return;
    m->state[idx_S] += (activation - m->state[idx_S]) * dt / tau_sens_ms;
    if (m->state[idx_S] < 0.0f) m->state[idx_S] = 0.0f;
    if (m->state[idx_S] > 1.0f) m->state[idx_S] = 1.0f;
}

// Комбинированное усиление: gain = (1 + k_sens*S) * (1 - k_hab*H)
static inline BIOCORE_HOST_DEVICE float sensor_adaptation_gain(float H, float S, float k_hab, float k_sens) {
    return (1.0f + k_sens * S) * (1.0f - k_hab * H);
}


// ==================== Na⁺/K⁺-АТФаза (насос) ====================

BIOCORE_DEVICE_CALLABLE double nak_pump_current(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — Na/K-АТФаза, цикл Поста–Альберса (Smith & Crampin 2004, Läuger 1991).
    // Менять ТОЛЬКО с согласования (алгоритм аудита модификаторов).
    float area = effective_area(m, s);

    // Нормированный оборот цикла v_norm ∈ [0,1]: V-зависимость + Hill-3 Na + Hill-2 K + ATP.
    float v_norm = nak_pump_vnorm(s->V_m, s->intra_ions[ION_NA], s->extra_ions[ION_K], s->ATP_level,
                                  NA_K_PUMP_K_NA_MM, NA_K_PUMP_K_K_MM);

    // ЕДИНИЦА — молекула: I = N·k_cat·v_norm·e·Δz. N = число молекул насоса на сегменте,
    // k_cat = NA_K_PUMP_TURNOVER_MAX_S, e·Δz = +1 заряд/цикл (электрогенный, наружу).
    double N = m->density * area;   // молекул (density — клеточный параметр: число молекул/µm²)
    float I_pump = N * v_norm * NA_K_PUMP_TURNOVER_MAX_S * ELEMENTARY_CHARGE_C * A_TO_PA; // пА

    // Перенос ионов: 3 Na⁺ наружу, 2 K⁺ внутрь (стехиометрия цикла).
    float volume = seg_volume(s);
    double cpm = FARADAY_CONST * volume / 1000.0;
    float transport = I_pump * dt / cpm;
    // Ограничить транспорт доступными ионами: 3 Na из intra, 2 K из extra (×10 резервуар).
    float na_avail = (s->intra_ions[ION_NA] - 1e-9f) / 3.0f;
    float k_avail  = (s->extra_ions[ION_K]  - 1e-9f) * EXTRA_VOLUME_RATIO / 2.0f;
    if (na_avail < 0.0f) na_avail = 0.0f;
    if (k_avail  < 0.0f) k_avail  = 0.0f;
    float max_t = (na_avail < k_avail) ? na_avail : k_avail;
    if (transport > max_t) transport = max_t;

    s->intra_ions[ION_NA] -= 3.0f * transport;
    s->extra_ions[ION_NA] += 3.0f * transport / EXTRA_VOLUME_RATIO;
    s->intra_ions[ION_K]  += 2.0f * transport;
    s->extra_ions[ION_K]  -= 2.0f * transport / EXTRA_VOLUME_RATIO;
    clamp_ions(s->intra_ions, ION_COUNT);
    clamp_ions(s->extra_ions, ION_COUNT);

    // Потребление ATP: 1 ATP на цикл → пропорционально току.
    s->ATP_level -= I_pump * dt * NA_K_PUMP_ATP_RATE_PA;
    if (s->ATP_level < 0.0f) s->ATP_level = 0.0f;

    // Электрогенный (+1 наружу) → гиперполяризующий → отрицательный ток для V_m.
    return -I_pump;
}

// ==================== Na⁺/K⁺-АТФаза α3 (динамический насос, usAHP) ====================
BIOCORE_DEVICE_CALLABLE double nak_pump_a3_current(Modifier* m, Segment* s, float dt) {
    // α3-изоформа (Zahler 1997, Blanco 2005): НИЗКОЕ сродство к Na⁺_i (K_m≈25 мМ).
    // Тонически почти неактивна на покое ([Na⁺]_i≈15 → f_Na мал), активируется при
    // сильном росте [Na⁺]_i во время залпа спайков → usAHP (обрыв повторной стрельбы).
    float area = effective_area(m, s);

    float v_norm = nak_pump_vnorm(s->V_m, s->intra_ions[ION_NA], s->extra_ions[ION_K], s->ATP_level,
                                  NA_K_PUMP_A3_K_NA_MM, NA_K_PUMP_K_K_MM);

    // Уабаин-ингибиция (state[5], нМ): конкурентный блок E2P-конформации (Blanco 1993).
    double oua = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    v_norm /= (1.0f + oua / NA_K_PUMP_OUABAIN_IC50_NM);

    double N = m->density * area;
    float I_pump = N * v_norm * NA_K_PUMP_TURNOVER_MAX_S * ELEMENTARY_CHARGE_C * A_TO_PA; // пА

    // Перенос ионов: 3 Na⁺ наружу, 2 K⁺ внутрь (стехиометрия цикла).
    float volume = seg_volume(s);
    double cpm = FARADAY_CONST * volume / 1000.0;
    float transport = I_pump * dt / cpm;
    float na_avail = (s->intra_ions[ION_NA] - 1e-9f) / 3.0f;
    float k_avail  = (s->extra_ions[ION_K]  - 1e-9f) * EXTRA_VOLUME_RATIO / 2.0f;
    if (na_avail < 0.0f) na_avail = 0.0f;
    if (k_avail  < 0.0f) k_avail  = 0.0f;
    float max_t = (na_avail < k_avail) ? na_avail : k_avail;
    if (transport > max_t) transport = max_t;

    s->intra_ions[ION_NA] -= 3.0f * transport;
    s->extra_ions[ION_NA] += 3.0f * transport / EXTRA_VOLUME_RATIO;
    s->intra_ions[ION_K]  += 2.0f * transport;
    s->extra_ions[ION_K]  -= 2.0f * transport / EXTRA_VOLUME_RATIO;
    clamp_ions(s->intra_ions, ION_COUNT);
    clamp_ions(s->extra_ions, ION_COUNT);

    s->ATP_level -= I_pump * dt * NA_K_PUMP_ATP_RATE_PA;
    if (s->ATP_level < 0.0f) s->ATP_level = 0.0f;

    // Электрогенный (+1 наружу) → гиперполяризующий.
    return -I_pump;
}

// ==================== ЗАГЛУШКИ ====================

BIOCORE_DEVICE_CALLABLE double ca_current(Modifier* m, Segment* s, float dt) {
    // Cav1.3 (CACNA1D) — L-type Ca²⁺, пейсмейкерный (Xu & Lipscombe 2001).
    // ✅ ДОСТОВЕРНО — менять ТОЛЬКО с согласования (алгоритм аудита модификаторов).
    // τ фиксированы (τ_m=2 мс, τ_h=100 мс) — слабая V-зависимость L-type в рабочем диапазоне.
    // bell-shaped вариант τ(V) возможен, но НЕ откалиброван лабораторно (требует сверки).
    float V = s->V_m;
    float area = effective_area(m, s);
    double Vhalf = (m->state[3] != 0.0f) ? -fabs(m->state[3]) : -35.0f; // активация Cav1.3
    double k = (m->state[4] > 0.0f) ? m->state[4] : 7.0f;  // slope активации
    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : 25.0f;  // γ (пС, L-type)

    // Активация: m² (L-type — два независимых гейта активации)
    float m_inf = 1.0f / (1.0f + exp(-(V - Vhalf) / (k + 1e-6f)));
    float tau_m = 2.0f; // мс — быстрая активация
    m->state[0] += (m_inf - m->state[0]) * dt / tau_m;

    // Инактивация: медленная VDI (V½=−52) + слабая CDI (Kd=3 µM, слабее Cav1.2)
    float Ca = s->intra_ions[ION_CA] * MM_TO_UM; // µM
    float h_inf = 1.0f / (1.0f + exp((V + 52.0f) / 8.0f));
    h_inf *= 1.0f / (1.0f + Ca / 3.0f);
    float tau_h = 100.0f; // мс — медленная инактивация → плато
    m->state[1] += (h_inf - m->state[1]) * dt / tau_h;

    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    // GHK для Ca²⁺ (z=2), P в см/с (честная γ→P шкала, паритет с плагином mod_01)
    float ca_in  = s->intra_ions[ION_CA];
    float ca_out = s->extra_ions[ION_CA];
    double P_single = ghk_single_channel_P(gamma_ps, 2.0f, GHK_CBAR_CA_MM);
    double open_prob = m->state[0] * m->state[0] * m->state[1]; // m²·h
    double flux = ghk_divalent(V, ca_in, ca_out);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    double I_Ca = I_A * A_TO_PA;

    double vol = seg_volume(s);
    double cpm = charge_per_mole(vol);
    s->intra_ions[ION_CA] -= (I_Ca * dt) / (2.0f * cpm);
    s->extra_ions[ION_CA] -= (I_Ca * dt) / (2.0f * cpm * EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT);
    clamp_ions(s->extra_ions, ION_COUNT);

    return -I_Ca;
}

// ==================== NAV16 (141): аксональный Na⁺ (Nav1.6/SCN8A) ====================
// Био: Nav1.6 (SCN8A) — аксональный transient Na⁺, repetitive firing (НЕ глубокая
// инактивация single-spike dIN). GHK-парадигма (γ=20.7 пС), как NaP_105. m³h гейтинг
// PNAS 2018. См. notes/modifier_refs/NAV16_141.md.
BIOCORE_DEVICE_CALLABLE double nav16_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    if (s->V_m < -200.0f) s->V_m = -200.0f;
    if (s->V_m > 200.0f)  s->V_m = 200.0f;
    V = s->V_m;

    // m³h гейтинг (PNAS 2018 HH), экспоненциальный Эйлер
    double m_val = nav16_m_inf(V) + (m->state[0] - nav16_m_inf(V)) * exp(-dt / nav16_m_tau(V));
    double h_val = nav16_h_inf(V) + (m->state[1] - nav16_h_inf(V)) * exp(-dt / nav16_h_tau(V));
    if (m_val < 0.0f) m_val = 0.0f; if (m_val > 1.0f) m_val = 1.0f;
    if (h_val < 0.0f) h_val = 0.0f; if (h_val > 1.0f) h_val = 1.0f;
    m->state[0] = m_val;
    m->state[1] = h_val;

    // GHK: P = γ·density, I_Na = P·flux·area·m³h (как NaP_105)
    float open_prob = m_val * m_val * m_val * h_val;
    // Ингибиция: TTX (state[5], нМ) + PKA/PKC фосфорилирование (слабое — Nav1.6 без Ser573)
    double ttx = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    open_prob /= (1.0f + ttx / TTX_IC50_NAV16);
    float pkc = segment_pkc_act(s);
    float pka = segment_pka_act(s);
    open_prob *= (1.0f - PKA_PKC_INHIBIT_NAV16 * (pkc + pka) * 0.5f);
    if (open_prob < 0.0f) open_prob = 0.0f;

    double P_single = ghk_single_channel_P(NAV16_GAMMA_PS, 1.0f, GHK_CBAR_NA_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    double I_Na = I_A * A_TO_PA;

    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpv;
    s->extra_ions[ION_NA] += (I_Na * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT);
    clamp_ions(s->extra_ions, ION_COUNT);

    return -I_Na;
}

// ==================== NAV12 (142): аксональный Na⁺ (Nav1.2/SCN2A) ====================
// Био: Nav1.2 (SCN2A) — тадпольный спинальный Na-изоформ (ранние нейроны Xenopus
// экспрессируют Nav1.2, не Nav1.6). Активация V½≈−23, инактивация V½≈−63.5 (JGP 2022).
// GHK-парадигма (γ≈20 пС). m³h гейтинг. См. notes/modifier_refs/NAV12_142.md.
BIOCORE_DEVICE_CALLABLE double nav12_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    if (s->V_m < -200.0f) s->V_m = -200.0f;
    if (s->V_m > 200.0f)  s->V_m = 200.0f;
    V = s->V_m;

    // m³h гейтинг, экспоненциальный Эйлер
    double m_val = nav12_m_inf(V) + (m->state[0] - nav12_m_inf(V)) * exp(-dt / nav12_m_tau(V));
    double h_val = nav12_h_inf(V) + (m->state[1] - nav12_h_inf(V)) * exp(-dt / nav12_h_tau(V));
    if (m_val < 0.0f) m_val = 0.0f; if (m_val > 1.0f) m_val = 1.0f;
    if (h_val < 0.0f) h_val = 0.0f; if (h_val > 1.0f) h_val = 1.0f;
    m->state[0] = m_val;
    m->state[1] = h_val;

    // GHK: P = γ·density, I_Na = P·flux·area·m³h
    float open_prob = m_val * m_val * m_val * h_val;
    // Ингибиция: TTX (state[5], нМ) + PKA/PKC фосфорилирование (сильное — Nav1.2 Ser573)
    double ttx = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    open_prob /= (1.0f + ttx / TTX_IC50_NAV12);
    float pkc = segment_pkc_act(s);
    float pka = segment_pka_act(s);
    open_prob *= (1.0f - PKA_PKC_INHIBIT_NAV12 * (pkc + pka) * 0.5f);
    if (open_prob < 0.0f) open_prob = 0.0f;

    double P_single = ghk_single_channel_P(NAV12_GAMMA_PS, 1.0f, GHK_CBAR_NA_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    double I_Na = I_A * A_TO_PA;

    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpv;
    s->extra_ions[ION_NA] += (I_Na * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT);
    clamp_ions(s->extra_ions, ION_COUNT);

    return -I_Na;
}

BIOCORE_DEVICE_CALLABLE double nav11_current(Modifier* m, Segment* s, float dt) {
    // Nav1.1 (SCN1A) — transient Na⁺, repetitive (PV⁺-интернейроны, проприоцепторы, таламус).
    // γ=17 пС (Vanoye 2006). V½_акт=−30 / инакт=−65 (PMC4220023). Однофазный быстрый recovery.
    float V = s->V_m;
    float area = effective_area(m, s);
    if (s->V_m < -200.0f) s->V_m = -200.0f;
    if (s->V_m > 200.0f)  s->V_m = 200.0f;
    V = s->V_m;

    double m_val = nav11_m_inf(V) + (m->state[0] - nav11_m_inf(V)) * exp(-dt / nav11_m_tau(V));
    double h_val = nav11_h_inf(V) + (m->state[1] - nav11_h_inf(V)) * exp(-dt / nav11_h_tau(V));
    if (m_val < 0.0f) m_val = 0.0f; if (m_val > 1.0f) m_val = 1.0f;
    if (h_val < 0.0f) h_val = 0.0f; if (h_val > 1.0f) h_val = 1.0f;
    m->state[0] = m_val;
    m->state[1] = h_val;

    float open_prob = m_val * m_val * m_val * h_val;
    double ttx = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    open_prob /= (1.0f + ttx / TTX_IC50_NAV12);

    double P_single = ghk_single_channel_P(NAV11_GAMMA_PS, 1.0f, GHK_CBAR_NA_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    double I_Na = I_A * A_TO_PA;

    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpv;
    s->extra_ions[ION_NA] += (I_Na * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT);
    clamp_ions(s->extra_ions, ION_COUNT);

    return -I_Na;
}

BIOCORE_DEVICE_CALLABLE double nav12dev_current(Modifier* m, Segment* s, float dt) {
    // Nav1.2 developmental (Scn2a) — тадпольный аксональный Na⁺ (тип 193).
    // Как Nav1.2, но инактивация деполяризована (V½=−35) → доступен на покое −51 мВ.
    float V = s->V_m;
    float area = effective_area(m, s);

    if (s->V_m < -200.0f) s->V_m = -200.0f;
    if (s->V_m > 200.0f)  s->V_m = 200.0f;
    V = s->V_m;

    double m_val = nav12dev_m_inf(V) + (m->state[0] - nav12dev_m_inf(V)) * exp(-dt / nav12dev_m_tau(V));
    double vhalf = (m->state[2] > 0.0f) ? m->state[2] : 45.0f;  // V½_инакт калибруемый (state[2], мВ)
    float h_inf = nav12dev_h_inf(V, vhalf);
    double h_fast = h_inf + (m->state[1] - h_inf) * exp(-dt / nav12_h_tau(V));
    // slow-компонента: инактивация БЫСТРАЯ (τ_h), recovery МЕДЛЕННЫЙ (τ_slow) — двухфазный recovery.
    double tau_slow = (h_inf < m->state[3]) ? nav12_h_tau(V) : NAV12DEV_SLOW_TAU_MS;
    double h_slow = h_inf + (m->state[3] - h_inf) * exp(-dt / tau_slow);
    if (m_val < 0.0f) m_val = 0.0f; if (m_val > 1.0f) m_val = 1.0f;
    if (h_fast < 0.0f) h_fast = 0.0f; if (h_fast > 1.0f) h_fast = 1.0f;
    if (h_slow < 0.0f) h_slow = 0.0f; if (h_slow > 1.0f) h_slow = 1.0f;
    m->state[0] = m_val;
    m->state[1] = h_fast;
    m->state[3] = h_slow;

    // Двухфазный recovery (single-spike dIN): open = m³·[(1−f)·h_fast + f·h_slow].
    double slow_frac = (m->state[4] > 0.0f) ? m->state[4] : NAV12DEV_SLOW_FRAC;  // калибруемая доля
    float h_eff = (1.0f - slow_frac) * h_fast + slow_frac * h_slow;
    float open_prob = m_val * m_val * m_val * h_eff;
    double ttx = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    open_prob /= (1.0f + ttx / TTX_IC50_NAV12);
    float pkc = segment_pkc_act(s);
    float pka = segment_pka_act(s);
    open_prob *= (1.0f - PKA_PKC_INHIBIT_NAV12 * (pkc + pka) * 0.5f);
    if (open_prob < 0.0f) open_prob = 0.0f;

    double P_single = ghk_single_channel_P(NAV12DEV_GAMMA_PS, 1.0f, GHK_CBAR_NA_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    double I_Na = I_A * A_TO_PA;

    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpv;
    s->extra_ions[ION_NA] += (I_Na * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT);
    clamp_ions(s->extra_ions, ION_COUNT);

    return -I_Na;
}

BIOCORE_DEVICE_CALLABLE double cav21_current(Modifier* m, Segment* s, float dt) {
    // Cav2.1 (CACNA1A) — P/Q-type Ca²⁺, пресинаптический (триггер выброса).
    // ✅ ДОСТОВЕРНО — быстрая активация (τ_m≈1мс) + медленная инактивация (τ_h≈300мс).
    // τ фиксированы (V-зависимость слабая) — bell-shaped вариант τ(V) см. modifier_helpers.h (CAV21_*).
    // См. notes/modifier_refs/CAV21_140.md.
    float V = s->V_m;
    float area = effective_area(m, s);

    float m_inf = cav21_m_inf(V);
    m->state[0] += (m_inf - m->state[0]) * dt / CAV21_TAU_M_MS;

    float Ca = s->intra_ions[ION_CA] * MM_TO_UM; // µM
    float h_inf = cav21_h_inf(V, Ca);
    m->state[1] += (h_inf - m->state[1]) * dt / CAV21_TAU_H_MS;

    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    // Cav2.1 — пресинаптический (P/Q-type). Ca²⁺-ток идёт ТОЛЬКО в микродомен бутона
    // (bouton_ca_step), НЕ в сому. Соматический Ca-ток удалён: иначе +186 пА держал
    // dIN на деполяризационном плато (+9.3 мВ) после спайка — ломал single-spike реполяризацию.
    return 0.0f;
}

BIOCORE_DEVICE_CALLABLE double cl_current(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — CLC-2 leak; GHK-анион (ионы первичны), γ≈2–3 пС.
    float V = s->V_m;
    float area = effective_area(m, s);

    // Инициализация: канал всегда открыт (чистая утечка)
    if (m->state[0] < 0.5f) m->state[0] = 1.0f;

    // γ (single-channel, пС): state[2] > 0 → переопределение, default CLC2_GAMMA_PS.
    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : CLC2_GAMMA_PS;

    float cl_in  = s->intra_ions[ION_CL];
    float cl_out = s->extra_ions[ION_CL];
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_CL_MM);
    double flux = ghk_anion(V, cl_in, cl_out);
    double I_A = P_single * flux * (m->density * area) * m->state[0];
    double I_Cl = I_A * A_TO_PA;

    float volume = seg_volume(s);
    float charge_per_mole = FARADAY_CONST * volume / 1000.0;
    s->intra_ions[ION_CL] += (I_Cl * dt) / charge_per_mole;   // Cl⁻: += (анион)
    s->extra_ions[ION_CL] -= (I_Cl * dt) / (charge_per_mole * EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT);
    clamp_ions(s->extra_ions, ION_COUNT);

    return -I_Cl;
}

BIOCORE_DEVICE_CALLABLE double metabotropic_current(Modifier* m, Segment* s, float dt) {
    float area = effective_area(m, s);

    // Суммарная активация всеми нейромедиаторами
    float total_nt = 0.0f;
    for (int n = 0; n < NT_COUNT; n++) total_nt += s->neurotransmitter[n];

    // Параметрические переопределения (state[2]=coupling, state[3]=tau)
    double p_coupling = (m->state[2] > 0.0f) ? m->state[2] : 1.0f;
    double p_tau = (m->state[3] > 0.0f) ? m->state[3] : 100.0f;

    // Активация G-белка (state[0]) → effector (state[1])
    float activation = total_nt / (total_nt + 1.0f);
    m->state[0] += (activation - m->state[0]) * dt / p_tau;
    m->state[1] = p_coupling * m->state[0];

    // GIRK K⁺ ток — GHK (как gi_o_ghk_effect), а не Ом.
    double vol = segment_volume(s);
    double cpm = charge_per_mole(vol);
    double P_single = ghk_single_channel_P(GIRK_GAMMA_PS, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(s->V_m, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double N = m->density * area;
    double I_K = P_single * flux * N * m->state[1] * A_TO_PA;
    s->intra_ions[ION_K] -= (I_K * dt) / cpm;
    clamp_ions(s->intra_ions, ION_COUNT);
    return -I_K;
}

// Миелин — пассивный модификатор. Не генерирует собственный ток.
// Эффект достигается через segment_has_myelin() + is_ionotropic_type() в kernel_dispatch.h:
// если на сегменте есть MOD_MYELIN, все ионотропные токи блокируются на уровне dispatch.
// Функция существует для заполнения dispatch-таблицы (каждый тип обязан иметь Current-функцию).
BIOCORE_DEVICE_CALLABLE double myelin_modulator(Modifier* m, Segment* s, float dt) {
    (void)m; (void)s; (void)dt;
    return 0.0f;
}

BIOCORE_DEVICE_CALLABLE double synaptic_release_current(Modifier* m, Segment* s, float dt) {
    float area = effective_area(m, s);

    // Ca²⁺-сенсор (Hill n=2, K_d ≈ 0.5 μM = 0.0005 mM)
    float ca = s->intra_ions[ION_CA];
    float ca_sensor = (ca * ca) / (ca * ca + 0.0005f * 0.0005f);

    // Параметрические переопределения (state[3]=g_SK, state[4]=fac_tau, state[5]=dep_tau)
    double p_gSK = (m->state[3] > 0.0f) ? m->state[3] : 0.5f;
    double p_fac_tau = (m->state[4] > 0.0f) ? m->state[4] : 100.0f;
    double p_dep_tau = (m->state[5] > 0.0f) ? m->state[5] : 500.0f;

    // Гейтинг: Ca_sensor (быстрый) → facilitation (средний) → depression (медленный)
    m->state[0] += (ca_sensor - m->state[0]) * dt / 1.0f;
    m->state[1] += (m->state[0] - m->state[1]) * dt / p_fac_tau;
    m->state[2] += (m->state[0] - m->state[2]) * dt / p_dep_tau;

    // Открытая вероятность = facilitation / (1 + depression)
    double open_prob = m->state[1] / (1.0f + m->state[2]);

    // SK-подобный K⁺ ток — GHK (как SK 103), а не Ом.
    double vol = segment_volume(s);
    double cpm = charge_per_mole(vol);
    double P_single = ghk_single_channel_P(SK_GAMMA_PS, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(s->V_m, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double N = m->density * area;
    double I_K = P_single * flux * N * open_prob * A_TO_PA;
    s->intra_ions[ION_K] -= (I_K * dt) / cpm;
    clamp_ions(s->intra_ions, ION_COUNT);
    return -I_K;
}

BIOCORE_DEVICE_CALLABLE double physical_sensor_current(Modifier* m, Segment* s, float dt) {
    float area = effective_area(m, s);

    // Параметрическое переопределение чувствительности (state[1]=sensitivity)
    double p_sens = (m->state[1] > 0.0f) ? m->state[1] : 1.0f;

    // 1. Температурная чувствительность (TRPV1 >43°C, TRPM8 <25°C)
    float T = s->temperature;
    float dT_hot  = (T > 37.0f) ? (T - 37.0f) / 6.0f : 0.0f;
    float dT_cold = (T < 33.0f) ? (33.0f - T) / 8.0f : 0.0f;
    float temp_act = dT_hot + dT_cold;

    // 2. Механочувствительность (растяжение сегмента, Piezo1/2)
    float stretch = (s->length - 50.0f) / 50.0f;
    if (stretch < 0.0f) stretch = 0.0f;
    float mech_act = stretch * 0.5f;

    // pH не моделируется (H⁺ отсутствует в ионной модели) — убран Na⁺-суррогат.

    // Суммарный стимул (температура + механо)
    float stimulus = p_sens * (temp_act + mech_act);

    // Гейтинг (общий сенсорный ток, tau=10ms)
    m->state[0] += (stimulus - m->state[0]) * dt / 10.0f;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    // Адаптация: контр-направленная (habituation, state[2]) + со-направленная (sensitization, state[3])
    // Habituation: GRK/arrestin-десенситизация GPCR + Ca²⁺/CaM-инактивация канала
    double tau_hab = (m->state[6] > 0.0f) ? m->state[6] : 2000.0f;  // по умолчанию SA (медленная), 2с
    sensor_habituation_step(m, 2, m->state[0], dt, tau_hab);
    // Sensitization: cAMP/PKA priming (очень медленная), tau=30s
    sensor_sensitization_step(m, 3, m->state[0], dt, 30000.0f);
    double k_hab = (m->state[4] > 0.0f) ? m->state[4] : 0.6f;
    double k_sens = (m->state[5] > 0.0f) ? m->state[5] : 0.2f;
    double gain = sensor_adaptation_gain(m->state[2], m->state[3], k_hab, k_sens);

    // GHK: неселективный катионный канал (Na⁺ 40% / K⁺ 30% / Ca²⁺ 30%) — синхрон с плагином mod_06.
    double vol = segment_volume(s);
    double cpm = charge_per_mole(vol);
    double P_Na, P_K, P_Ca;
    ghk_partial_P(TRP_SENSOR_GAMMA_PS, 0.4f, 0.3f, 0.3f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area * m->state[0] * gain;
    double I_Na = P_Na * ghk_monovalent(s->V_m, s->intra_ions[ION_NA], s->extra_ions[ION_NA]) * N * A_TO_PA;
    float I_K  = (float)(P_K  * ghk_monovalent(s->V_m, s->intra_ions[ION_K],  s->extra_ions[ION_K])  * N * A_TO_PA);
    double I_Ca = P_Ca * ghk_divalent(s->V_m, s->intra_ions[ION_CA], s->extra_ions[ION_CA]) * N * A_TO_PA;
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpm;
    s->intra_ions[ION_K]  -= (I_K  * dt) / cpm;
    s->intra_ions[ION_CA] -= (I_Ca * dt) / (2.0f * cpm);
    clamp_ions(s->intra_ions, ION_COUNT);
    return -(I_Na - I_K + I_Ca);
}

// ==================== CaMKII (Ca²⁺/кальмодулин-зависимая киназа II) ====================
// Ключевой фермент LTP. Активируется Ca²⁺/кальмодулином. Автофосфорилируется (Thr286).
// state[]: [0]=active (0..1), [1]=autonomous (0..1), [2]=Ca_threshold_μM, [3]=tau_act_ms
// Ca²⁺ => CaM => CaMKII активация. Hill n≈4. Автофосфорилирование даёт Ca²⁺-независимую активность.

BIOCORE_DEVICE_CALLABLE double camkii_current(Modifier* m, Segment* s, float dt) {
    float Ca = s->intra_ions[ION_CA]; // mM
    float Ca_uM = Ca * MM_TO_UM;       // → μM
    double Kd = (m->state[2] > 0.0f) ? m->state[2] : 1.0f;   // μM (HIGH threshold)
    double tau = (m->state[3] > 0.0f) ? m->state[3] : 2000.0f; // ms (VERY slow)
    double autophos_rate = (m->state[5] > 0.0f) ? m->state[5] : 0.0005f; // slow autophos
    dt *= m->density;  // enzyme_amount: концентрация фермента → скорость каскада

    // Hill-активация: Ca²⁺ связывается с кальмодулином (n=4, высокая кооперативность)
    float Ca_n = Ca_uM * Ca_uM * Ca_uM * Ca_uM;
    float CaMKII_inf = Ca_n / (Ca_n + Kd*Kd*Kd*Kd + 1e-9f);

    // Динамика: быстрая Ca²⁺-зависимая активация + медленная автофосфориляция
    m->state[0] += (CaMKII_inf - m->state[0]) * dt / tau;
    m->state[1] += (m->state[0] * autophos_rate - m->state[1] * 0.0001f) * dt;

    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    // Ток не генерирует — только обновляет state[]
    return 0.0f;
}

// ==================== Calcineurin (PP2B, Ca²⁺/кальмодулин-зависимая фосфатаза) ==========
// Ключевой фермент LTD. Более высокое сродство к Ca²⁺/CaM, чем CaMKII — активируется
// при МЕНЬШИХ [Ca²⁺]. Нет автофосфорилирования — быстрее деактивируется.
// state[]: [0]=active (0..1), [1]=Ca_threshold_μM, [2]=tau_act_ms

BIOCORE_DEVICE_CALLABLE double calcineurin_current(Modifier* m, Segment* s, float dt) {
    float Ca = s->intra_ions[ION_CA]; // mM
    float Ca_uM = Ca * MM_TO_UM;       // → μM
    double Kd = (m->state[1] > 0.0f) ? m->state[1] : 0.05f;   // μM (VERY sensitive — LOW threshold!)
    double tau = (m->state[2] > 0.0f) ? m->state[2] : 300.0f;  // ms (faster than CaMKII)
    dt *= m->density;  // enzyme_amount: концентрация фермента → скорость каскада

    // Михаэлис-Ментен (n_Hill≈2-3 для кальциневрина)
    float Ca_n = Ca_uM * Ca_uM;
    float CaN_inf = Ca_n / (Ca_n + Kd*Kd + 1e-9f);

    m->state[0] += (CaN_inf - m->state[0]) * dt / tau;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    return 0.0f;
}

// ==================== L-TYPE Ca²⁺ CHANNEL (Cav1.x, HVA) ============================
// Высокопороговый (активация >-30mV), медленная инактивация, дигидропиридин-чувств.
// Критичен для STDP: добавляет Ca²⁺ при постспайке, усиливает CaMKII.
// state[]: [0]=m, [1]=h (slow inactivation), [2]=g_Ca, [3]=V½_m, [4]=k_m
// Порог активации около -25mV (выше чем у обычного Ca²⁺)

BIOCORE_DEVICE_CALLABLE double ca_l_type_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double Vhalf = (m->state[3] != 0.0f) ? -fabs(m->state[3]) : -11.0f; // HVA (Cav1.2)
    double k = (m->state[4] > 0.0f) ? m->state[4] : 6.9f;  // slope (Cav1.2, 2 мМ Ca²⁺)
    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : 25.0f;  // γ (пС, L-type)

    // Активация: Boltzmann, высокий порог
    float m_inf = 1.0f / (1.0f + exp(-(V - Vhalf) / (k + 1e-6f)));
    float tau_m = 3.0f; // ms — быстрая активация
    m->state[0] += (m_inf - m->state[0]) * dt / tau_m;

    // VDI (voltage-dependent inactivation) — медленная, V½=−37.3 (Cav1.2)
    float vdi_inf = 1.0f / (1.0f + exp((V + 37.3f) / 8.0f));
    m->state[1] += (vdi_inf - m->state[1]) * dt / CAV12_TAU_VDI_MS;

    // CDI (Ca²⁺-dependent inactivation) — быстрая, Kd=1 µM (кальмодулин)
    float Ca = s->intra_ions[ION_CA] * MM_TO_UM; // μM
    float cdi_inf = 1.0f / (1.0f + Ca / CAV12_CDI_KD_UM);
    m->state[5] += (cdi_inf - m->state[5]) * dt / CAV12_TAU_CDI_MS;

    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[5] < 0.0f) m->state[5] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;
    if (m->state[5] > 1.0f) m->state[5] = 1.0f;

    // GHK для Ca²⁺ (z=2), P в см/с (паритет с плагином mod_84)
    float ca_in  = s->intra_ions[ION_CA];
    float ca_out = s->extra_ions[ION_CA];
    double P_single = ghk_single_channel_P(gamma_ps, 2.0f, GHK_CBAR_CA_MM);
    double open_prob = m->state[0] * m->state[0] * m->state[1] * m->state[5]; // m²·h_VDI·h_CDI (L-type)
    // PKA (из MOD_CAMP_PKA) фосфорилирует Cav1.2 Ser1928 → рост I_Ca (до +100%).
    float pka_boost = 1.0f + CAV12_PKA_BOOST * segment_pka_act(s);
    double flux = ghk_divalent(V, ca_in, ca_out);
    double I_A = P_single * flux * (m->density * area) * open_prob * pka_boost;
    double I_Ca = I_A * A_TO_PA;

    double vol = seg_volume(s);
    double cpm = charge_per_mole(vol);
    s->intra_ions[ION_CA] -= (I_Ca * dt) / (2.0f * cpm);
    s->extra_ions[ION_CA] -= (I_Ca * dt) / (2.0f * cpm * EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT);
    clamp_ions(s->extra_ions, ION_COUNT);

    return -I_Ca;
}

BIOCORE_DEVICE_CALLABLE double cav14_current(Modifier* m, Segment* s, float dt) {
    // Cav1.4 (CACNA1F) — L-type Ca²⁺, фоторецептор. γ=4 пС, БЕЗ CDI (ICDI), медленная VDI.
    // ✅ ДОСТОВЕРНО — градуальный sustained-выброс. CaBP4 связывает IQ → снимает ингибицию ICDI/CTM
    // → ΔV½=−10 мВ + boost×5 (Shaltiel 2012). См. notes/modifier_refs/CAV14_144.md.
    float V = s->V_m;
    float area = effective_area(m, s);
    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : CAV14_GAMMA_PS;

    float bound = segment_cabp4_bound(s);
    float Vhalf = CAV14_VHALF_BARE + CAV14_CABP4_DVHALF * bound;

    float m_inf = cav14_m_inf(V, Vhalf);
    m->state[0] += (m_inf - m->state[0]) * dt / CAV14_TAU_M_MS;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    // GHK для Ca²⁺ (z=2), P из γ. Без инактивации: open_prob = m²·(1+boost·bound).
    float ca_in  = s->intra_ions[ION_CA];
    float ca_out = s->extra_ions[ION_CA];
    double P_single = ghk_single_channel_P(gamma_ps, 2.0f, GHK_CBAR_CA_MM);
    double open_prob = m->state[0] * m->state[0];  // m² (сдвиг V½ от CaBP4 уже учтён в Vhalf)

    double flux = ghk_divalent(V, ca_in, ca_out);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    double I_Ca = I_A * A_TO_PA;

    double vol = seg_volume(s);
    double cpm = charge_per_mole(vol);
    s->intra_ions[ION_CA] -= (I_Ca * dt) / (2.0f * cpm);
    s->extra_ions[ION_CA] -= (I_Ca * dt) / (2.0f * cpm * EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT);
    clamp_ions(s->extra_ions, ION_COUNT);

    return -I_Ca;
}

BIOCORE_DEVICE_CALLABLE double cabp4_current(Modifier* m, Segment* s, float dt) {
    // CaBP4 (calcium-binding protein 4) — модулятор Cav1.4, НЕ канал (ток = 0).
    // ✅ ДОСТОВЕРНО — связывает IQ-мотив Cav1.4, снимает ингибицию ICDI/CTM (Shaltiel 2012).
    // state[0] = bound (доля Cav1.4, связанная CaBP4), насыщается к 1 (изобилен, высокое сродство).
    // См. notes/modifier_refs/CABP4_145.md.
    m->state[0] += (1.0f - m->state[0]) * dt / CABP4_TAU_BIND_MS;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    return 0.0f;
}

BIOCORE_DEVICE_CALLABLE double cacc_current(Modifier* m, Segment* s, float dt) {
    // CaCC/ANO2 (TMEM16B) — Ca²⁺-активируемый Cl⁻-канал. GHK Cl⁻ (ghk_anion, z=−1).
    // ✅ ДОСТОВЕРНО — Ca²⁺ EC50=4 µM, Hill n=2 (Cenedese 2011); Cl⁻-селективен.
    // ⚠️ γ=2 пС — структурная оценка; V-зависимость не моделируется. См. CACC_146.md.
    float V = s->V_m;
    float area = effective_area(m, s);
    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : CACC_GAMMA_PS;

    float ca_um = s->intra_ions[ION_CA] * MM_TO_UM;
    if (ca_um < 0.0f) ca_um = 0.0f;
    float m_inf = cacc_m_inf(ca_um);
    m->state[0] += (m_inf - m->state[0]) * dt / CACC_TAU_MS;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    float cl_in  = s->intra_ions[ION_CL];
    float cl_out = s->extra_ions[ION_CL];
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_CL_MM);
    double flux = ghk_anion(V, cl_in, cl_out);
    double I_A = P_single * flux * (m->density * area) * m->state[0];
    double I_Cl = I_A * A_TO_PA;

    double vol = seg_volume(s);
    double cpm = charge_per_mole(vol);
    s->intra_ions[ION_CL] += (I_Cl * dt) / cpm;  // Cl⁻: += (анион)
    clamp_ions(s->intra_ions, ION_COUNT);

    return -I_Cl;
}

BIOCORE_DEVICE_CALLABLE double glast_current(Modifier* m, Segment* s, float dt) {
    // GLAST/GLT-1 (EAAT1/2, SLC1) — глиальный глутаматный транспортер.
    // 3 Na⁺ + 1 H⁺ + 1 Glu (внутрь), 1 K⁺ (наружу), net +2. См. GLAST_147.md.
    float I = glial_nt_uptake_current(m, s, dt, NT_GLUTAMATE, GLAST_K_M_REL,
                                      GLAST_IMAX_UA_PER_CM2, GLAST_GLU_REL_PER_MM, 3, 0, 1, 2);
    clamp_ions(s->intra_ions, ION_COUNT);
    clamp_ions(s->extra_ions, ION_COUNT);
    return I;
}

BIOCORE_DEVICE_CALLABLE double glyt1_current(Modifier* m, Segment* s, float dt) {
    // GlyT1 (SLC6A9) — глиальный транспортер глицина. 2 Na⁺ + 1 Cl⁻ + Gly (внутрь), net +1.
    float I = glial_nt_uptake_current(m, s, dt, NT_GLYCINE, GLYT1_K_M_REL,
                                      GLYT1_IMAX_UA_PER_CM2, GLYT1_GLY_REL_PER_MM, 2, 1, 0, 1);
    clamp_ions(s->intra_ions, ION_COUNT);
    clamp_ions(s->extra_ions, ION_COUNT);
    return I;
}

BIOCORE_DEVICE_CALLABLE double gat3_current(Modifier* m, Segment* s, float dt) {
    // GAT3 (SLC6A11) — глиальный транспортер GABA. 2 Na⁺ + 1 Cl⁻ + GABA (внутрь), net +1.
    float I = glial_nt_uptake_current(m, s, dt, NT_GABA, GAT3_K_M_REL,
                                      GAT3_IMAX_UA_PER_CM2, GAT3_GABA_REL_PER_MM, 2, 1, 0, 1);
    clamp_ions(s->intra_ions, ION_COUNT);
    clamp_ions(s->extra_ions, ION_COUNT);
    return I;
}

BIOCORE_DEVICE_CALLABLE double eaat3_current(Modifier* m, Segment* s, float dt) {
    // EAAT3/EAAC1 (SLC1A1) — нейрональный глутаматный транспортер (пресинаптический/соматический reuptake).
    // 3 Na⁺ + 1 H⁺ + 1 Glu (внутрь), 1 K⁺ (наружу), net +2. См. EAAT3_183.md (Zerangue & Kavanaugh 1996).
    float I = glial_nt_uptake_current(m, s, dt, NT_GLUTAMATE, EAAT3_K_M_REL,
                                      EAAT3_IMAX_UA_PER_CM2, EAAT3_GLU_REL_PER_MM, 3, 0, 1, 2);
    clamp_ions(s->intra_ions, ION_COUNT);
    clamp_ions(s->extra_ions, ION_COUNT);
    return I;
}

BIOCORE_DEVICE_CALLABLE double glyt2_current(Modifier* m, Segment* s, float dt) {
    // GlyT2 (SLC6A5) — нейрональный транспортер глицина (пресинаптический reuptake).
    // 3 Na⁺ + 1 Cl⁻ + Gly (внутрь), net +2 (Roux & Supplisson 2000). См. GLYT2_184.md.
    float I = glial_nt_uptake_current(m, s, dt, NT_GLYCINE, GLYT2_K_M_REL,
                                      GLYT2_IMAX_UA_PER_CM2, GLYT2_GLY_REL_PER_MM, 3, 1, 0, 2);
    clamp_ions(s->intra_ions, ION_COUNT);
    clamp_ions(s->extra_ions, ION_COUNT);
    return I;
}

BIOCORE_DEVICE_CALLABLE double gat1_current(Modifier* m, Segment* s, float dt) {
    // GAT1 (SLC6A1) — нейрональный транспортер GABA (пресинаптический reuptake).
    // 3 Na⁺ + 1 Cl⁻ + GABA (внутрь), net +2 (Willford 2015). См. GAT1_185.md.
    float I = glial_nt_uptake_current(m, s, dt, NT_GABA, GAT1_K_M_REL,
                                      GAT1_IMAX_UA_PER_CM2, GAT1_GABA_REL_PER_MM, 3, 1, 0, 2);
    clamp_ions(s->intra_ions, ION_COUNT);
    clamp_ions(s->extra_ions, ION_COUNT);
    return I;
}

// ==================== Kv4.2 A-TYPE K⁺ CHANNEL (с фосфорилированием) ================
// A-type K⁺: быстрая активация + быстрая инактивация. Снижает возбудимость дендритов.
// CaMKII фосфорилирует Kv4.2 → снижает проводимость → повышает возбудимость (LTP-поддержка).
// state[]: [0]=m, [1]=h, [2]=γ (пС), [3]=phospho, [4]=V½_m, [5]=k_m
// GHK-поток — биологически корректен для ионных каналов, унифицирован с BK/SK/Kir.

BIOCORE_DEVICE_CALLABLE double kv42_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double Vhalf = (m->state[4] != 0.0f) ? -fabs(m->state[4]) : -10.0f; // Kv4.2 (HVA для K⁺)
    double k = (m->state[5] > 0.0f) ? m->state[5] : 13.5f;  // slope (Kv4.2)
    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : 5.0f;  // γ (пС, Kv4.2)

    // Активация m³ (A-type — быстрая, три гейта). τ_m=1.5 мс — быстрая активация
    // (Migliore 1999), слабо V-зависима; точная bell-shaped τ(V) не найдена → фикс-τ обоснован.
    float m_inf = 1.0f / (1.0f + exp(-(V - Vhalf) / (k + 1e-6f)));
    float tau_m = 1.5f;
    m->state[0] += (m_inf - m->state[0]) * dt / tau_m;

    // Инактивация (быстрая N-type при деполяр.) + де-инактивация (медленная recovery при гиперполяр.)
    float h_inf = 1.0f / (1.0f + exp((V + 65.0f) / 8.0f));
    float tau_h = kv42_h_tau(V);
    m->state[1] += (h_inf - m->state[1]) * dt / tau_h;

    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    // Чтение CaMKII → фосфорилирование Kv4.2
    float camkii = 0.0f;
    for (uint8_t j = 0; j < s->modifier_count; j++)
        if (s->modifiers[j].type == MOD_CAMKII)
            camkii = s->modifiers[j].state[0] + s->modifiers[j].state[1];
    float phospho_rate = (camkii > 0.1f) ? 0.0005f : 0.0f;
    (void)phospho_rate;
    m->state[3] += (camkii * 0.5f - m->state[3]) * dt / 5000.0f; // медленно
    if (m->state[3] < 0.0f) m->state[3] = 0.0f;
    if (m->state[3] > 1.0f) m->state[3] = 1.0f;

    // Фосфорилирование снижает γ (0-70%)
    double gamma_eff = gamma_ps * (1.0f - m->state[3] * 0.7f);
    float K_i = s->intra_ions[ION_K];  // моль/м³
    float K_o = s->extra_ions[ION_K];
    double P_single = ghk_single_channel_P(gamma_eff, 1.0f, GHK_CBAR_K_MM);
    double open_prob = m->state[0] * m->state[0] * m->state[0] * m->state[1]; // m³·h
    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    double I_K = I_A * A_TO_PA;

    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);

    return -I_K;
}

BIOCORE_DEVICE_CALLABLE double kv43_current(Modifier* m, Segment* s, float dt) {
    // Kv4.3 (KCND3) — A-type K⁺, быстрая N-type инактивация. γ=4.4пС, V½ −31.7/−47.4.
    float V = s->V_m;
    float area = effective_area(m, s);
    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : KV43_GAMMA_PS;
    double m_val = m->state[0], h_val = m->state[1];
    float m_inf = boltzmann_act(V, KV43_VHALF_M, KV43_K_M);
    float h_inf = boltzmann_inact(V, KV43_VHALF_H, KV43_K_H);
    float tau_m = KV43_TAU_M_MS;
    float tau_h = kv43_h_tau(V);
    float sub_dt = dt / 10.0f;
    for (int step = 0; step < 10; step++) {
        m_val += (m_inf - m_val) * sub_dt / tau_m;
        h_val += (h_inf - h_val) * sub_dt / tau_h;
    }
    if (m_val < 0.0f) m_val = 0.0f;
    if (m_val > 1.0f) m_val = 1.0f;
    if (h_val < 0.0f) h_val = 0.0f;
    if (h_val > 1.0f) h_val = 1.0f;
    m->state[0] = m_val; m->state[1] = h_val;
    float open_prob = m_val * m_val * m_val * h_val;
    // Ингибиция (фармакология): 4-AP (state[3], µM) + HpTx3 (state[4], нМ)
    double ap4 = (m->state[3] > 0.0f) ? m->state[3] : 0.0f;
    double hptx = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;
    open_prob /= (1.0f + ap4 / KV43_4AP_IC50_UM) * (1.0f + hptx / KV43_HPTX3_IC50_NM);
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    double I_K = I_A * A_TO_PA;
    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);
    return -I_K;
}

BIOCORE_DEVICE_CALLABLE double kv32_current(Modifier* m, Segment* s, float dt) {
    // Kv3.2 (KCNC2) — high-threshold K⁺, non-inactivating. γ=15пС, V½ +10.
    float V = s->V_m;
    float area = effective_area(m, s);
    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : KV32_GAMMA_PS;
    double m_val = m->state[0];
    float m_inf = boltzmann_act(V, KV32_VHALF_M, KV32_K_M);
    float tau_m = KV32_TAU_M_MS;
    m_val += (m_inf - m_val) * dt / tau_m;
    if (m_val < 0.0f) m_val = 0.0f;
    if (m_val > 1.0f) m_val = 1.0f;
    m->state[0] = m_val;
    float open_prob = m_val * m_val * m_val;
    // Ингибиция (фармакология): TEA (state[3], µM) + 4-AP (state[4], µM)
    double tea = (m->state[3] > 0.0f) ? m->state[3] : 0.0f;
    double ap4 = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;
    open_prob /= (1.0f + tea / KV32_TEA_IC50_UM) * (1.0f + ap4 / KV32_4AP_IC50_UM);
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    double I_K = I_A * A_TO_PA;
    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);
    return -I_K;
}

BIOCORE_DEVICE_CALLABLE double kv33_current(Modifier* m, Segment* s, float dt) {
    // Kv3.3 (KCNC3) — high-threshold K⁺, A-type (медленная инактивация). γ=35пС, V½ +1.5/−27.9.
    float V = s->V_m;
    float area = effective_area(m, s);
    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : KV33_GAMMA_PS;
    double m_val = m->state[0], h_val = m->state[1];
    float m_inf = boltzmann_act(V, KV33_VHALF_M, KV33_K_M);
    float h_inf = boltzmann_inact(V, KV33_VHALF_H, KV33_K_H);
    float tau_m = KV33_TAU_M_MS;
    float tau_h = KV33_TAU_H_MS;
    m_val += (m_inf - m_val) * dt / tau_m;
    h_val += (h_inf - h_val) * dt / tau_h;
    if (m_val < 0.0f) m_val = 0.0f;
    if (m_val > 1.0f) m_val = 1.0f;
    if (h_val < 0.0f) h_val = 0.0f;
    if (h_val > 1.0f) h_val = 1.0f;
    m->state[0] = m_val; m->state[1] = h_val;
    float open_prob = m_val * m_val * m_val * h_val;
    // Ингибиция (фармакология): TEA (state[3], µM) + 4-AP (state[4], µM)
    double tea = (m->state[3] > 0.0f) ? m->state[3] : 0.0f;
    double ap4 = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;
    open_prob /= (1.0f + tea / KV33_TEA_IC50_UM) * (1.0f + ap4 / KV33_4AP_IC50_UM);
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    double I_K = I_A * A_TO_PA;
    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);
    return -I_K;
}

BIOCORE_DEVICE_CALLABLE double kv22_current(Modifier* m, Segment* s, float dt) {
    // Kv2.2 (KCNB2) — медленный delayed rectifier, медленнее Kv2.1. γ=14.5пС, V½ +13.2/−16.6.
    float V = s->V_m;
    float area = effective_area(m, s);
    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : KV22_GAMMA_PS;
    double m_val = m->state[0], h_val = m->state[1];
    float m_inf = boltzmann_act(V, KV22_VHALF_M, KV22_K_M);
    float h_inf = boltzmann_inact(V, KV22_VHALF_H, KV22_K_H);
    float tau_m = KV22_TAU_M_MS;
    float tau_h = KV22_TAU_H_MS;
    m_val += (m_inf - m_val) * dt / tau_m;
    h_val += (h_inf - h_val) * dt / tau_h;
    if (m_val < 0.0f) m_val = 0.0f;
    if (m_val > 1.0f) m_val = 1.0f;
    if (h_val < 0.0f) h_val = 0.0f;
    if (h_val > 1.0f) h_val = 1.0f;
    m->state[0] = m_val; m->state[1] = h_val;
    float open_prob = m_val * m_val * m_val * h_val;
    // Ингибиция (фармакология): TEA (state[3], мМ) + 4-AP (state[4], мМ)
    double tea = (m->state[3] > 0.0f) ? m->state[3] : 0.0f;
    double ap4 = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;
    open_prob /= (1.0f + tea / KV22_TEA_IC50_MM) * (1.0f + ap4 / KV22_4AP_IC50_MM);
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    double I_K = I_A * A_TO_PA;
    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);
    return -I_K;
}

BIOCORE_DEVICE_CALLABLE double kv14_current(Modifier* m, Segment* s, float dt) {
    // Kv1.4 (KCNA4) — A-type K⁺, N-type инактивация (N-ball). γ=9.3пС, V½ −21.7/−51.8.
    float V = s->V_m;
    float area = effective_area(m, s);
    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : KV14_GAMMA_PS;
    double m_val = m->state[0], h_val = m->state[1];
    float m_inf = boltzmann_act(V, KV14_VHALF_M, KV14_K_M);
    float h_inf = boltzmann_inact(V, KV14_VHALF_H, KV14_K_H);
    float tau_m = KV14_TAU_M_MS;
    float tau_h = kv14_h_tau(V);
    float sub_dt = dt / 10.0f;
    for (int step = 0; step < 10; step++) {
        m_val += (m_inf - m_val) * sub_dt / tau_m;
        h_val += (h_inf - h_val) * sub_dt / tau_h;
    }
    if (m_val < 0.0f) m_val = 0.0f;
    if (m_val > 1.0f) m_val = 1.0f;
    if (h_val < 0.0f) h_val = 0.0f;
    if (h_val > 1.0f) h_val = 1.0f;
    m->state[0] = m_val; m->state[1] = h_val;
    float open_prob = m_val * m_val * m_val * h_val;
    // Ингибиция (фармакология): 4-AP (state[3], µM) + TEA (state[4], мМ — нечувствителен)
    double ap4 = (m->state[3] > 0.0f) ? m->state[3] : 0.0f;
    double tea = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;
    open_prob /= (1.0f + ap4 / KV14_4AP_IC50_UM) * (1.0f + tea / KV14_TEA_IC50_MM);
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    double I_K = I_A * A_TO_PA;
    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);
    return -I_K;
}

BIOCORE_DEVICE_CALLABLE double kv16_current(Modifier* m, Segment* s, float dt) {
    // Kv1.6 (KCNA6) — delayed rectifier, non-inactivating. γ=8.2пС, V½ −8.1.
    float V = s->V_m;
    float area = effective_area(m, s);
    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : KV16_GAMMA_PS;
    double m_val = m->state[0];
    float m_inf = boltzmann_act(V, KV16_VHALF_M, KV16_K_M);
    float tau_m = KV16_TAU_M_MS;
    m_val += (m_inf - m_val) * dt / tau_m;
    if (m_val < 0.0f) m_val = 0.0f;
    if (m_val > 1.0f) m_val = 1.0f;
    m->state[0] = m_val;
    float open_prob = m_val * m_val * m_val;
    // Ингибиция (фармакология): TEA (state[3], мМ) + 4-AP (state[4], мМ)
    double tea = (m->state[3] > 0.0f) ? m->state[3] : 0.0f;
    double ap4 = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;
    open_prob /= (1.0f + tea / KV16_TEA_IC50_MM) * (1.0f + ap4 / KV16_4AP_IC50_MM);
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    double I_K = I_A * A_TO_PA;
    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);
    return -I_K;
}

// ==================== Kv3.1 (fast-spiking K⁺) =====================
BIOCORE_DEVICE_CALLABLE double kv31_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float Vhalf = 10.0f;    // Kv3.1 высокопороговая активация (Rudy & McBain 2001)
    float k     = 10.0f;    // slope
    float gamma_ps = 27.0f; // γ (пС, Grissmer 1992)

    // Активация m³ (fast-spiking). τ_m=1 мс — быстрая активация (Rudy & McBain 2001),
    // слабо V-зависима в рабочем диапазоне; точная bell-shaped τ(V) не найдена → фикс-τ обоснован.
    float m_inf = 1.0f / (1.0f + exp(-(V - Vhalf) / (k + 1e-6f)));
    float tau_m = 1.0f;
    m->state[0] += (m_inf - m->state[0]) * dt / tau_m;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    // C-type медленная инактивация (τ~2 с, V½≈−30 мВ) — Klemic 1998 J Gen Physiol
    float h_inf = 1.0f / (1.0f + exp((V + 30.0f) / 6.0f));
    float tau_h = 2000.0f;  // мс (медленная)
    m->state[1] += (h_inf - m->state[1]) * dt / tau_h;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    float K_i = s->intra_ions[ION_K];
    float K_o = s->extra_ions[ION_K];
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double open_prob = m->state[0] * m->state[0] * m->state[0] * m->state[1]; // m³·h_slow

    // #29 TEA-блок (внешний сайт Y407): IC50 = 0.2 мМ (Rudy & McBain 2001; Wang 1998). state[5]=TEA мМ.
    double tea = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    open_prob /= (1.0f + tea / 0.2f);

    // #30 PKC фосфорилирует Ser503 (Kv3.1b) → подавление АМПЛИТУДЫ (НЕ V½). Macica 2003; Song & Kaczmarek 2006.
    // Полная динамика фосфо-состояния pSer503 (state[2]): фосфо (PKC) − дефосфо (PP1/PP2A).
    // Фосфатаза Ser503 = PP1/PP2A (НЕ кальциневрин — циклоспорин без эффекта), конкретный фермент НЕ разделён (Song 2005 Nat Neurosci; Song & Kaczmarek 2006 JBC).
    // τ дефосфо ~сек (быстрое), τ рефосфо 1-5 мин (PKC-δ/ε/конв.) — точных τ нет → «нужна лит-ра».
    // V½-сдвиг >20 мВ делает CK2 (Macica & Kaczmarek 2001), НЕ PKC — отдельный механизм, НЕ реализован.
    float pkc = segment_pkc_act(s);
    float k_phos   = 5.6e-6f;  // PKC-рефосфорилирование, τ~180 с → «нужна лит-ра»
    float k_dephos = 1e-4f;    // PP1/PP2A-дефосфорилирование, τ~10 с → «нужна лит-ра»
    m->state[2] += (k_phos * pkc * (1.0f - m->state[2]) - k_dephos * m->state[2]) * dt;
    if (m->state[2] < 0.0f) m->state[2] = 0.0f;
    if (m->state[2] > 1.0f) m->state[2] = 1.0f;
    open_prob *= (1.0f - 0.27f * m->state[2]);  // f_max ~27% подавления (неподтверждённое, Macica 2003)

    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    double I_K = I_A * A_TO_PA;

    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);

    return -I_K;
}

// ==================== Kv1.3 (delayed-rectifier K⁺, KCNA3) =====================
// γ=14–24 пС (Fadool 2011); V½≈−30 мВ (⚠️ типовое Kv1.x — точная Kv1.3 в откр. лит-ре не найдена); m⁴, GHK K⁺.
BIOCORE_DEVICE_CALLABLE double kv13_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float Vhalf = -30.0f;   // ⚠️ Kv1.1 −30 / Kv1.2 −27; точная Kv1.3 V½ не найдена — взято типовое
    float k     = 6.0f;     // slope (Kv1.1)
    float gamma_ps = 19.0f; // γ 14–24 пС (среднее, Fadool 2011)

    // Активация m⁴ (delayed rectifier), τ_m=5 мс (⚠️ из Kv1.1 — Kv1.3 точная не найдена)

    float m_inf = 1.0f / (1.0f + exp(-(V - Vhalf) / (k + 1e-6f)));
    float tau_m = kv_dr_m_tau(V, Vhalf);
    m->state[0] += (m_inf - m->state[0]) * dt / tau_m;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    // C-type медленная инактивация (τ~3 с, V½≈−40) — Kv1.x slow inactivation (⚠️ Kv1.3 точная не найдена, взято Kv1.x)
    float h_inf = 1.0f / (1.0f + exp((V + 40.0f) / 6.0f));
    float tau_h = 3000.0f;
    m->state[1] += (h_inf - m->state[1]) * dt / tau_h;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    float K_i = s->intra_ions[ION_K];
    float K_o = s->extra_ions[ION_K];
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double open_prob = m->state[0] * m->state[0] * m->state[0] * m->state[0] * m->state[1]; // m⁴·h_slow

    // Фармакология: ShK-186 (state[5], пМ) — селективный Kv1.3 блокатор
    double shk = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    open_prob /= (1.0f + shk / SHK_IC50_KV13);

    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    double I_K = I_A * A_TO_PA;

    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);

    return -I_K;
}

// ==================== CNG (циклический нуклеотид-зависимый, CNGA2/A4/B1b) =====================
// Лиганд cAMP (state[4], µM — вход от Gs-каскада, follow-up); Po=Hill([cAMP], Kd=35µM, n=1.5); Na⁺/Ca²⁺ (PCa/PNa≈3). γ=13 пС.
BIOCORE_DEVICE_CALLABLE double cng_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double cAMP = m->state[4];  // внешний вход (по умолчанию)
    for (uint8_t j = 0; j < s->modifier_count; j++)
        if (s->modifiers[j].type == MOD_CAMP_PKA) { cAMP = s->modifiers[j].state[0]; break; }  // честный пул cAMP (MOD_CAMP_PKA 135)
    if (cAMP < 0.0f) cAMP = 0.0f;
    float Kd = 35.0f;   // µM (CNGA2 гомомер; гетеромер CNGA2/A4/B1b K½ ниже ~2×)
    float n  = 1.5f;    // Hill (cAMP)
    float Po_inf = pow(cAMP, n) / (pow(Kd, n) + pow(cAMP, n));
    m->state[0] += (Po_inf - m->state[0]) * dt / 2.0f;  // активация ~2 мс (быстрая)
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    // Ca²⁺/CaM обратная связь (ингибиция, адаптация): Ca²⁺ через CNG → CaM → CNG закрывается
    float Ca_uM = s->intra_ions[ION_CA] * 1000.0f;  // mM → µM
    float K_cam = 1.0f;  // µM (CaM Kd для Ca²⁺)
    float CaM = pow(Ca_uM, 4.0f) / (pow(K_cam, 4.0f) + pow(Ca_uM, 4.0f));  // Hill 4
    double open_prob = m->state[0] / (1.0f + 2.0f * CaM);  // CaM ингибирует CNG (до ~1/3)

    double P_Na, P_K, P_Ca;
    ghk_partial_P(13.0f, 1.0f, 0.0f, 3.0f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double flux_Ca = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    double I_Na = P_Na * flux_Na * N * open_prob * A_TO_PA;
    double I_Ca = P_Ca * flux_Ca * N * open_prob * A_TO_PA;

    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpv;
    s->extra_ions[ION_NA] += (I_Na * dt) / (cpv * EXTRA_VOLUME_RATIO);
    s->intra_ions[ION_CA] -= (I_Ca * dt) / cpv;
    s->extra_ions[ION_CA] += (I_Ca * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);
    clamp_ions(s->intra_ions, ION_COUNT);

    return -(I_Na + I_Ca);
}

// ==================== Kv1.1 (delayed-rectifier K⁺, KCNA1) =====================
// γ=9–14 пС (Grissmer 1994); V½ −30 мВ; m⁴ + C-type медленная инактивация (τ~3 с); α-DTX/TEA фарма.
BIOCORE_DEVICE_CALLABLE double kv11_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float Vhalf = -30.0f;    // Kv1.1 (Grissmer 1994)
    float k     = 6.0f;
    float gamma_ps = 10.0f;  // γ 9–14 пС

    float m_inf = 1.0f / (1.0f + exp(-(V - Vhalf) / (k + 1e-6f)));
    float tau_m = kv_dr_m_tau(V, Vhalf);
    m->state[0] += (m_inf - m->state[0]) * dt / tau_m;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    float h_inf = 1.0f / (1.0f + exp((V + 40.0f) / 6.0f));
    float tau_h = 3000.0f;   // C-type медленная (2–4 с)
    m->state[1] += (h_inf - m->state[1]) * dt / tau_h;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    double open_prob = m->state[0] * m->state[0] * m->state[0] * m->state[0] * m->state[1]; // m⁴·h_slow

    // Фармакология: α-DTX (state[5], нМ) + TEA (state[6], µМ) — концентрационный блок
    double dtx = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    double tea = (m->state[6] > 0.0f) ? m->state[6] : 0.0f;
    open_prob /= (1.0f + dtx / DTX_IC50_KV11) * (1.0f + tea / TEA_IC50_KV1X);

    float K_i = s->intra_ions[ION_K];
    float K_o = s->extra_ions[ION_K];
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    double I_K = I_A * A_TO_PA;

    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);

    return -I_K;
}

// ==================== Kv1.2 (delayed-rectifier K⁺, KCNA2) =====================
// γ=14–18 пС; V½ −27 мВ; m⁴ + C-type быстрее (τ~0.4 с); α-DTX/TEA фарма; mGluR1→PKC ингибиция.
BIOCORE_DEVICE_CALLABLE double kv12_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float Vhalf = -27.0f;    // Kv1.2 (Grissmer 1994)
    float k     = 6.0f;
    float gamma_ps = 16.0f;  // γ 14–18 пС

    float m_inf = 1.0f / (1.0f + exp(-(V - Vhalf) / (k + 1e-6f)));
    float tau_m = kv_dr_m_tau(V, Vhalf);
    m->state[0] += (m_inf - m->state[0]) * dt / tau_m;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    float h_inf = 1.0f / (1.0f + exp((V + 40.0f) / 6.0f));
    float tau_h = 400.0f;    // C-type быстрее (<400 мс)
    m->state[1] += (h_inf - m->state[1]) * dt / tau_h;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    double open_prob = m->state[0] * m->state[0] * m->state[0] * m->state[0] * m->state[1]; // m⁴·h_slow

    // PKC-каскад (mGluR1→Gq→PLC→DAG→PKC→тирозин-фосфорилирование→эндоцитоз): segment_pkc_act(s)
    float pkc = segment_pkc_act(s);
    open_prob *= (1.0f - PKC_KV12_INHIBIT * pkc);
    if (open_prob < 0.0f) open_prob = 0.0f;

    // Фармакология: α-DTX (state[5], нМ) + TEA (state[6], µМ)
    double dtx = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    double tea = (m->state[6] > 0.0f) ? m->state[6] : 0.0f;
    open_prob /= (1.0f + dtx / DTX_IC50_KV12) * (1.0f + tea / TEA_IC50_KV1X);

    float K_i = s->intra_ions[ION_K];
    float K_o = s->extra_ions[ION_K];
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    double I_K = I_A * A_TO_PA;

    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);

    return -I_K;
}

// ==================== Cav2.3 (R-type Ca²⁺, CACNA1E) =====================
// γ=14 пС (Randall & Tsien 1995); HVA m² V½ −25 (⚠️ точная не найдена); медленная частичная инактивация; слабая CDI; SNX-482 фарма.
BIOCORE_DEVICE_CALLABLE double cav23_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float Vhalf = -25.0f;    // HVA (⚠️ точная не найдена)
    float k     = 6.0f;
    float gamma_ps = 14.0f;

    // SNX-482 (state[5], нМ): сдвиг V½ активации +70 мВ (не поровая окклюзия)
    double snx = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    float snx_occ = snx / (snx + SNX482_IC50);
    float Vhalf_eff = Vhalf + SNX482_VHALF_SHIFT * snx_occ;

    float m_inf = 1.0f / (1.0f + exp(-(V - Vhalf_eff) / (k + 1e-6f)));
    float tau_m = 2.0f;
    m->state[0] += (m_inf - m->state[0]) * dt / tau_m;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    // Двухкомпонентная инактивация: fast (~25 мс) + slow (~100 мс), V½=−60 (k=6).
    float h_inf = 1.0f / (1.0f + exp((V + 60.0f) / 6.0f));
    m->state[1] += (h_inf - m->state[1]) * dt / CAV23_TAU_H_FAST_MS;
    m->state[2] += (h_inf - m->state[2]) * dt / CAV23_TAU_H_SLOW_MS;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;
    if (m->state[2] < 0.0f) m->state[2] = 0.0f;
    if (m->state[2] > 1.0f) m->state[2] = 1.0f;

    double h_eff = CAV23_FAST_WEIGHT * m->state[1] + (1.0f - CAV23_FAST_WEIGHT) * m->state[2];
    double open_prob = m->state[0] * m->state[0] * h_eff; // m²·(fast·h_f + slow·h_s)

    // CDI (Ca²⁺/CaM, слабая — R-type << L-type)
    float Ca_uM = s->intra_ions[ION_CA] * 1000.0f;
    float CaM = pow(Ca_uM, 4.0f) / (1.0f + pow(Ca_uM, 4.0f));
    open_prob *= (1.0f - CDI_CAV23 * CaM);
    if (open_prob < 0.0f) open_prob = 0.0f;

    float Ca_i = s->intra_ions[ION_CA];
    float Ca_o = s->extra_ions[ION_CA];
    double P_single = ghk_single_channel_P(gamma_ps, 2.0f, GHK_CBAR_CA_MM);
    double flux = ghk_divalent(V, Ca_i, Ca_o);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    double I_Ca = I_A * A_TO_PA;

    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_CA] -= (I_Ca * dt) / cpv;
    s->extra_ions[ION_CA] += (I_Ca * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);
    clamp_ions(s->intra_ions, ION_COUNT);

    return -I_Ca;
}

// ==================== Kv3.4 (A-type K⁺, KCNC4) =====================
// V½_акт +19 (k 11.3), V½_инакт −15 (k −7.4); A-type m·h N-ball; γ ~20 пС (⚠️ типовое Kv3); PKC dis-inhibition; 4-AP/TEA.
BIOCORE_DEVICE_CALLABLE double kv34_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float gamma_ps = 20.0f;  // ⚠️ точная не найдена — типовое Kv3

    float m_inf = 1.0f / (1.0f + exp(-(V - 19.0f) / 11.3f));
    float tau_m = kv34_m_tau(V);
    m->state[0] += (m_inf - m->state[0]) * dt / tau_m;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    float h_inf = 1.0f / (1.0f + exp((V + 15.0f) / 7.4f));
    float tau_h = kv34_h_tau(V);
    m->state[1] += (h_inf - m->state[1]) * dt / tau_h;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    // PKC dis-inhibition: PKC фосфорилирует N-ball → убирает инактивацию (h → 1)
    float pkc = segment_pkc_act(s);
    double h_eff = m->state[1] + (1.0f - m->state[1]) * pkc * PKC_KV34_DISINHIBIT;

    // C-type медленная инактивация (общая для Kv3.x, τ~2 с, V½≈−30 мВ) — Klemic 1998
    float hs_inf = 1.0f / (1.0f + exp((V + 30.0f) / 6.0f));
    float tau_hs = 2000.0f;
    m->state[2] += (hs_inf - m->state[2]) * dt / tau_hs;
    if (m->state[2] < 0.0f) m->state[2] = 0.0f;
    if (m->state[2] > 1.0f) m->state[2] = 1.0f;

    double open_prob = m->state[0] * h_eff * m->state[2];  // m · h_fast(N-ball) · h_slow(C-type)

    // 4-AP (state[5], µМ) + TEA (state[6], µМ)
    double ap4 = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    double tea = (m->state[6] > 0.0f) ? m->state[6] : 0.0f;
    open_prob /= (1.0f + ap4 / AP4_IC50_KV34) * (1.0f + tea / TEA_IC50_KV34);

    float K_i = s->intra_ions[ION_K];
    float K_o = s->extra_ions[ION_K];
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    double I_K = I_A * A_TO_PA;

    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);

    return -I_K;
}

// ==================== Kv2.1 (KCNB1): медленный delayed rectifier K⁺ (I_Ks) ====================
// Био: Kv2.1 (KCNB1) — sKDR, медленная активация + U-type инактивация → single-spike (I_Ks,
// Winlove & Roberts 2011/2012). γ=8 пС (PMC2151531), V½_акт=0 (Alexander 2017), U-type (Klemic 1998),
// TEA IC50=10 мМ (IUPHAR). ✅ ДОСТОВЕРНО (2026-09-22) — V½_right=+15/k_right=10 ОЦЕНКА (нужна лит-ра).
BIOCORE_DEVICE_CALLABLE double kv21_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : KV21_GAMMA_PS;

    // Активация m⁴ (медленная)
    float m_inf = kv21_m_inf(V);
    float tau_m = kv21_m_tau(V);
    m->state[0] += (m_inf - m->state[0]) * dt / tau_m;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    // U-type инактивация h (медленная, V-зависимая recovery)
    float h_inf = kv21_h_inf(V);
    float tau_h = kv21_h_tau(V);
    m->state[1] += (h_inf - m->state[1]) * dt / tau_h;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    double open_prob = m->state[0] * m->state[0] * m->state[0] * m->state[0] * m->state[1]; // m⁴·h

    // TEA-блок (state[5], мМ)
    double tea = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    open_prob /= (1.0f + tea / KV21_TEA_IC50_MM);

    float K_i = s->intra_ions[ION_K];
    float K_o = s->extra_ions[ION_K];
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    double I_K = I_A * A_TO_PA;

    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);

    return -I_K;
}

// ==================== GIRK (Kir3.x, Gβγ-активируемый inward-rectifier K⁺) =====================
// γ=30–40 пС (гетеромер Kir3.1/3.2); Gβγ-зависимый (state[4]), НЕ V-зависимый; inward-выпрямление.
BIOCORE_DEVICE_CALLABLE double girk_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double gbg = m->state[4];  // Gβγ активация (0–1)
    if (gbg < 0.0f) gbg = 0.0f; if (gbg > 1.0f) gbg = 1.0f;
    float Po = gbg;
    // tertiapin-Q (state[5], нМ)
    double tert = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    Po /= (1.0f + tert / TERTIAPINQ_IC50_GIRK);
    float gamma_ps = 35.0f;  // γ гетеромер (⚠️ G в нативных не найдена → density этап 6)
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double I_A = P_single * flux * (m->density * area) * Po;
    double I_K = I_A * A_TO_PA;
    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);
    return -I_K;
}

// ==================== KATP (Kir6.2/SUR1, ATP-чувствительный K⁺) =====================
// γ=70–80 пС; ATP-зависимый (state[4]=[ATP] µМ, IC50 15 µМ); слабое inward-выпрямление.
BIOCORE_DEVICE_CALLABLE double katp_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double atp = m->state[4];  // [ATP] µМ
    if (atp < 0.0f) atp = 0.0f;
    float Po = 1.0f / (1.0f + atp / 15.0f);  // IC50 15 µМ (⚠️ гипоталамический SUR1bΔ33 ~86 µМ)
    // глибенкламид (state[5], нМ)
    double glib = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    Po /= (1.0f + glib / GLIBENCLAMIDE_IC50_KATP);
    float gamma_ps = 75.0f;
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double I_A = P_single * flux * (m->density * area) * Po;
    double I_K = I_A * A_TO_PA;
    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);
    return -I_K;
}

// ==================== TRPC4 (неселективный катионный, Gq/PLC) =====================
// γ=30–42 пС; Gq-зависимый (state[4]); Na⁺/Ca²⁺ (PCa/PNa~4).
BIOCORE_DEVICE_CALLABLE double trpc4_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double gq = m->state[4];  // Gq/PLC (0–1)
    if (gq < 0.0f) gq = 0.0f; if (gq > 1.0f) gq = 1.0f;
    float Po = gq;
    double ml = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;  // ML204 (µМ)
    Po /= (1.0f + ml / ML204_IC50_TRPC4);
    double P_Na, P_K, P_Ca;
    ghk_partial_P(40.0f, 1.0f, 0.0f, 4.0f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double flux_Ca = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    double I_Na = P_Na * flux_Na * N * Po * A_TO_PA;
    double I_Ca = P_Ca * flux_Ca * N * Po * A_TO_PA;
    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpv; s->extra_ions[ION_NA] += (I_Na * dt) / (cpv * EXTRA_VOLUME_RATIO);
    s->intra_ions[ION_CA] -= (I_Ca * dt) / cpv; s->extra_ions[ION_CA] += (I_Ca * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT); clamp_ions(s->intra_ions, ION_COUNT);
    return -(I_Na + I_Ca);
}

// ==================== TRPC3 (неселективный катионный, DAG) =====================
// γ=60 пС; DAG-активируемый (state[4]); Na⁺/Ca²⁺ (P_Ca:P_Na=1.62); Pyr3 0.7 µM.
BIOCORE_DEVICE_CALLABLE double trpc3_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double dag = m->state[4];  // DAG (0–1)
    if (dag < 0.0f) dag = 0.0f; if (dag > 1.0f) dag = 1.0f;
    float Po = dag;
    double pyr3 = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;  // Pyr3 (µM)
    Po /= (1.0f + pyr3 / PYR3_IC50_TRPC3);
    double P_Na, P_K, P_Ca;
    ghk_partial_P(TRPC3_GAMMA_PS, 1.0f, 0.0f, TRPC3_PCA_PNA, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double flux_Ca = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    double I_Na = P_Na * flux_Na * N * Po * A_TO_PA;
    double I_Ca = P_Ca * flux_Ca * N * Po * A_TO_PA;
    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpv; s->extra_ions[ION_NA] += (I_Na * dt) / (cpv * EXTRA_VOLUME_RATIO);
    s->intra_ions[ION_CA] -= (I_Ca * dt) / cpv; s->extra_ions[ION_CA] += (I_Ca * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT); clamp_ions(s->intra_ions, ION_COUNT);
    return -(I_Na + I_Ca);
}

// ==================== TRPM7 (211): chanzyme, Mg²⁺-ингибируемый ====================
// γ=40 пС; Na⁺/Ca²⁺ (P_Ca/P_Na=0.34); ингибируется внутриклеточным Mg²⁺ (0.7 мМ, Hill 2).
BIOCORE_DEVICE_CALLABLE double trpm7_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float Mg_i = s->intra_ions[ION_MG];
    float r = Mg_i / TRPM7_MG_IC50_MM;
    float Po = 1.0f / (1.0f + r * r);
    double ns8593 = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;  // NS8593 (µM)
    Po /= (1.0f + ns8593 / NS8593_IC50_TRPM7);
    double P_Na, P_K, P_Ca;
    ghk_partial_P(TRPM7_GAMMA_PS, 1.0f, 0.0f, TRPM7_PCA_PNA, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double flux_Ca = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    double I_Na = P_Na * flux_Na * N * Po * A_TO_PA;
    double I_Ca = P_Ca * flux_Ca * N * Po * A_TO_PA;
    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpv; s->extra_ions[ION_NA] += (I_Na * dt) / (cpv * EXTRA_VOLUME_RATIO);
    s->intra_ions[ION_CA] -= (I_Ca * dt) / cpv; s->extra_ions[ION_CA] += (I_Ca * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT); clamp_ions(s->intra_ions, ION_COUNT);
    return -(I_Na + I_Ca);
}

// ==================== TRPC5 (неселективный катионный, Gq + Ca²⁺) =====================
// γ=41–66 пС; Gq (state[4]) + Ca²⁺-зависимый (EC50 0.5 µМ); Na⁺/Ca²⁺ (PCa/PNa~5).
BIOCORE_DEVICE_CALLABLE double trpc5_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double gq = m->state[4];
    if (gq < 0.0f) gq = 0.0f; if (gq > 1.0f) gq = 1.0f;
    float Ca_uM = s->intra_ions[ION_CA] * 1000.0f;
    float ca_act = Ca_uM / (Ca_uM + 0.5f);  // EC50 0.5 µМ
    float Po = gq * ca_act;
    double hc = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;  // HC-070 (нМ)
    Po /= (1.0f + hc / HC070_IC50_TRPC5);
    double P_Na, P_K, P_Ca;
    ghk_partial_P(50.0f, 1.0f, 0.0f, 5.0f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double flux_Ca = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    double I_Na = P_Na * flux_Na * N * Po * A_TO_PA;
    double I_Ca = P_Ca * flux_Ca * N * Po * A_TO_PA;
    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpv; s->extra_ions[ION_NA] += (I_Na * dt) / (cpv * EXTRA_VOLUME_RATIO);
    s->intra_ions[ION_CA] -= (I_Ca * dt) / cpv; s->extra_ions[ION_CA] += (I_Ca * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT); clamp_ions(s->intra_ions, ION_COUNT);
    return -(I_Na + I_Ca);
}

// ==================== TRPM4 (Ca²⁺-активируемый неселективный, Na⁺/K⁺) =====================
// γ=25 пС; Ca²⁺-зависимый (EC50 0.4 µМ); моновалентный (не Ca).
BIOCORE_DEVICE_CALLABLE double trpm4_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float Ca_uM = s->intra_ions[ION_CA] * 1000.0f;
    float Po = Ca_uM / (Ca_uM + 0.4f);  // EC50 0.4 µМ
    double phen = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;  // 9-phenanthrol (µМ)
    Po /= (1.0f + phen / PHENANTHROL_IC50_TRPM4);
    double P_Na, P_K, P_Ca;
    ghk_partial_P(25.0f, 1.0f, 1.0f, 0.0f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double flux_K = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double I_Na = P_Na * flux_Na * N * Po * A_TO_PA;
    double I_K = P_K * flux_K * N * Po * A_TO_PA;
    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpv; s->extra_ions[ION_NA] += (I_Na * dt) / (cpv * EXTRA_VOLUME_RATIO);
    s->intra_ions[ION_K] -= (I_K * dt) / cpv; s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT); clamp_ions(s->intra_ions, ION_COUNT);
    return -(I_Na + I_K);
}

// ==================== TRPM5 (Ca²⁺-активируемый неселективный, Na⁺/K⁺) =====================
// γ=25 пС; Ca²⁺-зависимый (EC50 0.7 µМ, Hill 2.4); моновалентный.
BIOCORE_DEVICE_CALLABLE double trpm5_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float Ca_uM = s->intra_ions[ION_CA] * 1000.0f;
    float ca_n = pow(Ca_uM, 2.4f);
    float Po = ca_n / (pow(0.7f, 2.4f) + ca_n);  // EC50 0.7 µМ, Hill 2.4
    double P_Na, P_K, P_Ca;
    ghk_partial_P(25.0f, 1.0f, 1.0f, 0.0f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double flux_K = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double I_Na = P_Na * flux_Na * N * Po * A_TO_PA;
    double I_K = P_K * flux_K * N * Po * A_TO_PA;
    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpv; s->extra_ions[ION_NA] += (I_Na * dt) / (cpv * EXTRA_VOLUME_RATIO);
    s->intra_ions[ION_K] -= (I_K * dt) / cpv; s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT); clamp_ions(s->intra_ions, ION_COUNT);
    return -(I_Na + I_K);
}

// ==================== THIK-1 (K2P13.1, фоновая K⁺-утечка) =====================
// γ≈5 пС; voltage/time-independent, non-inactivating; Po фиксирована (⚠️ не опубликована).
BIOCORE_DEVICE_CALLABLE double thik1_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float Po = 0.5f;  // ⚠️ Po не опубликована — константа
    double hal = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;  // галотан (мМ)
    Po /= (1.0f + hal / HALOTHANE_IC50_THIK);
    float gamma_ps = 5.0f;
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double I_A = P_single * flux * (m->density * area) * Po;
    double I_K = I_A * A_TO_PA;
    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);
    return -I_K;
}

// ==================== THIK-2 (K2P12.1, ⚠️ гомомерно нефункционален) =====================
// γ не измерена (гомомер задержан в ЭПР, Rajan 2001) — placeholder ~5 пС.
BIOCORE_DEVICE_CALLABLE double thik2_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float Po = 0.5f;  // placeholder (нефункционален in vivo)
    float gamma_ps = 5.0f;
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double I_A = P_single * flux * (m->density * area) * Po;
    double I_K = I_A * A_TO_PA;
    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);
    return -I_K;
}

// ==================== Cav2.2 (CACNA1B, N-type Ca²⁺) =====================
// γ=2.76 пС (Ca²⁺); HVA m² V½ −20; h β-зависимая; ω-GVIA.
BIOCORE_DEVICE_CALLABLE double cav22_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float Vhalf = -20.0f;   // HVA
    float k = 6.0f;
    float gamma_ps = 2.76f; // γ (физиол. Ca²⁺)

    float m_inf = 1.0f / (1.0f + exp(-(V - Vhalf) / (k + 1e-6f)));
    float tau_m = 1.7f;
    m->state[0] += (m_inf - m->state[0]) * dt / tau_m;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f; if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    float h_inf = 1.0f / (1.0f + exp((V + 60.0f) / 6.0f));
    float tau_h = 150.0f;   // β-зависимая (~116–174 мс)
    m->state[1] += (h_inf - m->state[1]) * dt / tau_h;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f; if (m->state[1] > 1.0f) m->state[1] = 1.0f;
    double open_prob = m->state[0] * m->state[0] * m->state[1]; // m²·h

    double gvia = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;  // ω-GVIA (нМ)
    open_prob /= (1.0f + gvia / GVIA_IC50_CAV22);

    double P_single = ghk_single_channel_P(gamma_ps, 2.0f, GHK_CBAR_CA_MM);
    double flux = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    double I_Ca = I_A * A_TO_PA;
    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_CA] -= (I_Ca * dt) / cpv;
    s->extra_ions[ION_CA] += (I_Ca * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT); clamp_ions(s->intra_ions, ION_COUNT);
    return -I_Ca;
}

// ==================== TASK (фоновая K⁺-утечка, K2P3.1/9.1) =====================
BIOCORE_DEVICE_CALLABLE double task_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float gamma_ps = 30.0f;  // TASK-1/3 γ ~14–38 пС (среднее)
    float Po = 0.3f;         // фоновый open probability (voltage-independent leak)
    // pH-ингибиция (протонный сенсор): кислый pH закрывает (IC50 pH 6.7 — Talley 2000, Duprat 1997)
    double pH = (m->state[4] > 0.0f) ? m->state[4] : 7.4f;
    float Po_eff = Po / (1.0f + pow(10.0f, 6.7f - pH));

    // #31 O₂-чувствительность — МЕТАБОЛИЧЕСКАЯ (митохондриальный сигналинг), НЕ прямой PO₂-гейт.
    // TASK не интринсивно O₂-чувствителен: гипоксия → митохондрии → метаболич. сигнал → закрытие.
    // Buckler 2000: PO₂ 150→40 мм рт.ст. → ток падает ~20%. state[5]=PO₂ (мм рт.ст.), state[6]=метаболич. сигнал.
    double po2 = (m->state[5] > 0.0f) ? m->state[5] : 150.0f;
    float hyp_inf = 40.0f / (40.0f + po2);   // митохондриальный сигнал (K=40 мм рт.ст., каротидное тело)
    m->state[6] += (hyp_inf - m->state[6]) * dt / 1000.0f;  // τ ~1 с (метаболич. адаптация)
    if (m->state[6] < 0.0f) m->state[6] = 0.0f;
    if (m->state[6] > 1.0f) m->state[6] = 1.0f;
    Po_eff *= (1.0f - 0.5f * m->state[6]);   // до ~50% при полной гипоксии; ~25% при PO₂ 40 (экстраполяция)

    float K_i = s->intra_ions[ION_K];
    float K_o = s->extra_ions[ION_K];
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * Po_eff;
    double I_K = I_A * A_TO_PA;

    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);

    return -I_K;
}

// ==================== eCB SYNTHESIS (эндоканнабиноидный синтез) =====================
// Ca²⁺ (через NMDAR или L-type) → PLCβ → DAG → DAGLα → 2-AG
// 2-AG (эндоканнабиноид) высвобождается в синаптическую щель (retrograde).
// state[]: [0]=2-AG_level, [1]=synthesis_rate, [2]=degradation_rate
// Запись: cleft_neurotransmitter[NT_ECB] на ближайшем синапсе (выставляется в step 3)

BIOCORE_DEVICE_CALLABLE double ecb_synthesis_current(Modifier* m, Segment* s, float dt) {
    float Ca = s->intra_ions[ION_CA] * MM_TO_UM; // μM
    double synth_rate = (m->state[1] > 0.0f) ? m->state[1] : 0.01f;
    double degrad_rate = (m->state[2] > 0.0f) ? m->state[2] : 0.005f;
    float dag = segment_dag(s);  // DAG из MOD_PLC_IP3_DAG (PLCβ→DAG→DGLα)
    dt *= m->density;  // enzyme_amount

    // Ca²⁺- и DAG-зависимый синтез 2-AG (Hill n=2, Kd~0.5μM)
    float Ca_n = Ca * Ca;
    float synth = synth_rate * Ca_n / (Ca_n + 0.25f + 1e-9f) * (1.0f + dag);

    m->state[0] += (synth - m->state[0] * degrad_rate) * dt;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 10.0f) m->state[0] = 10.0f;

    // Ток не генерирует — eCB записывается в cleft через simulation_host.cpp step 3
    return 0.0f;
}

// ==================== MAPK/ERK (87) — MAP-киназный каскад консолидации ==================
// Ca²⁺→Ras→Raf→MEK→ERK→ядерная транслокация→фосфорилирование CREB
// state[]: [0]=ERK_active, [1]=MEK_active, [2]=Ras_active, [3]=pERK_nuclear
//           [4]=Kd_Ca_uM, [5]=tau_erk_ms, [6]=tau_nuclear_ms, [7]=scratchpad

BIOCORE_DEVICE_CALLABLE double mapk_erk_current(Modifier* m, Segment* s, float dt) {
    float Ca = s->intra_ions[ION_CA] * MM_TO_UM; // uM
    double Kd_Ca = (m->state[4] > 0.0f) ? m->state[4] : 0.5f;
    double tau_erk = (m->state[5] > 0.0f) ? m->state[5] : 5000.0f;
    double tau_nuc = (m->state[6] > 0.0f) ? m->state[6] : 30000.0f;
    dt *= m->density;  // enzyme_amount

    // Ca2+ → Ras (Hill n=3)
    float Ca3 = Ca * Ca * Ca;
    float Kd3 = Kd_Ca * Kd_Ca * Kd_Ca;
    float Ras_inf = Ca3 / (Ca3 + Kd3 + 1e-9f);
    m->state[2] += (Ras_inf - m->state[2]) * dt / 500.0f;

    // Ras → MEK
    m->state[1] += (m->state[2] - m->state[1]) * dt / 1000.0f;

    // MEK → ERK (dual phosphorylation, cooperative)
    double ERK_inf = m->state[1] * m->state[1];
    m->state[0] += (ERK_inf - m->state[0]) * dt / tau_erk;

    // ERK → nuclear translocation
    m->state[3] += (m->state[0] * 0.8f - m->state[3]) * dt / tau_nuc;

    for (int i = 0; i < 4; i++) {
        if (m->state[i] < 0.0f) m->state[i] = 0.0f;
        if (m->state[i] > 1.0f) m->state[i] = 1.0f;
    }

    // Cross-talk: nuclear ERK boosts CREB pCREB on same segment
    for (uint8_t j = 0; j < s->modifier_count; j++) {
        if (s->modifiers[j].type == MOD_NB_CREB_TRANSCRIPTION) {
            s->modifiers[j].state[0] += m->state[3] * 0.01f * dt / 1000.0f;
            if (s->modifiers[j].state[0] > 1.0f) s->modifiers[j].state[0] = 1.0f;
        }
    }
    return 0.0f;
}

// ==================== IKK/NF-kB (88) — путь реконсолидации ============================
// NMDA→Ca²⁺→IKK→фосфорилирование IkB→деградация IkB→NF-κB высвобождается→ядро→Zif268
// state[]: [0]=NFkB_active, [1]=IKK_active, [2]=IkB_level, [3]=pNFkB_nuclear
//           [4]=Kd_Ca_uM, [5]=tau_nfkb_ms, [6]=tau_ikb_ms, [7]=scratchpad

BIOCORE_DEVICE_CALLABLE double ikk_nfkb_current(Modifier* m, Segment* s, float dt) {
    float Ca = s->intra_ions[ION_CA] * MM_TO_UM;
    double Kd_Ca = (m->state[4] > 0.0f) ? m->state[4] : 0.3f;
    double tau_nfkb = (m->state[5] > 0.0f) ? m->state[5] : 10000.0f;
    double tau_ikb = (m->state[6] > 0.0f) ? m->state[6] : 5000.0f;
    dt *= m->density;  // enzyme_amount

    // Ca2+ → IKK
    float IKK_inf = Ca / (Ca + Kd_Ca + 1e-9f);
    m->state[1] += (IKK_inf - m->state[1]) * dt / 2000.0f;

    // IKK → IkB degradation
    m->state[2] += (1.0f - m->state[1] * 0.9f - m->state[2]) * dt / tau_ikb;
    if (m->state[2] < 0.1f) m->state[2] = 0.1f;

    // NF-kB released from IkB
    double NFkB_free = 1.0f - m->state[2];
    m->state[0] += (NFkB_free - m->state[0]) * dt / tau_nfkb;

    // NF-kB nuclear translocation
    m->state[3] += (m->state[0] * 0.7f - m->state[3]) * dt / (tau_nfkb * 2.0f);

    for (int i = 0; i < 4; i++) {
        if (m->state[i] < 0.0f) m->state[i] = 0.0f;
        if (m->state[i] > 1.0f) m->state[i] = 1.0f;
    }

    // NF-kB adds to transcription signal (reconsolidation — different from CREB)
    s->transcription_signal += m->state[3] * 0.1f;
    if (s->transcription_signal > 1.0f) s->transcription_signal = 1.0f;
    return 0.0f;
}

// ==================== mTORC1 (89) — двухключевая активация ============================
// (1) Аминокислоты→Rag→Rheb-GTP; (2) PI3K/AKT→фосфорилирование TSC→Rheb
// Оба нужны для активации. ATP ингибирует через AMPK.
// state[]: [0]=mTORC1_active, [1]=Rheb_GTP, [2]=AKT_active, [3]=AA_sensor
//           [4]=Kd_AA, [5]=tau_mtor_ms, [6]=tau_akt_ms, [7]=scratchpad

BIOCORE_DEVICE_CALLABLE double mtorc1_current(Modifier* m, Segment* s, float dt) {
    double tau_mtor = (m->state[5] > 0.0f) ? m->state[5] : 15000.0f;
    double tau_akt = (m->state[6] > 0.0f) ? m->state[6] : 8000.0f;
    double Kd_AA = (m->state[4] > 0.0f) ? m->state[4] : 0.3f;
    dt *= m->density;  // enzyme_amount

    // Key 1: AA → Rheb-GTP. AA = state[3] (внешний parametric input — нутриентная доступность).
    double AA = m->state[3];
    float Rheb_inf = AA / (AA + Kd_AA + 1e-9f);
    m->state[1] += (Rheb_inf - m->state[1]) * dt / 5000.0f;

    // Key 2: PI3K/AKT
    float AKT_inf = AA * 0.8f;
    m->state[2] += (AKT_inf - m->state[2]) * dt / tau_akt;

    // Both keys: mTORC1 = Rheb * AKT
    double mTOR_inf = m->state[1] * m->state[2];

    // ATP clamp: low ATP inhibits mTORC1
    float ATP_norm = s->ATP_level / 5.0f;
    if (ATP_norm > 1.0f) ATP_norm = 1.0f;
    if (ATP_norm < 0.0f) ATP_norm = 0.0f;
    mTOR_inf *= ATP_norm;

    m->state[0] += (mTOR_inf - m->state[0]) * dt / tau_mtor;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[2] < 0.0f) m->state[2] = 0.0f;
    return 0.0f;
}

// ==================== PROTEIN_SYNTH (90) — синтез белка ================================
// mTORC1→S6K1/S6 + mTORC1→4E-BP→eIF4E — два параллельных пути инициации трансляции
// state[]: [0]=translation_rate, [1]=S6K1_active, [2]=4EBP_phospho, [3]=ribosome_occ
//           [4]=tau_s6k_ms, [5]=tau_4ebp_ms, [6]=tau_translate_ms, [7]=scratchpad

BIOCORE_DEVICE_CALLABLE double protein_synth_current(Modifier* m, Segment* s, float dt) {
    double tau_s6k = (m->state[4] > 0.0f) ? m->state[4] : 10000.0f;
    double tau_4ebp = (m->state[5] > 0.0f) ? m->state[5] : 8000.0f;
    double tau_translate = (m->state[6] > 0.0f) ? m->state[6] : 60000.0f;
    dt *= m->density;  // enzyme_amount

    // Read mTORC1 from same segment
    float mtorc1 = 0.0f;
    for (uint8_t j = 0; j < s->modifier_count; j++)
        if (s->modifiers[j].type == MOD_MTORC1)
            mtorc1 = s->modifiers[j].state[0];

    // mTORC1 → S6K1
    m->state[1] += (mtorc1 - m->state[1]) * dt / tau_s6k;
    // mTORC1 → 4E-BP phosphorylation (releases eIF4E)
    m->state[2] += (mtorc1 - m->state[2]) * dt / tau_4ebp;
    // Combined translation
    double trans_inf = m->state[1] * m->state[2];
    m->state[0] += (trans_inf - m->state[0]) * dt / tau_translate;
    m->state[3] = m->state[0] * 0.9f;

    for (int i = 0; i < 4; i++) {
        if (m->state[i] < 0.0f) m->state[i] = 0.0f;
        if (m->state[i] > 1.0f) m->state[i] = 1.0f;
    }

    // Protein synthesis consumes ATP
    s->ATP_level -= m->state[0] * 0.0001f * dt;
    if (s->ATP_level < 0.0f) s->ATP_level = 0.0f;
    return 0.0f;
}

// ==================== COFILIN (91) — актиновое ремоделирование =========================
// ERK→LIMK→фосфорилирование→инактивация Cofilin; Cofilin разрезает F-actin→G-actin
// state[]: [0]=cofilin_active, [1]=LIMK_active, [2]=pCofilin_ratio, [3]=F_actin_density
//           [4]=tau_cof_ms, [5]=tau_limk_ms, [6]=tau_actin_ms, [7]=scratchpad

BIOCORE_DEVICE_CALLABLE double cofilin_current(Modifier* m, Segment* s, float dt) {
    double tau_cof = (m->state[4] > 0.0f) ? m->state[4] : 5000.0f;
    double tau_limk = (m->state[5] > 0.0f) ? m->state[5] : 8000.0f;
    double tau_actin = (m->state[6] > 0.0f) ? m->state[6] : 30000.0f;
    dt *= m->density;  // enzyme_amount

    // Read ERK from same segment
    float erk = 0.0f;
    for (uint8_t j = 0; j < s->modifier_count; j++)
        if (s->modifiers[j].type == MOD_MAPK_ERK)
            erk = s->modifiers[j].state[0];

    // ERK/RSK → LIMK
    m->state[1] += (erk * 0.8f - m->state[1]) * dt / tau_limk;
    // LIMK → phospho-Cofilin (inactive)
    double pCofilin_target = m->state[1] * 0.9f;
    m->state[2] += (pCofilin_target - m->state[2]) * dt / tau_cof;
    // Active cofilin = 1 - phospho-cofilin
    double cof_target = 1.0f - m->state[2];
    m->state[0] += (cof_target - m->state[0]) * dt / tau_cof;
    // F-actin: polymerization minus severing
    float polymerization = 0.3f;
    double severing = m->state[0] * 0.1f;
    m->state[3] += (polymerization - severing * m->state[3]) * dt / tau_actin;

    for (int i = 0; i < 4; i++) {
        if (m->state[i] < 0.0f) m->state[i] = 0.0f;
        if (m->state[i] > 1.0f) m->state[i] = 1.0f;
    }
    return 0.0f;
}

// ==================== MMP-9 (92) — деградация внеклеточного матрикса ==================
// ERK+NF-κB→MMP-9 транскрипция; TIMP ингибирует; ECM→интегрины
// state[]: [0]=MMP9_active, [1]=TIMP_level, [2]=ECM_density, [3]=integrin_activation
//           [4]=tau_mmp9_ms, [5]=tau_timp_ms, [6]=tau_ecm_ms, [7]=scratchpad

BIOCORE_DEVICE_CALLABLE double mmp9_current(Modifier* m, Segment* s, float dt) {
    double tau_mmp9 = (m->state[4] > 0.0f) ? m->state[4] : 20000.0f;
    double tau_timp = (m->state[5] > 0.0f) ? m->state[5] : 15000.0f;
    double tau_ecm = (m->state[6] > 0.0f) ? m->state[6] : 100000.0f;
    dt *= m->density;  // enzyme_amount

    // MMP-9 transcription depends on ERK + NF-kB
    float erk = 0.0f, nfkb = 0.0f;
    for (uint8_t j = 0; j < s->modifier_count; j++) {
        if (s->modifiers[j].type == MOD_MAPK_ERK) erk = s->modifiers[j].state[3];
        if (s->modifiers[j].type == MOD_IKK_NFKB) nfkb = s->modifiers[j].state[3];
    }
    float mmp9_transcription = (erk + nfkb) * 0.5f;

    // MMP-9 activation
    m->state[0] += (mmp9_transcription - m->state[0]) * dt / tau_mmp9;
    // TIMP anti-correlated
    double timp_target = 1.0f - m->state[0] * 0.5f;
    m->state[1] += (timp_target - m->state[1]) * dt / tau_timp;
    // ECM degradation vs synthesis
    double degradation = m->state[0] * (1.0f - m->state[1]) * 0.01f;
    float synthesis = 0.005f;
    m->state[2] += (synthesis - degradation * m->state[2]) * dt / tau_ecm;
    if (m->state[2] < 0.1f) m->state[2] = 0.1f;
    if (m->state[2] > 1.0f) m->state[2] = 1.0f;
    // Integrin activation (inverse to ECM density)
    double integrin_inf = 1.0f - m->state[2];
    m->state[3] += (integrin_inf - m->state[3]) * dt / 10000.0f;

    for (int i = 0; i < 4; i++) {
        if (m->state[i] < 0.0f) m->state[i] = 0.0f;
        if (m->state[i] > 1.0f) m->state[i] = 1.0f;
    }
    return 0.0f;
}

// ==================== PIR T-type Ca²⁺ (96): Cav3 GHK-поток ====================
BIOCORE_DEVICE_CALLABLE double pir_ttype_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : TCA_GAMMA_PS;  // γ (пС, Cav3.2)
    float V_half_m = TCA_VHALF_M, V_half_h = TCA_VHALF_H, EC50_5HTDA = 0.01f;
    double m_val = m->state[0], h_val = m->state[1];
    float m_inf = boltzmann_act(V, V_half_m, TCA_K_M);
    float h_inf = boltzmann_inact(V, V_half_h, TCA_K_H);
    float tau_h = tca_h_tau(V);
    float sub_dt = dt / 10.0f;
    for (int step = 0; step < 10; step++) {
        m_val += (m_inf - m_val) * sub_dt / 3.0f;
        h_val += (h_inf - h_val) * sub_dt / tau_h;
    }
    if (m_val < 0.0f) m_val = 0.0f;
    if (m_val > 1.0f) m_val = 1.0f;
    if (h_val < 0.0f) h_val = 0.0f;
    if (h_val > 1.0f) h_val = 1.0f;
    m->state[0] = m_val; m->state[1] = h_val;
    float ca_in = s->intra_ions[ION_CA], ca_out = s->extra_ions[ION_CA];  // моль/м³
    double P_single = ghk_single_channel_P(gamma_ps, 2.0f, GHK_CBAR_CA_MM);
    float ht = s->neurotransmitter[NT_SEROTONIN], da = s->neurotransmitter[NT_DOPAMINE];
    double mod = (1.0 - 0.7 / (1.0 + (ht + da) / EC50_5HTDA));
    double flux = ghk_divalent(V, ca_in, ca_out);
    double I_A = P_single * flux * (m->density * area) * mod * (m_val*m_val*m_val*h_val);
    double I_CaT = I_A * A_TO_PA;
    float volume = seg_volume(s);
    float charge_per_mole = FARADAY_CONST * volume / 1000.0;
    s->intra_ions[ION_CA] -= (I_CaT * dt) / (2.0f * charge_per_mole);
    return -I_CaT;
}

// ==================== Cav3.3 (191): T-type Ca²⁺, γ=11 пС, самая медленная кинетика ====================
BIOCORE_DEVICE_CALLABLE double cav33_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : CAV33_GAMMA_PS;  // γ (пС, Cav3.3)
    double m_val = m->state[0], h_val = m->state[1];
    float m_inf = boltzmann_act(V, CAV33_VHALF_M, CAV33_K_M);
    float h_inf = boltzmann_inact(V, CAV33_VHALF_H, CAV33_K_H);
    float tau_m = cav33_m_tau(V);
    float tau_h = cav33_h_tau(V);
    float sub_dt = dt / 10.0f;
    for (int step = 0; step < 10; step++) {
        m_val += (m_inf - m_val) * sub_dt / tau_m;
        h_val += (h_inf - h_val) * sub_dt / tau_h;
    }
    if (m_val < 0.0f) m_val = 0.0f;
    if (m_val > 1.0f) m_val = 1.0f;
    if (h_val < 0.0f) h_val = 0.0f;
    if (h_val > 1.0f) h_val = 1.0f;
    m->state[0] = m_val; m->state[1] = h_val;
    float open_prob = m_val * m_val * m_val * h_val;
    // Ингибиция (фармакология): Ni²⁺/мибефрадил/TTA-P2/Z944 (state[3..6], мультипликативный блок)
    double ni = (m->state[3] > 0.0f) ? m->state[3] : 0.0f;   // Ni²⁺ (µМ)
    double mib = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;   // мибефрадил (µМ)
    double tta = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;   // TTA-P2 (нМ)
    double z944 = (m->state[6] > 0.0f) ? m->state[6] : 0.0f;   // Z944 (нМ)
    open_prob /= (1.0f + ni / CAV33_NI_IC50_UM) * (1.0f + mib / CAV33_MIBEFRADIL_IC50_UM)
               * (1.0f + tta / CAV33_TTAP2_IC50_NM) * (1.0f + z944 / CAV33_Z944_IC50_NM);
    double P_single = ghk_single_channel_P(gamma_ps, 2.0f, GHK_CBAR_CA_MM);
    double flux = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    double I_CaT = I_A * A_TO_PA;
    float volume = seg_volume(s);
    float charge_per_mole = FARADAY_CONST * volume / 1000.0;
    s->intra_ions[ION_CA] -= (I_CaT * dt) / (2.0f * charge_per_mole);
    return -I_CaT;
}

BIOCORE_DEVICE_CALLABLE double cav31_current(Modifier* m, Segment* s, float dt) {
    // Cav3.1 (CACNA1G/α1G) — T-type Ca²⁺, быстрее Cav3.3. γ=7.5пС, V½ −45/−77, Ni²⁺ 250µM.
    float V = s->V_m;
    float area = effective_area(m, s);
    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : CAV31_GAMMA_PS;
    double m_val = m->state[0], h_val = m->state[1];
    float m_inf = boltzmann_act(V, CAV31_VHALF_M, CAV31_K_M);
    float h_inf = boltzmann_inact(V, CAV31_VHALF_H, CAV31_K_H);
    float tau_m = cav31_m_tau(V);
    float tau_h = cav31_h_tau(V);
    float sub_dt = dt / 10.0f;
    for (int step = 0; step < 10; step++) {
        m_val += (m_inf - m_val) * sub_dt / tau_m;
        h_val += (h_inf - h_val) * sub_dt / tau_h;
    }
    if (m_val < 0.0f) m_val = 0.0f;
    if (m_val > 1.0f) m_val = 1.0f;
    if (h_val < 0.0f) h_val = 0.0f;
    if (h_val > 1.0f) h_val = 1.0f;
    m->state[0] = m_val; m->state[1] = h_val;
    float open_prob = m_val * m_val * m_val * h_val;
    double ni = (m->state[3] > 0.0f) ? m->state[3] : 0.0f;
    double mib = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;
    double tta = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    double z944 = (m->state[6] > 0.0f) ? m->state[6] : 0.0f;
    open_prob /= (1.0f + ni / CAV31_NI_IC50_UM) * (1.0f + mib / CAV31_MIBEFRADIL_IC50_UM)
               * (1.0f + tta / CAV31_TTAP2_IC50_NM) * (1.0f + z944 / CAV31_Z944_IC50_NM);
    double P_single = ghk_single_channel_P(gamma_ps, 2.0f, GHK_CBAR_CA_MM);
    double flux = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    double I_CaT = I_A * A_TO_PA;
    float volume = seg_volume(s);
    float charge_per_mole = FARADAY_CONST * volume / 1000.0;
    s->intra_ions[ION_CA] -= (I_CaT * dt) / (2.0f * charge_per_mole);
    return -I_CaT;
}

BIOCORE_DEVICE_CALLABLE double cav32_current(Modifier* m, Segment* s, float dt) {
    // Cav3.2 (CACNA1H/α1H) — T-type Ca²⁺, самая Ni²⁺-чувствительная. γ=9пС, V½ −46/−72, Ni²⁺ 13µM.
    float V = s->V_m;
    float area = effective_area(m, s);
    double gamma_ps = (m->state[2] > 0.0f) ? m->state[2] : CAV32_GAMMA_PS;
    double m_val = m->state[0], h_val = m->state[1];
    float m_inf = boltzmann_act(V, CAV32_VHALF_M, CAV32_K_M);
    float h_inf = boltzmann_inact(V, CAV32_VHALF_H, CAV32_K_H);
    float tau_m = cav32_m_tau(V);
    float tau_h = cav32_h_tau(V);
    float sub_dt = dt / 10.0f;
    for (int step = 0; step < 10; step++) {
        m_val += (m_inf - m_val) * sub_dt / tau_m;
        h_val += (h_inf - h_val) * sub_dt / tau_h;
    }
    if (m_val < 0.0f) m_val = 0.0f;
    if (m_val > 1.0f) m_val = 1.0f;
    if (h_val < 0.0f) h_val = 0.0f;
    if (h_val > 1.0f) h_val = 1.0f;
    m->state[0] = m_val; m->state[1] = h_val;
    float open_prob = m_val * m_val * m_val * h_val;
    double ni = (m->state[3] > 0.0f) ? m->state[3] : 0.0f;
    double mib = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;
    double tta = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    double z944 = (m->state[6] > 0.0f) ? m->state[6] : 0.0f;
    open_prob /= (1.0f + ni / CAV32_NI_IC50_UM) * (1.0f + mib / CAV32_MIBEFRADIL_IC50_UM)
               * (1.0f + tta / CAV32_TTAP2_IC50_NM) * (1.0f + z944 / CAV32_Z944_IC50_NM);
    double P_single = ghk_single_channel_P(gamma_ps, 2.0f, GHK_CBAR_CA_MM);
    double flux = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    double I_CaT = I_A * A_TO_PA;
    float volume = seg_volume(s);
    float charge_per_mole = FARADAY_CONST * volume / 1000.0;
    s->intra_ions[ION_CA] -= (I_CaT * dt) / (2.0f * charge_per_mole);
    return -I_CaT;
}

// ==================== PIR HCN (97): Na⁺/K⁺ GHK-потоки ====================
BIOCORE_DEVICE_CALLABLE double pir_hcn_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = 2.0f * PI * s->radius * s->length;
    // Параметрические переопределения из state[] (паритет с плагином mod_97)
    double V_half = (m->state[1] != 0.0f) ? m->state[1] : HCN_VHALF;
    double k_slope = (m->state[2] > 0.0f)  ? m->state[2] : HCN_K;
    double gamma_ps = (m->state[3] > 0.0f)  ? m->state[3] : HCN2_GAMMA_PS;
    double tau_open = (m->state[4] > 0.0f)  ? m->state[4] : 250.0f;
    double tau_close = (m->state[5] > 0.0f)  ? m->state[5] : 30.0f;
    double EC50_DA = (m->state[6] > 0.0f)  ? m->state[6] : 0.01f;
    float da = s->neurotransmitter[NT_DOPAMINE];
    float V_half_eff = V_half + (1.0f / (1.0f + da / EC50_DA)) * 5.0f;
    double m_val = m->state[0];
    float m_inf = 1.0f / (1.0f + exp((V - V_half_eff) / k_slope));
    float tau_eff = (V < V_half_eff) ? tau_open : tau_close;
    m_val += (m_inf - m_val) * dt / tau_eff;
    if (m_val < 0.0f) m_val = 0.0f;
    if (m_val > 1.0f) m_val = 1.0f;
    m->state[0] = m_val;
    double P_Na, P_K, P_Ca;
    ghk_partial_P(gamma_ps, HCN_P_NA_FRAC, HCN_P_K_FRAC, 0.0f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double I_Na = P_Na * ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]) * N * m_val * A_TO_PA;
    float I_K  = (float)(P_K  * ghk_monovalent(V, s->intra_ions[ION_K],  s->extra_ions[ION_K])  * N * m_val * A_TO_PA);
    float volume = seg_volume(s);
    double cpm = FARADAY_CONST * volume / 1000.0;
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpm;
    s->intra_ions[ION_K]  -= (I_K  * dt) / cpm;
    s->extra_ions[ION_NA] -= (I_Na * dt) / (cpm * EXTRA_VOLUME_RATIO);
    s->extra_ions[ION_K]  += (I_K  * dt) / (cpm * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);
    return -(I_Na - I_K);
}

// ==================== PIR (93): Постингибирующий отскок (сохранён) ==========
// ==================== HUNGER (94): Сенсор голода ====================
BIOCORE_DEVICE_CALLABLE double hunger_sensor_current(Modifier* m, Segment* s, float dt) {
    float atp_norm = s->ATP_level / 5.0f;  // мМ (0..5) → нормировка (как mTORC1)
    if (atp_norm > 1.0f) atp_norm = 1.0f;
    if (atp_norm < 0.0f) atp_norm = 0.0f;
    float hunger = 1.0f / (1.0f + exp(10.0f * (atp_norm - 0.3f)));
    m->state[0] += (hunger - m->state[0]) * dt / 500.0f;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    // Habituation (state[1]): медленная контр-адаптация, tau=300s — AMPK/mTOR-десенситизация
    sensor_habituation_step(m, 1, m->state[0], dt, 300000.0f);
    // Sensitization (state[2]): очень медленная со-адаптация, tau=600s — грелин/лептин priming
    sensor_sensitization_step(m, 2, m->state[0], dt, 600000.0f);
    double k_hab = (m->state[3] > 0.0f) ? m->state[3] : 0.5f;
    double k_sens = (m->state[4] > 0.0f) ? m->state[4] : 0.4f;
    double gain = sensor_adaptation_gain(m->state[1], m->state[2], k_hab, k_sens);
    // GHK: неселективный катионный (Na⁺ 60% / K⁺ 30%) — синхрон с плагином mod_94.
    float area = effective_area(m, s);
    double vol = segment_volume(s);
    double cpm = charge_per_mole(vol);
    double P_Na, P_K, P_Ca;
    ghk_partial_P(TRP_SENSOR_GAMMA_PS, 0.6f, 0.3f, 0.0f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area * m->state[0] * gain;
    double I_Na = P_Na * ghk_monovalent(s->V_m, s->intra_ions[ION_NA], s->extra_ions[ION_NA]) * N * A_TO_PA;
    float I_K  = (float)(P_K  * ghk_monovalent(s->V_m, s->intra_ions[ION_K],  s->extra_ions[ION_K])  * N * A_TO_PA);
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpm;
    s->intra_ions[ION_K]  -= (I_K  * dt) / cpm;
    clamp_ions(s->intra_ions, ION_COUNT);
    return -(I_Na - I_K);
}

// ==================== LIGHT (95): Фоторецептор ====================
BIOCORE_DEVICE_CALLABLE double light_sensor_current(Modifier* m, Segment* s, float dt) {
    double intensity = m->state[1];
    // Habituation (state[2]): контр-адаптация — аррестин/родопсин-киназа, tau_rise=2s, tau_decay=5s
    if (intensity > 0.1f) sensor_habituation_step(m, 2, 1.0f, dt, 2000.0f);
    else m->state[2] -= dt * m->state[2] / 5000.0f;
    if (m->state[2] < 0.0f) m->state[2] = 0.0f;
    if (m->state[2] > 1.0f) m->state[2] = 1.0f;
    // Sensitization (state[3]): со-адаптация — Ca²⁺/рековерин/гуанилатциклаза (GCAP), tau=60s
    sensor_sensitization_step(m, 3, intensity, dt, 60000.0f);
    float k_hab = 0.7f;
    double k_sens = (m->state[4] > 0.0f) ? m->state[4] : 0.5f;
    double adapted_intensity = intensity * (1.0f - k_hab * m->state[2]) * (1.0f + k_sens * m->state[3]);
    if (adapted_intensity < 0.0f) adapted_intensity = 0.0f;
    m->state[0] += (adapted_intensity - m->state[0]) * dt / 50.0f;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    // CNG-канал: открыт в темноте, закрыт на свету. Na⁺ 50% / K⁺ 30% / Ca²⁺ 20% — синхрон с плагином mod_95.
    float area = effective_area(m, s);
    double vol = segment_volume(s);
    double cpm = charge_per_mole(vol);
    double open_prob = 1.0f - m->state[0];
    double P_Na, P_K, P_Ca;
    ghk_partial_P(CNG_GAMMA_PS, 0.5f, 0.3f, 0.2f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area * open_prob;
    double I_Na = P_Na * ghk_monovalent(s->V_m, s->intra_ions[ION_NA], s->extra_ions[ION_NA]) * N * A_TO_PA;
    float I_K  = (float)(P_K  * ghk_monovalent(s->V_m, s->intra_ions[ION_K],  s->extra_ions[ION_K])  * N * A_TO_PA);
    double I_Ca = P_Ca * ghk_divalent(s->V_m, s->intra_ions[ION_CA], s->extra_ions[ION_CA]) * N * A_TO_PA;
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpm;
    s->intra_ions[ION_K]  -= (I_K  * dt) / cpm;
    s->intra_ions[ION_CA] -= (I_Ca * dt) / (2.0f * cpm);
    clamp_ions(s->intra_ions, ION_COUNT);
    return -(I_Na + I_K + I_Ca);
}

// ==================== MOD_ROD_PHOTOTRANSDUCTION (192): родопсин фотокаскад ====================
// Полный каскад палочки: свет → R* (метародопсин II) → трансдуцин (Gt) → PDE6 → cGMP↓ → CNG закрытие.
// CNG открыт в темноте (тёмный ток Na/K/Ca, деполяризация −35..−40 мВ), закрыт на свету (гиперполяр. −60..−70 мВ).
// state[4]=свет (0..1), state[0]=R*, state[1]=Gt*, state[2]=PDE*, state[3]=cGMP (норм.).
// Усиление ~10⁵–10⁶ cGMP/фотон (Leskov 2000); CNG K1/2 20 µM, Hill n=3 (Yau & Nakatani 1985).
BIOCORE_DEVICE_CALLABLE double rod_phototransduction_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    double light = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;
    m->state[0] += (light - m->state[0]) * dt / RHODOPSIN_TAU_R_MS;       // R* (τ~1.5 с)
    m->state[1] += (m->state[0] - m->state[1]) * dt / RHODOPSIN_TAU_GT_MS;  // Gt* (~130/с)
    m->state[2] += (m->state[1] - m->state[2]) * dt / RHODOPSIN_TAU_PDE_MS; // PDE* (τ~200 мс)
    m->state[3] += (1.0f - m->state[3]) * dt / RHODOPSIN_TAU_CGMP_SYN        // cGMP синтез (GC)
                 - m->state[2] * m->state[3] * dt / RHODOPSIN_TAU_CGMP_HYD; // минус гидролиз (PDE)
    if (m->state[0] < 0) m->state[0] = 0; if (m->state[0] > 1) m->state[0] = 1;
    if (m->state[1] < 0) m->state[1] = 0; if (m->state[1] > 1) m->state[1] = 1;
    if (m->state[2] < 0) m->state[2] = 0; if (m->state[2] > 1) m->state[2] = 1;
    if (m->state[3] < 0) m->state[3] = 0; if (m->state[3] > 1) m->state[3] = 1;
    double cg = m->state[3];
    float cg3 = cg * cg * cg;
    float kh = RHODOPSIN_CNG_KHALF, kh3 = kh * kh * kh;
    float Po = cg3 / (kh3 + cg3);  // CNG Hill n=3: открыт в темноте, закрыт на свету
    float area = effective_area(m, s);
    double vol = seg_volume(s);
    double cpm = FARADAY_CONST * vol / 1000.0;
    double P_Na, P_K, P_Ca;
    ghk_partial_P(CNG_GAMMA_PS, 0.5f, 0.3f, 0.2f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area * Po;
    double I_Na = P_Na * ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]) * N * A_TO_PA;
    float I_K  = (float)(P_K  * ghk_monovalent(V, s->intra_ions[ION_K],  s->extra_ions[ION_K])  * N * A_TO_PA);
    double I_Ca = P_Ca * ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]) * N * A_TO_PA;
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpm;
    s->intra_ions[ION_K]  -= (I_K  * dt) / cpm;
    s->intra_ions[ION_CA] -= (I_Ca * dt) / (2.0f * cpm);
    clamp_ions(s->intra_ions, ION_COUNT);
    return -(I_Na + I_K + I_Ca);
}

// ==================== MOD_OLFACTORY_RECEPTOR (108): обонятельный GPCR Gs ====================
BIOCORE_DEVICE_CALLABLE double olfactory_receptor_current(Modifier* m, Segment* s, float dt) {
    double ligand = m->state[1];
    if (ligand < 0.0f) ligand = 0.0f;
    if (ligand > 1.0f) ligand = 1.0f;
    float activation = 1.0f / (1.0f + exp(-2.0f * (ligand - 0.01f) * 100.0f));
    m->state[0] += (activation - m->state[0]) * dt / 50.0f;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    // Habituation (state[2]): GRK3/β-arrestin2 + Ca²⁺/CaM→PDE→cAMP↓, tau=2s
    sensor_habituation_step(m, 2, m->state[0], dt, 2000.0f);
    // Sensitization (state[3]): cAMP/PKA priming при слабой активации, tau=10s
    sensor_sensitization_step(m, 3, m->state[0], dt, 10000.0f);
    float k_hab = 0.7f;
    double k_sens = (m->state[4] > 0.0f) ? m->state[4] : 0.3f;
    double gain = sensor_adaptation_gain(m->state[2], m->state[3], k_hab, k_sens);
    // Gs-механизм: закрытие K⁺-каналов → деполяризация (синхрон с плагином mod_108).
    float area = effective_area(m, s);
    double vol = segment_volume(s);
    double cpm = charge_per_mole(vol);
    double effective_act = m->state[0] * gain;
    double P_single = ghk_single_channel_P(GIRK_GAMMA_PS, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(s->V_m, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double N = m->density * area;
    float I_K_basal = (float)(P_single * flux * N * A_TO_PA);
    float I_K_modulated = (float)(P_single * (1.0f - effective_act * 0.8f) * flux * N * A_TO_PA);
    float I_effect = I_K_basal - I_K_modulated;
    s->intra_ions[ION_K] -= (I_effect * dt) / cpm;
    clamp_ions(s->intra_ions, ION_COUNT);
    return -I_effect;
}

// ==================== MOD_VIBRATION_SENSOR (115): Вибрационный/акустический сенсор ====================
BIOCORE_DEVICE_CALLABLE double vibration_sensor_current(Modifier* m, Segment* s, float dt) {
    double intensity = m->state[1];
    if (intensity < 0.0f) intensity = 0.0f;
    if (intensity > 1.0f) intensity = 1.0f;
    float activation = 1.0f / (1.0f + exp(-3.0f * (intensity - 0.3f) * 10.0f));
    m->state[0] += (activation - m->state[0]) * dt / 30.0f;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    // Habituation (state[2]): tau=3s
    sensor_habituation_step(m, 2, m->state[0], dt, 3000.0f);
    // Sensitization (state[3]): tau=10s
    sensor_sensitization_step(m, 3, m->state[0], dt, 10000.0f);
    double k_sens = (m->state[4] > 0.0f) ? m->state[4] : 0.25f;
    double gain = sensor_adaptation_gain(m->state[2], m->state[3], 0.6f, k_sens);
    double eff = m->state[0] * gain;
    float area = effective_area(m, s);
    double vol = segment_volume(s);
    double cpm = charge_per_mole(vol);
    double P_Na, P_K, P_Ca;
    ghk_partial_P(MET_GAMMA_PS, 0.6f, 0.4f, 0.0f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area * eff;
    double I_Na = P_Na * ghk_monovalent(s->V_m, s->intra_ions[ION_NA], s->extra_ions[ION_NA]) * N * A_TO_PA;
    float I_K  = (float)(P_K  * ghk_monovalent(s->V_m, s->intra_ions[ION_K],  s->extra_ions[ION_K])  * N * A_TO_PA);
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpm;
    s->intra_ions[ION_K]  -= (I_K  * dt) / cpm;
    clamp_ions(s->intra_ions, ION_COUNT);
    return -(I_Na + I_K);
}

// ==================== MOD_OTOLITH_HAIR_CELL (116): Волосковая клетка отолитового органа ====================
// state[0] = MET P_open (тонический ~0.15 в покое — направление гравитации)
// state[1] = shear_force / линейное ускорение (0..1, из среды)
// state[2] = Ca²⁺-зависимая адаптация миозиновых моторов (τ=30ms)
// state[4] = gain override (чувствительность)
// state[5] = cos(preferred_angle) — направленное тюнингование
BIOCORE_DEVICE_CALLABLE double otolith_hair_cell_current(Modifier* m, Segment* s, float dt) {
    double shear = m->state[1];
    if (shear < 0.0f) shear = 0.0f;
    if (shear > 1.0f) shear = 1.0f;
    // Направленное (cosine) тюнингование: эффективный сдвиг в preferred direction
    double cos_theta = (m->state[5] > -99.0f) ? m->state[5] : 1.0f;
    if (cos_theta < -1.0f) cos_theta = -1.0f;
    if (cos_theta > 1.0f) cos_theta = 1.0f;
    float eff_shear = shear * cos_theta;
    if (eff_shear < 0.0f) eff_shear = 0.0f;
    // Ca²⁺-адаптация снижает эффективный сдвиг
    double adaptation = m->state[2];
    float adapted = eff_shear * (1.0f - 0.7f * adaptation);
    double sensitivity = (m->state[4] > 0.0f) ? m->state[4] : 5.0f;
    // Тонический MET P_open ~0.15 (гравитация), фазический до 1.0
    float target_open = 0.15f + 0.85f / (1.0f + exp(-sensitivity * (adapted - 0.15f) * 10.0f));
    m->state[0] += (target_open - m->state[0]) * dt / 1.0f; // τ≈1ms (быстрая механотрансдукция)
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    // Ca²⁺-зависимая медленная адаптация: dCa_adapt/dt ∝ P_open (больше Ca²⁺ → больше slip)
    m->state[2] += (m->state[0] - m->state[2]) * dt / 30.0f;
    if (m->state[2] < 0.0f) m->state[2] = 0.0f;
    if (m->state[2] > 1.0f) m->state[2] = 1.0f;
    float area = effective_area(m, s);
    double vol = segment_volume(s);
    double cpm = charge_per_mole(vol);
    double P_Na, P_K, P_Ca;
    ghk_partial_P(MET_GAMMA_PS, 0.60f, 0.30f, 0.10f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area * m->state[0];
    double I_Na = P_Na * ghk_monovalent(s->V_m, s->intra_ions[ION_NA], s->extra_ions[ION_NA]) * N * A_TO_PA;
    float I_K  = (float)(P_K  * ghk_monovalent(s->V_m, s->intra_ions[ION_K],  s->extra_ions[ION_K])  * N * A_TO_PA);
    double I_Ca = P_Ca * ghk_divalent(s->V_m, s->intra_ions[ION_CA], s->extra_ions[ION_CA]) * N * A_TO_PA;
    float I_total = I_Na - I_K + I_Ca;
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpm;
    s->intra_ions[ION_K]  -= (I_K  * dt) / cpm;
    s->intra_ions[ION_CA] -= (I_Ca * dt) / (2.0f * cpm);
    clamp_ions(s->intra_ions, ION_COUNT);
    return -I_total;
}

// ==================== MOD_SEMICIRCULAR_CUPULA (117): Купула полукружного канала ====================
// state[0] = θ — угол отклонения купулы (торсионный маятник)
// state[1] = α(t) — угловое ускорение (нормализованное, из среды)
// state[2] = Ca²⁺-зависимая адаптация волосковой клетки (τ=10ms, фазическая)
// state[3] = ω = dθ/dt (интегрирование, внутренняя переменная)
// state[4] = ω₀² override (жёсткость купулы, default=1.0 → τ≈6.28s)
// state[5] = 2ζω₀ override (демпфирование, default=0.9)
// state[6] = canal_id (0=левый, 1=правый — для push-pull)
BIOCORE_DEVICE_CALLABLE double semicircular_cupula_current(Modifier* m, Segment* s, float dt) {
    double alpha = m->state[1];
    if (alpha < -1.0f) alpha = -1.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    // Торсионный маятник: d²θ/dt² + 2ζω₀·dθ/dt + ω₀²·θ = γ·α
    double omega0_sq = (m->state[4] > 0.0f) ? m->state[4] : 1.0f;
    double two_zeta_omega0 = (m->state[5] > 0.0f) ? m->state[5] : 0.9f;
    float gamma = 1.0f;
    double omega = m->state[3]; // dθ/dt с предыдущего шага
    double theta = m->state[0];
    // dω/dt = γ·α - 2ζω₀·ω - ω₀²·θ
    float domega_dt = alpha * gamma - two_zeta_omega0 * omega - omega0_sq * theta;
    omega += domega_dt * dt / 1000.0f;
    theta += omega * dt / 1000.0f;
    // Упругий возврат купулы (τ≈8s)
    theta *= exp(-dt / 8000.0f);
    omega *= exp(-dt / 8000.0f);
    if (theta < -1.0f) theta = -1.0f;
    if (theta > 1.0f) theta = 1.0f;
    m->state[0] = theta;
    m->state[3] = omega;
    // Активация волосковой клетки: deflection → P_open
    float abs_deflection = fabsf(theta);
    double hc_sens = (m->state[4] > 0.0f) ? m->state[4] * 0.4f : 2.5f;
    float hc_target = 1.0f / (1.0f + exp(-hc_sens * (abs_deflection - 0.1f) * 15.0f));
    // Ca²⁺-адаптация (фазическая, τ=10ms)
    m->state[2] += (hc_target - m->state[2]) * dt / 10.0f;
    if (m->state[2] < 0.0f) m->state[2] = 0.0f;
    if (m->state[2] > 1.0f) m->state[2] = 1.0f;
    double eff_open = hc_target * (1.0f - 0.6f * m->state[2]);
    float area = effective_area(m, s);
    double vol = segment_volume(s);
    double cpm = charge_per_mole(vol);
    double P_Na, P_K, P_Ca;
    ghk_partial_P(MET_GAMMA_PS, 0.55f, 0.35f, 0.10f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area * eff_open;
    double I_Na = P_Na * ghk_monovalent(s->V_m, s->intra_ions[ION_NA], s->extra_ions[ION_NA]) * N * A_TO_PA;
    float I_K  = (float)(P_K  * ghk_monovalent(s->V_m, s->intra_ions[ION_K],  s->extra_ions[ION_K])  * N * A_TO_PA);
    double I_Ca = P_Ca * ghk_divalent(s->V_m, s->intra_ions[ION_CA], s->extra_ions[ION_CA]) * N * A_TO_PA;
    float I_total = I_Na - I_K + I_Ca;
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpm;
    s->intra_ions[ION_K]  -= (I_K  * dt) / cpm;
    s->intra_ions[ION_CA] -= (I_Ca * dt) / (2.0f * cpm);
    clamp_ions(s->intra_ions, ION_COUNT);
    return -I_total;
}

// ==================== MOD_ELECTRORECEPTOR_AMPULLARY (118): Ампулярный электрорецептор ====================
// state[0] = P_open MET-каналов (тонический ~0.15, модулируется внешним E-полем)
// state[1] = V_ext — внешнее напряжение на коже (нормированное, -1..1)
// state[2] = Ca²⁺-зависимая адаптация (τ=5s, медленная)
// state[3] = V_canal — low-pass фильтрованное напряжение (τ=100ms)
// state[4] = sensitivity gain override
// state[5] = cos(θ_pref) — направленное тюнингование
BIOCORE_DEVICE_CALLABLE double electroreceptor_ampullary_current(Modifier* m, Segment* s, float dt) {
    double V_ext = m->state[1];
    if (V_ext < -1.0f) V_ext = -1.0f;
    if (V_ext > 1.0f) V_ext = 1.0f;
    // Направленное тюнингование: эффективное напряжение = V_ext × cos(θ — θ_pref)
    double cos_pref = (m->state[5] > -99.0f) ? m->state[5] : 1.0f;
    if (cos_pref < -1.0f) cos_pref = -1.0f;
    if (cos_pref > 1.0f) cos_pref = 1.0f;
    float V_eff = V_ext * cos_pref;
    // Low-pass фильтр канала (RC: τ≈100ms)
    m->state[3] += (V_eff - m->state[3]) * dt / 100.0f;
    // Ca²⁺-адаптация снижает чувствительность к постоянному полю (τ=5s)
    double adapt_target = fabs(m->state[3]);
    m->state[2] += (adapt_target - m->state[2]) * dt / 5000.0f;
    if (m->state[2] < 0.0f) m->state[2] = 0.0f;
    if (m->state[2] > 1.0f) m->state[2] = 1.0f;
    double adapted_V = m->state[3] * (1.0f - 0.8f * m->state[2]);
    // Сигмоидная активация MET-каналов
    double sensitivity = (m->state[4] > 0.0f) ? m->state[4] : 4.0f;
    float target_open = 0.15f + 0.85f / (1.0f + exp(-sensitivity * (adapted_V - 0.1f) * 15.0f));
    m->state[0] += (target_open - m->state[0]) * dt / 2.0f; // τ≈2ms
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    float area = effective_area(m, s);
    double vol = segment_volume(s);
    double cpm = charge_per_mole(vol);
    double P_Na, P_K, P_Ca;
    ghk_partial_P(MET_GAMMA_PS, 0.50f, 0.35f, 0.15f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area * m->state[0];
    double I_Na = P_Na * ghk_monovalent(s->V_m, s->intra_ions[ION_NA], s->extra_ions[ION_NA]) * N * A_TO_PA;
    float I_K  = (float)(P_K  * ghk_monovalent(s->V_m, s->intra_ions[ION_K],  s->extra_ions[ION_K])  * N * A_TO_PA);
    double I_Ca = P_Ca * ghk_divalent(s->V_m, s->intra_ions[ION_CA], s->extra_ions[ION_CA]) * N * A_TO_PA;
    float I_total = I_Na - I_K + I_Ca;
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpm;
    s->intra_ions[ION_K]  -= (I_K  * dt) / cpm;
    s->intra_ions[ION_CA] -= (I_Ca * dt) / (2.0f * cpm);
    clamp_ions(s->intra_ions, ION_COUNT);
    return -I_total;
}

// ==================== MOD_MAGNETITE_MECHANORECEPTOR (119): Магнетитовый механорецептор ====================
// state[0] = P_open (механочувствительный, 0..1)
// state[1] = B_norm — норма магнитного поля (0..1, из среды)
// state[2] = Ca²⁺-зависимая адаптация (τ=8s)
// state[3] = cos(θ_B) — угол между цепочкой и полем (крутящий момент ∝ sin(θ))
// state[4] = k_adapt override
// state[5] = предпочтительный угол цепочки θ_anat (в радианах)
BIOCORE_DEVICE_CALLABLE double magnetite_mechanoreceptor_current(Modifier* m, Segment* s, float dt) {
    double B_norm = m->state[1];
    if (B_norm < 0.0f) B_norm = 0.0f;
    if (B_norm > 1.0f) B_norm = 1.0f;
    // Крутящий момент τ = m × B → |τ| ∝ sin(θ_B) × |B|
    // sin²(θ) для вероятности открытия (энергетический барьер ∝ torque²)
    double sin_theta = sqrtf(1.0f - m->state[3] * m->state[3]); // sin from cos
    if (sin_theta < 0.0f) sin_theta = 0.0f;
    float torque_factor = B_norm * sin_theta * sin_theta; // ∝ sin²(θ)
    // Механотрансдукция: torque → MET P_open
    double sensitivity = (m->state[4] > 0.0f) ? m->state[4] : 3.0f;
    float target_open = 1.0f / (1.0f + exp(-sensitivity * (torque_factor - 0.05f) * 20.0f));
    m->state[0] += (target_open - m->state[0]) * dt / 5.0f; // τ≈5ms (медленнее аудио)
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    // Ca²⁺-адаптация (τ=8s)
    m->state[2] += (m->state[0] - m->state[2]) * dt / 8000.0f;
    if (m->state[2] < 0.0f) m->state[2] = 0.0f;
    if (m->state[2] > 1.0f) m->state[2] = 1.0f;
    double eff_open = m->state[0] * (1.0f - 0.5f * m->state[2]);
    float area = effective_area(m, s);
    double vol = segment_volume(s);
    double cpm = charge_per_mole(vol);
    double P_Na, P_K, P_Ca;
    ghk_partial_P(MET_GAMMA_PS, 0.45f, 0.30f, 0.25f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area * eff_open;
    double I_Na = P_Na * ghk_monovalent(s->V_m, s->intra_ions[ION_NA], s->extra_ions[ION_NA]) * N * A_TO_PA;
    float I_K  = (float)(P_K  * ghk_monovalent(s->V_m, s->intra_ions[ION_K],  s->extra_ions[ION_K])  * N * A_TO_PA);
    double I_Ca = P_Ca * ghk_divalent(s->V_m, s->intra_ions[ION_CA], s->extra_ions[ION_CA]) * N * A_TO_PA;
    float I_total = I_Na - I_K + I_Ca;
    s->intra_ions[ION_NA] -= (I_Na * dt) / cpm;
    s->intra_ions[ION_K]  -= (I_K  * dt) / cpm;
    s->intra_ions[ION_CA] -= (I_Ca * dt) / (2.0f * cpm);
    clamp_ions(s->intra_ions, ION_COUNT);
    return -I_total;
}

// ==================== MOD_CRYPTOCHROME_MAGNETIC (120): Криптохромный магнитный сенсор ====================
// state[0] = RP_yield — выход радикальной пары (0..1, зависит от B)
// state[1] = light_intensity — интенсивность синего света (0..1, из среды)
// state[2] = B_angle_factor = sin²(θ) (вычисляется из B-поля)
// state[4] = sensitivity override
// state[5] = wavelength_factor (1.0 для синего, <0.1 для красного)
// МОДУЛЯТОРНЫЙ: изменяет проводимость MOD_LIGHT_SENSOR на том же сегменте
BIOCORE_DEVICE_CALLABLE double cryptochrome_magnetic_current(Modifier* m, Segment* s, float dt) {
    double light = m->state[1];
    if (light < 0.0f) light = 0.0f;
    if (light > 1.0f) light = 1.0f;
    double sin2_theta = m->state[2];
    if (sin2_theta < 0.0f) sin2_theta = 0.0f;
    if (sin2_theta > 1.0f) sin2_theta = 1.0f;
    double wavelength_factor = (m->state[5] > 0.0f) ? m->state[5] : 1.0f;
    // Выход радикальной пары: RP ∝ light × sin²(θ) × wavelength
    double sensitivity = (m->state[4] > 0.0f) ? m->state[4] : 1.0f;
    float target_rp = light * sin2_theta * wavelength_factor * sensitivity;
    m->state[0] += (target_rp - m->state[0]) * dt / 50.0f; // τ≈50ms (реакция переноса электрона)
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    // Модуляция: найти MOD_LIGHT_SENSOR на этом сегменте и модифицировать его state[0]
    for (int i = 0; i < s->modifier_count; i++) {
        if (s->modifiers[i].type == MOD_LIGHT_SENSOR) {
            // Усиление светового ответа: Δg = g × (1 + k_cry × RP_yield)
            float k_cry = 0.3f;
            double boost = 1.0f + k_cry * m->state[0];
            // Модифицируем adapted_signal (state[0]) светового сенсора
            s->modifiers[i].state[0] *= boost;
            if (s->modifiers[i].state[0] > 1.0f) s->modifiers[i].state[0] = 1.0f;
            break;
        }
    }
    return 0.0f; // чисто модуляторный — не генерирует собственный ток
}

// ==================== PMCA (98): Ca²⁺-АТФаза плазмалеммы ====================
BIOCORE_DEVICE_CALLABLE double pmca_current(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — PMCA Ca²⁺-АТФаза (Carafoli 1991). Hill-2, K_m=300нМ, электрогенный Ca²⁺ наружу.
    float area = effective_area(m, s);
    double K_m_Ca = (m->state[1] > 0.0f) ? m->state[1] : PMCA_K_M_CA_M_M;
    double hill_n = (m->state[2] > 0.0f) ? m->state[2] : PMCA_HILL_N;

    // ATP: концентрация в мМ (свойство клетки). Michaelis-Menten, K_m_ATP в мМ.
    float atp = s->ATP_level;
    if (atp < 0.0f) atp = 0.0f;
    float f_ATP = atp / (atp + PMCA_K_ATP_MM);

    // Ca²⁺-зависимость: Hill-2 (два Ca²⁺-связывающих сайта).
    float ca = s->intra_ions[ION_CA];
    if (ca < 1e-9f) ca = 1e-9f;
    float ca_pow = pow(ca, hill_n);
    float f_Ca = ca_pow / (ca_pow + pow(K_m_Ca, hill_n));

    // ЕДИНИЦА — молекула: I = N·k_cat·f_Ca·f_ATP·e·Δz (Δz=+2, 1 Ca²⁺ наружу).
    double N = m->density * area;
    float I_pump = N * PMCA_TURNOVER_MAX_S * f_Ca * f_ATP * ELEMENTARY_CHARGE_C * 2.0f * A_TO_PA; // пА

    // Транспорт 1 Ca²⁺ наружу (z=2 → /2).
    float volume = seg_volume(s);
    double cpv = FARADAY_CONST * volume / 1000.0;
    float transport = I_pump * dt / (2.0f * cpv);
    // Ограничить транспорт доступным Ca²⁺ (не уводить ниже пола 1e-9).
    float ca_avail = s->intra_ions[ION_CA] - 1e-9f;
    if (ca_avail < 0.0f) ca_avail = 0.0f;
    if (transport > ca_avail) transport = ca_avail;
    s->intra_ions[ION_CA] -= transport;
    s->extra_ions[ION_CA] += transport / EXTRA_VOLUME_RATIO;
    clamp_ions(s->intra_ions, ION_COUNT);

    // ATP: 1 ATP на 1 Ca²⁺.
    s->ATP_level -= I_pump * dt * NA_K_PUMP_ATP_RATE_PA;
    if (s->ATP_level < 0.0f) s->ATP_level = 0.0f;

    // Электрогенный (+2 наружу) → гиперполяризующий → отрицательный ток.
    return -I_pump;
}

// ==================== NCX (99): Na⁺/Ca²⁺ обменник ====================
BIOCORE_DEVICE_CALLABLE double ncx_current(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — NCX Na⁺/Ca²⁺ обменник (Weber 2001, Shannon 2004). 3Na:1Ca, реверсивный.
    float area = effective_area(m, s);
    float Na_i = s->intra_ions[ION_NA]; if (Na_i < 1e-9f) Na_i = 1e-9f;
    float Na_o = s->extra_ions[ION_NA]; if (Na_o < 1e-9f) Na_o = 1e-9f;
    float Ca_i = s->intra_ions[ION_CA]; if (Ca_i < 1e-9f) Ca_i = 1e-9f;
    float Ca_o = s->extra_ions[ION_CA]; if (Ca_o < 1e-9f) Ca_o = 1e-9f;

    // Реверсивный поток (знак меняется при E_NaCa), нормированный на I_max.
    float flux_norm = ncx_flux_norm(s->V_m, Na_i, Na_o, Ca_i, Ca_o);

    // ЕДИНИЦА — молекула: I = N·k_cat·flux_norm·e·Δz (Δz=+1, forward = +1 внутрь).
    double N = m->density * area;
    float I_ncx = N * NCX_TURNOVER_MAX_S * flux_norm * ELEMENTARY_CHARGE_C * A_TO_PA; // пА

    // Транспорт: 1 Ca²⁺ и 3 Na⁺ на цикл (знаки разворачиваются в reverse автоматически).
    float volume = seg_volume(s);
    double cpv = FARADAY_CONST * volume / 1000.0;
    float transport = I_ncx * dt / cpv;
    // Ограничить транспорт доступными ионами (не выкачивать больше, чем есть).
    if (transport > Ca_i) transport = Ca_i;                   // forward: Ca наружу ≤ Ca_i
    if (transport < -Na_i / 3.0f) transport = -Na_i / 3.0f;  // reverse: Na наружу ≤ Na_i
    s->intra_ions[ION_CA] -= transport;             // 1 Ca наружу (forward)
    s->extra_ions[ION_CA] += transport / EXTRA_VOLUME_RATIO;
    s->intra_ions[ION_NA] += 3.0f * transport;      // 3 Na внутрь (forward)
    s->extra_ions[ION_NA] -= 3.0f * transport / EXTRA_VOLUME_RATIO;
    clamp_ions(s->intra_ions, ION_COUNT);

    return I_ncx;  // forward (+1 внутрь) = деполяризующий
}

// ==================== KCC2 (100): K⁺-Cl⁻ котранспортёр ====================
BIOCORE_DEVICE_CALLABLE double kcc2_current(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — KCC2 K⁺-Cl⁻ котранспортёр (Payne 1997, Rivera 1999). Электронейтральный.
    float area = effective_area(m, s);
    double Km_K_i = (m->state[1] > 0.0f) ? m->state[1] : KCC2_K_M_K_I_MM;
    double Km_Cl_i = (m->state[2] > 0.0f) ? m->state[2] : KCC2_K_M_CL_I_MM;
    float K_i  = s->intra_ions[ION_K];  if (K_i  < 1e-9f) K_i  = 1e-9f;  // мМ ≡ моль/м³
    float Cl_i = s->intra_ions[ION_CL]; if (Cl_i < 1e-9f) Cl_i = 1e-9f;
    float K_o  = s->extra_ions[ION_K];  if (K_o  < 1e-9f) K_o  = 1e-9f;
    float Cl_o = s->extra_ions[ION_CL]; if (Cl_o < 1e-9f) Cl_o = 1e-9f;

    // Насыщение (Michaelis) по внутриклеточным субстратам.
    float f_K = K_i / (K_i + Km_K_i);
    float f_Cl = Cl_i / (Cl_i + Km_Cl_i);

    // Термодинамическая движущая сила электронейтрального 1:1 котранспорта:
    // net flux ∝ 1 − [K]o[Cl]o/([K]i[Cl]i). >0 → выводит K+Cl; <0 → вводит; =0 → равновесие.
    float driving = 1.0f - (K_o * Cl_o) / (K_i * Cl_i);
    if (driving > 1.0f) driving = 1.0f;
    if (driving < -1.0f) driving = -1.0f;

    // Поток J [моль/тик] = V_max[нмоль/см²/с]·1e-20 · density·area[µm²]·f_K·f_Cl·driving·dt[мс].
    float flux_mol = KCC2_VMAX_NMOL_PER_CM2_S * NMOL_CM2_S_TO_MOL_UM2_MS
                   * m->density * area * f_K * f_Cl * driving * dt;

    // Δc [мМ] = J / vol[m³]. Электронейтральный 1:1 K⁺:Cl⁻ (знак задаёт направление).
    float dconc = flux_mol / (seg_volume(s) * UM3_TO_M3);

    // Ограничить транспорт доступными ионами: вывод — из intra, ввод — из extra.
    if (dconc > 0.0f) {
        float k_avail  = K_i  - 1e-9f;
        float cl_avail = Cl_i - 1e-9f;
        float min_avail = (k_avail < cl_avail) ? k_avail : cl_avail;
        if (min_avail < 0.0f) min_avail = 0.0f;
        if (dconc > min_avail) dconc = min_avail;
    } else {
        float k_avail  = (K_o  - 1e-9f) * EXTRA_VOLUME_RATIO;
        float cl_avail = (Cl_o - 1e-9f) * EXTRA_VOLUME_RATIO;
        float min_avail = (k_avail < cl_avail) ? k_avail : cl_avail;
        if (min_avail < 0.0f) min_avail = 0.0f;
        float mag = -dconc;
        if (mag > min_avail) mag = min_avail;
        dconc = -mag;
    }
    s->intra_ions[ION_K]  -= dconc;
    s->intra_ions[ION_CL] -= dconc;
    s->extra_ions[ION_K]  += dconc / EXTRA_VOLUME_RATIO;
    s->extra_ions[ION_CL] += dconc / EXTRA_VOLUME_RATIO;
    clamp_ions(s->intra_ions, ION_COUNT);

    return 0.0f;  // электронейтральный — нет тока
}

// ==================== NKCC1 (122): Na-K-2Cl котранспортер ====================
// Импорт 1Na⁺:1K⁺:2Cl⁻ ВНУТРЬ клетки (электронейтральный).
// Движущая сила — Na⁺ градиент (создаётся Na/K-АТФазой).
// Функциональный антагонист KCC2: повышает [Cl⁻]i → деполяризующее торможение.
// Ключевой механизм онтогенетического Cl⁻ переключателя в развивающемся Xenopus.
BIOCORE_DEVICE_CALLABLE double nkcc1_current(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — NKCC1 Na⁺-K⁺-2Cl⁻ котранспортёр (Delpire & Gagnon 2011). Электронейтральный.
    float area = effective_area(m, s);
    double Km_Na_i = (m->state[1] > 0.0f) ? m->state[1] : NKCC1_K_M_NA_I_MM;
    double Km_K_i = (m->state[2] > 0.0f) ? m->state[2] : NKCC1_K_M_K_I_MM;
    double Km_Cl_i = (m->state[3] > 0.0f) ? m->state[3] : NKCC1_K_M_CL_I_MM;
    double Km_Na_o = (m->state[4] > 0.0f) ? m->state[4] : NKCC1_K_M_NA_O_MM;
    double Km_K_o = (m->state[5] > 0.0f) ? m->state[5] : NKCC1_K_M_K_O_MM;

    float na_i = s->intra_ions[ION_NA]; // мМ ≡ моль/м³
    float k_i  = s->intra_ions[ION_K];
    float cl_i = s->intra_ions[ION_CL];
    float na_o = s->extra_ions[ION_NA];
    float k_o  = s->extra_ions[ION_K];
    if (na_i < 1e-9f) na_i = 1e-9f;
    if (k_i  < 1e-9f) k_i  = 1e-9f;
    if (cl_i < 1e-9f) cl_i = 1e-9f;
    if (na_o < 1e-9f) na_o = 1e-9f;
    if (k_o  < 1e-9f) k_o  = 1e-9f;

    // Michaelis-Menten: Na_i, K_i, Cl_i
    float na_factor = na_i / (na_i + Km_Na_i);
    float k_factor  = k_i  / (k_i  + Km_K_i);
    float cl_factor = cl_i / (cl_i + Km_Cl_i);

    // Транс-стимуляция Na⁺_o, транс-ингибирование K⁺_o
    float trans_stim  = na_o / (na_o + Km_Na_o);
    float trans_inhib = 1.0f / (1.0f + k_o / Km_K_o);

    // Поток J [моль/тик] = V_max[нмоль/см²/с]·1e-20 · density·area[µm²]·factors·dt[мс].
    float flux_mol = NKCC1_VMAX_NMOL_PER_CM2_S * NMOL_CM2_S_TO_MOL_UM2_MS
                   * m->density * area * na_factor * k_factor * cl_factor * trans_stim * trans_inhib * dt;

    // Δc [мМ] = J / vol[m³]. Электронейтральный 1:1:2 внутрь.
    float dconc = flux_mol / (seg_volume(s) * UM3_TO_M3);
    // Ограничить транспорт доступными ионами extra (×10 резервуар): Na 1, K 1, Cl 2.
    float na_avail = (s->extra_ions[ION_NA] - 1e-9f) * EXTRA_VOLUME_RATIO;
    float k_avail  = (s->extra_ions[ION_K]  - 1e-9f) * EXTRA_VOLUME_RATIO;
    float cl_avail = (s->extra_ions[ION_CL] - 1e-9f) * EXTRA_VOLUME_RATIO / 2.0f;
    float min_avail = (na_avail < k_avail) ? na_avail : k_avail;
    if (cl_avail < min_avail) min_avail = cl_avail;
    if (min_avail < 0.0f) min_avail = 0.0f;
    if (dconc > min_avail) dconc = min_avail;
    s->intra_ions[ION_NA] += dconc;
    s->intra_ions[ION_K]  += dconc;
    s->intra_ions[ION_CL] += 2.0f * dconc;
    s->extra_ions[ION_NA] -= dconc / EXTRA_VOLUME_RATIO;
    s->extra_ions[ION_K]  -= dconc / EXTRA_VOLUME_RATIO;
    s->extra_ions[ION_CL] -= 2.0f * dconc / EXTRA_VOLUME_RATIO;
    clamp_ions(s->intra_ions, ION_COUNT);

    return 0.0f; // электронейтральный
}

// ==================== KV DELAYED RECTIFIER (123): Kv1.1 delayed rectifier ====================
// Быстрый потенциал-зависимый K⁺ канал без инактивации. Основной механизм реполяризации спайка.
// OHM-парадигма (как DIN_NAK): I = g[С/см²]·m·(V−E_K). state[0]=m, state[1]=V½(−30), state[2]=k(6),
// state[3]=g_max(0.02 С/см²), state[4]=tau(2мс). Kv1.1 (KCNA1), Stühmer 1989 / Gutman 2005.
BIOCORE_DEVICE_CALLABLE double kv_delayed_rectifier_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double m_val = m->state[0];
    double V_half = (m->state[1] != 0.0f) ? m->state[1] : KDR_VHALF_MV;
    double k = (m->state[2] > 0.0f)  ? m->state[2] : KDR_K_MV;
    double tau = (m->state[4] > 0.0f)  ? m->state[4] : kv_dr_m_tau(V, V_half);
    // ✅ ДОСТОВЕРНО — Kv1.1; менять ТОЛЬКО с согласования (алгоритм аудита модификаторов).

    // m-гейт (Больцман, Kv1.1)
    float m_inf = 1.0f / (1.0f + exp(-(V - V_half) / (k + 1e-6f)));
    m_val += (m_inf - m_val) * dt / (tau + 1e-6f);
    if (m_val < 0.0f) m_val = 0.0f;
    if (m_val > 1.0f) m_val = 1.0f;
    m->state[0] = m_val;

    // Одномолекулярная GHK: I_K = P_single·flux·(density·area)·m (Kv1.1 delayed rectifier).
    float K_i = s->intra_ions[ION_K];  // моль/м³
    float K_o = s->extra_ions[ION_K];
    double P_single = ghk_single_channel_P(KDR_GAMMA_PS, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * m_val;
    double I_K = I_A * A_TO_PA;

    double vol = seg_volume(s);
    float charge_per_mole_vol = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_K] -= (I_K * dt) / charge_per_mole_vol;
    // Биологическая точность: K+ из клетки → во внеклеточное пространство
    // (объём внеклетки ~10× внутриклеточного)
    s->extra_ions[ION_K] += (I_K * dt) / (charge_per_mole_vol * EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT);
    clamp_ions(s->extra_ions, ION_COUNT);

    return -I_K;
}

// ==================== Ca LEAK (101): Пассивная Ca²⁺ утечка ====================
BIOCORE_DEVICE_CALLABLE double ca_leak_current(Modifier* m, Segment* s, float dt) {
    float area = effective_area(m, s);
    double gamma_ps = (m->state[0] > 0.0f) ? m->state[0] : 50.0f;  // γ (пС, TRPC7)
    float V = s->V_m;
    float ca_in  = s->intra_ions[ION_CA], ca_out = s->extra_ions[ION_CA];
    float na_in  = s->intra_ions[ION_NA], na_out = s->extra_ions[ION_NA];

    // nonselective TRPC7: P_Ca/P_Na ≈ 2 (Ca²⁺-предпочитающий)
    double P_Na, P_K, P_Ca;
    ghk_partial_P(50.0f, 1.0f, 0.0f, 2.0f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Na = ghk_monovalent(V, na_in, na_out);
    double flux_Ca = ghk_divalent(V, ca_in, ca_out);
    double I_Na = P_Na * flux_Na * N * A_TO_PA;
    double I_Ca = P_Ca * flux_Ca * N * A_TO_PA;

    float volume = seg_volume(s);
    double cpv = FARADAY_CONST*volume/1000.0f;
    s->intra_ions[ION_NA] -= (I_Na*dt)/cpv;
    s->intra_ions[ION_CA] -= (I_Ca*dt)/(2.0f*cpv);
    clamp_ions(s->intra_ions, ION_COUNT);
    return -(I_Na + I_Ca);
}

// ==================== Ca BUFFER (102): Быстрый Ca²⁺ буфер ====================
BIOCORE_DEVICE_CALLABLE double ca_buffer_current(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — Ca²⁺-буфер (парвальбумин-подобный, Schwaller 2010). Без тока.
    // density масштабирует ЁМКОСТЬ B_total (количество белка), кинетика k_on/k_off — свойство молекулы.
    double B_total_ref = (m->state[0] > 0.0f) ? m->state[0] : CA_BUFFER_B_TOTAL_MM;
    double B_total = B_total_ref * m->density;   // density → ёмкость
    double K_d = (m->state[2] > 0.0f) ? m->state[2] : CA_BUFFER_K_D_MM;
    double k_on = (m->state[3] > 0.0f) ? m->state[3] : CA_BUFFER_K_ON_MM_MS;
    double k_off = (m->state[4] > 0.0f) ? m->state[4] : (K_d*k_on);
    double Ca_bound = m->state[1];
    if(Ca_bound<=0.0f && B_total>0.0f) {
        float Ca_free0 = s->intra_ions[ION_CA];
        Ca_bound = B_total*Ca_free0/(Ca_free0+K_d);
        m->state[1] = Ca_bound;
    }
    float Ca_free = s->intra_ions[ION_CA];
    if(Ca_free<1e-9f) Ca_free=1e-9f;
    float B_free = B_total - Ca_bound;
    if(B_free<0.0f) B_free=0.0f;
    float dCa_bound = (k_on*Ca_free*B_free - k_off*Ca_bound)*dt;
    Ca_bound += dCa_bound;
    if(Ca_bound<0.0f) Ca_bound=0.0f;
    if(Ca_bound>B_total) Ca_bound=B_total;
    s->intra_ions[ION_CA] -= dCa_bound;
    if(s->intra_ions[ION_CA]<1e-9f) {
        float deficit = 1e-9f - s->intra_ions[ION_CA];
        s->intra_ions[ION_CA] = 1e-9f;
        Ca_bound -= deficit;
        if(Ca_bound<0.0f) Ca_bound=0.0f;
    }
    m->state[1] = Ca_bound;
    return 0.0f;
}

// ==================== SK (103): Ca²⁺-активируемый K⁺ (KCa2.2, mAHP) ====================
BIOCORE_DEVICE_CALLABLE double sk_channel_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double open_prob = m->state[0];
    double gamma_ps = (m->state[3] > 0.0f) ? m->state[3] : SK_GAMMA_PS;   // γ (пС)
    double tau_open = (m->state[4] > 0.0f) ? m->state[4] : SK_TAU_OPEN_MS;
    double tau_close = (m->state[5] > 0.0f) ? m->state[5] : SK_TAU_CLOSE_MS;
    // ✅ ДОСТОВЕРНО — менять ТОЛЬКО с согласования (алгоритм аудита модификаторов).

    float ca = s->intra_ions[ION_CA];  // моль/м³ (= мМ)
    float p_inf = sk_p_inf(ca);
    float tau = (p_inf > open_prob) ? tau_open : tau_close;
    open_prob += (p_inf - open_prob)*dt/tau;
    if(open_prob<0.0f) open_prob=0.0f;
    if(open_prob>1.0f) open_prob=1.0f;
    m->state[0] = open_prob;

    float K_i=s->intra_ions[ION_K], K_o=s->extra_ions[ION_K];  // моль/м³
    double P_single=ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux=ghk_monovalent(V, K_i, K_o);
    double I_A=P_single*flux*(m->density*area)*open_prob;
    float I_K=(float)(I_A*A_TO_PA);
    double vol=seg_volume(s);
    double cpv=FARADAY_CONST*vol/1000.0f;
    s->intra_ions[ION_K]-=(I_K*dt)/cpv;
    s->extra_ions[ION_K] += (I_K*dt)/(cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);
    return -I_K;
}

// ==================== KCa3.1 (209): Ca²⁺-активируемый K⁺ (IK) ====================
BIOCORE_DEVICE_CALLABLE double kca31_current(Modifier* m, Segment* s, float dt) {
    // KCa3.1 (KCNN4) — Ca²⁺-активируемый K⁺ (IK), voltage-independent. γ=25пС, EC50 188нМ.
    float V = s->V_m;
    float area = effective_area(m, s);
    double open_prob = m->state[0];
    double gamma_ps = (m->state[3] > 0.0f) ? m->state[3] : KCA31_GAMMA_PS;
    double tau_open = (m->state[4] > 0.0f) ? m->state[4] : KCA31_TAU_OPEN_MS;
    double tau_close = (m->state[5] > 0.0f) ? m->state[5] : KCA31_TAU_CLOSE_MS;
    float ca = s->intra_ions[ION_CA];
    float p_inf = kca31_p_inf(ca);
    float tau = (p_inf > open_prob) ? tau_open : tau_close;
    open_prob += (p_inf - open_prob) * dt / tau;
    if (open_prob < 0.0f) open_prob = 0.0f;
    if (open_prob > 1.0f) open_prob = 1.0f;
    m->state[0] = open_prob;
    // TRAM-34 блок (state[6], нМ)
    double tram = (m->state[6] > 0.0f) ? m->state[6] : 0.0f;
    open_prob /= (1.0f + tram / KCA31_TRAM34_IC50_NM);
    float K_i = s->intra_ions[ION_K], K_o = s->extra_ions[ION_K];
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    double I_K = I_A * A_TO_PA;
    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);
    return -I_K;
}

// ==================== BK (104): Ca²⁺+V-активируемый K⁺ (KCa1.1, Horrigan–Aldrich) ====================
BIOCORE_DEVICE_CALLABLE double bk_channel_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double open_prob = m->state[0];
    double gamma_ps = (m->state[1]>0.0f)?m->state[1]:BK_GAMMA_PS;  // γ (пС)
    double tau_ch = (m->state[2]>0.0f)?m->state[2]:BK_TAU_MS;      // τ (мс)
    // ✅ ДОСТОВЕРНО — Horrigan–Aldrich; менять ТОЛЬКО с согласования (алгоритм аудита модификаторов).

    float Ca=s->intra_ions[ION_CA];  // моль/м³ (= мМ)
    float po_inf=bk_ha_po(V, Ca);
    open_prob+=(po_inf-open_prob)*dt/tau_ch;
    if(open_prob<0.0f) open_prob=0.0f;
    if(open_prob>1.0f) open_prob=1.0f;
    m->state[0]=open_prob;

    float K_i=s->intra_ions[ION_K], K_o=s->extra_ions[ION_K];  // моль/м³
    double P_single=ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux=ghk_monovalent(V, K_i, K_o);
    double I_A=P_single*flux*(m->density*area)*open_prob;
    float I_K=(float)(I_A*A_TO_PA);
    double vol=seg_volume(s);
    double cpv=FARADAY_CONST*vol/1000.0f;
    s->intra_ions[ION_K]-=(I_K*dt)/cpv;
    s->extra_ions[ION_K] += (I_K*dt)/(cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);
    return -I_K;
}

// ==================== NaP (105): Персистентный Na⁺ (Nav1.6) ====================
BIOCORE_DEVICE_CALLABLE double nap_channel_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double m_val = m->state[0];
    double V_half = (m->state[1]!=0.0f)?m->state[1]:NAP_VHALF_M;
    double k_m = (m->state[2]>0.0f)?m->state[2]:NAP_K_M;
    double gamma_ps = (m->state[3]>0.0f)?m->state[3]:NAP_GAMMA_PS;    // γ (пС, Nav1.6)
    double tau_m = (m->state[4]>0.0f)?m->state[4]:NAP_TAU_M_MS;
    double Po = (m->state[5]>0.0f)?m->state[5]:NAP_PERSIST_FRAC;      // persistent-фракция
    float m_inf=boltzmann_act(V,V_half,k_m);
    m_val+=(m_inf-m_val)*dt/tau_m;
    if(m_val<0.0f) m_val=0.0f;
    if(m_val>1.0f) m_val=1.0f;
    m->state[0]=m_val;
    float Na_i=s->intra_ions[ION_NA], Na_o=s->extra_ions[ION_NA];  // моль/м³
    double P_single=ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_NA_MM);
    double flux=ghk_monovalent(V, Na_i, Na_o);
    double I_A=P_single*flux*(m->density*area)*(m_val*Po);
    float I_Na=(float)(I_A*A_TO_PA);
    double vol=seg_volume(s);
    double cpv=FARADAY_CONST*vol/1000.0f;
    s->intra_ions[ION_NA]-=(I_Na*dt)/cpv;  // Na⁺ входит (знак как в плагине mod_105)
    clamp_ions(s->intra_ions, ION_COUNT);
    return -I_Na;
}

// ==================== NaP-tadpole (195): Nav1.2-persistent Na dIN ====================
// Тадпольный INaP dIN (Svensson 2017, J Neurophysiol 118:121): активация V½=−25 мВ (активен
// выше −40, пик 0) — НЕ Nav1.6 Mes V (V½=−58.7). Структура как NaP (105), другой V½.
BIOCORE_DEVICE_CALLABLE double nap_tadpole_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double m_val = m->state[0];
    double V_half = (m->state[1]!=0.0f)?m->state[1]:NAP_TAD_VHALF_M;
    double k_m = (m->state[2]>0.0f)?m->state[2]:NAP_TAD_K_M;
    double gamma_ps = (m->state[3]>0.0f)?m->state[3]:NAP_TAD_GAMMA_PS;
    double tau_m = (m->state[4]>0.0f)?m->state[4]:NAP_TAD_TAU_M_MS;
    double Po = (m->state[5]>0.0f)?m->state[5]:NAP_TAD_PERSIST_FRAC;
    float m_inf=boltzmann_act(V,V_half,k_m);
    m_val+=(m_inf-m_val)*dt/tau_m;
    if(m_val<0.0f) m_val=0.0f;
    if(m_val>1.0f) m_val=1.0f;
    m->state[0]=m_val;
    float Na_i=s->intra_ions[ION_NA], Na_o=s->extra_ions[ION_NA];  // моль/м³
    // Одноканальная проницаемость [м³/с] = γ·RT/(z²F²·c̄_Na) — chord-conductance (Hille).
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_NA_MM);
    double flux = ghk_monovalent(V, Na_i, Na_o);
    // Slow inactivation (state[6]): s развивается при деполяризации (τ_slow), ограничивает NaP
    // → НЕ держит dIN деполяризованным (single-spike). Fleidervish & Gutnick 1996, Crill 1996.
    float s_inf = 1.0f / (1.0f + exp((V - NAP_TAD_SLOW_VHALF_MV) / NAP_TAD_SLOW_K_MV));
    double s_val = m->state[6];
    s_val += (s_inf - s_val) * dt / NAP_TAD_SLOW_TAU_MS;
    if (s_val < 0.0f) s_val = 0.0f; if (s_val > 1.0f) s_val = 1.0f;
    m->state[6] = s_val;

    double I_A = P_single * flux * (m->density * area) * (m_val * Po * s_val);
    double I_Na = I_A * A_TO_PA;
    double vol=seg_volume(s);
    double cpv=FARADAY_CONST*vol/1000.0f;
    s->intra_ions[ION_NA]-=(I_Na*dt)/cpv;
    s->extra_ions[ION_NA]+=(I_Na*dt)/(cpv*EXTRA_VOLUME_RATIO);  // консервация: Na внутрь ← внеклетка
    clamp_ions(s->intra_ions, ION_COUNT);
    clamp_ions(s->extra_ions, ION_COUNT);
    return -I_Na;
}

// ==================== Kir (106): Входящий выпрямитель K⁺ ====================
BIOCORE_DEVICE_CALLABLE double kir_channel_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double gamma_ps = (m->state[3]>0.0f)?m->state[3]:KIR_GAMMA_PS;  // γ (пС, Kir2.1)
    float K_i=s->intra_ions[ION_K], K_o=s->extra_ions[ION_K];  // моль/м³

    // Woodhull N-сайтовый блок (Mg²⁺ + spermine) — честный inward-rectification.
    float f_open = kir_f_open(V);

    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * f_open;
    double I_K = I_A * A_TO_PA;
    double vol=seg_volume(s);
    double cpv=FARADAY_CONST*vol/1000.0f;
    s->intra_ions[ION_K]-=(I_K*dt)/cpv;
    s->extra_ions[ION_K] += (I_K*dt)/(cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);
    return -I_K;
}

// ==================== Kir2.2 (208): Входящий выпрямитель K⁺ ====================
BIOCORE_DEVICE_CALLABLE double kir22_current(Modifier* m, Segment* s, float dt) {
    // Kir2.2 (KCNJ12) — inward rectifier K⁺, конститутивный. γ=36пС (выше Kir2.1 30).
    float V = s->V_m;
    float area = effective_area(m, s);
    double gamma_ps = (m->state[3] > 0.0f) ? m->state[3] : KIR22_GAMMA_PS;
    float K_i = s->intra_ions[ION_K], K_o = s->extra_ions[ION_K];
    float f_open = kir_f_open(V);  // Woodhull Mg²⁺/spermine (общий механизм rectification)
    // Ba²⁺ блок (state[4], µM) — чувствительнее Kir2.1 (2.8 vs 16.2 µM)
    double ba = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;
    f_open /= (1.0f + ba / KIR22_BA_IC50_UM);
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * f_open;
    double I_K = I_A * A_TO_PA;
    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);
    return -I_K;
}

// ==================== K2P (107): Пассивная K⁺ утечка ====================
BIOCORE_DEVICE_CALLABLE double k2p_leak_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double gamma_ps = (m->state[0] > 0.0f) ? m->state[0] : 100.0f;  // single-channel γ (пС, TREK-1)
    double Po = (m->state[1] > 0.0f) ? m->state[1] : 0.012f;        // open probability (покой, TREK-1)
    float K_i = s->intra_ions[ION_K];  // моль/м³
    float K_o = s->extra_ions[ION_K];
    // Одноканальная проницаемость [м³/с] = γ·RT/(z²F²·c̄_K) — chord-conductance (Hille).
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    // Ток [пА] = P_single·flux·(density·area)·Po  (density=каналов/µm², area=µm²)
    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * Po;
    double I_K = I_A * A_TO_PA;
    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);
    return -I_K;
}

// ==================== KNa (121): Slack/Slick Na⁺-активируемый K⁺ канал ====================
// KCNT1/KCNT2 (Slack/Slick) — K⁺ канал, открывающийся внутриклеточным Na⁺.
// Hill n≈3, EC50≈40mM, tau≈800ms. Slow AHP, terminates long bursts.
BIOCORE_DEVICE_CALLABLE double kna_channel_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double open_prob = m->state[0];
    double EC50 = (m->state[1] > 0.0f) ? m->state[1] : KNA_NA_EC50_MM;  // 41 мМ — Slack (Yuan 2003)
    double gamma_ps = (m->state[3] > 0.0f) ? m->state[3] : KNA_GAMMA_PS;    // γ (пС)
    double tau = (m->state[4] > 0.0f) ? m->state[4] : KNA_TAU_MS;
    // ✅ ДОСТОВЕРНО — Slack (KCNT1); менять ТОЛЬКО с согласования (алгоритм аудита модификаторов).

    // Na⁺-зависимость: Hill по [Na⁺]in (чисто Na⁺-гейт, нет V).
    float Na_in = s->intra_ions[ION_NA];  // моль/м³ (= мМ)
    float Na_n = pow(Na_in / EC50, KNA_HILL);
    float m_inf = Na_n / (1.0f + Na_n);
    float alpha = dt / tau;
    if (alpha > 1.0f) alpha = 1.0f;
    open_prob += (m_inf - open_prob) * alpha;
    if (open_prob < 0.0f) open_prob = 0.0f;
    if (open_prob > 1.0f) open_prob = 1.0f;
    m->state[0] = open_prob;

    float K_i = s->intra_ions[ION_K], K_o = s->extra_ions[ION_K];  // моль/м³
    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * open_prob;
    double I_K = I_A * A_TO_PA;
    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);
    return -I_K;
}

// ==================== Thermal Noise (111): Johnson-Nyquist шум мембраны ====================
// xorshift32 PRNG (идентичен версии в ядрах и .so плагине)
static inline BIOCORE_HOST_DEVICE float xorshift32_rnd(uint32_t seed) {
    uint32_t x = seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return (float)(x & 0x7FFFFFFFu) / (float)0x7FFFFFFFu;
}

BIOCORE_DEVICE_CALLABLE double thermal_noise_current(Modifier* m, Segment* s, float dt) {
    double sigma = (m->state[0] > 0.0f) ? m->state[0] : 0.05f;  // mV RMS
    double seed_off = m->state[1];
    uint32_t seed = (s->id * 7919u) + ((uint32_t)(seed_off * 131.0f));
    seed ^= (uint32_t)(s->V_m * 1000.0f);
    float r = xorshift32_rnd(seed);
    float noise = (r * 2.0f - 1.0f) * sigma * 1.732f;
    float area = effective_area(m, s);
    float C_m = area * 0.01f;
    if (C_m < 1e-4f) C_m = 1e-4f;
    return noise * C_m / dt;  // I_eq (nA)
}

// ==================== Kv7 (KCNQ2/3) M-current (112): Медленный K⁺ ток ====================
// Kv7 (KCNQ2/3) — потенциал-зависимый K⁺ канал с МЕДЛЕННЫМИ кинетиками, БЕЗ инактивации.
// Формирует M-ток (M = muscarinic-sensitive): подавляется Gq/11 → PLCβ → PIP2 depletion.
//
// Ключевая роль: регуляция возбудимости, spike frequency adaptation,
// подпороговый резонанс. Медленная активация (~100ms) — самая медленная среди Kv.
//
// state[0] = m_val (динамическая активация, 0-1)
// state[1] = V_half (mV, default -38) — BIOREF: KCNQ2/3 в нейронах SCG
// state[2] = k_slope (mV, default 10.0)
// state[3] = g_max (nS, default 0.5) — BIOREF: 0.5-2.0 mS/cm²
// state[4] = tau_act (ms, default 100) — BIOREF: 50-200ms
// state[5] = tau_deact (ms, default 60) — BIOREF: 20-100ms
// state[6] = PIP2_factor (0-1, default 1.0)

BIOCORE_DEVICE_CALLABLE double kv7_mcurrent_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    // Параметры кинетики с разумными default
    double m_val = m->state[0];
    double V_half = (m->state[1] != 0.0f) ? m->state[1] : -38.0f;
    double k_slope = (m->state[2] > 0.0f)  ? m->state[2] : 10.0f;
    double tau_act = (m->state[4] > 0.0f)  ? m->state[4] : 100.0f;
    double tau_deact = (m->state[5] > 0.0f)  ? m->state[5] : 60.0f;
    double pip2 = (m->state[6] >= 0.0f) ? m->state[6] : 1.0f;

    // Активация (Больцман, без инактивации — ключевое отличие от Kv4.2)
    float m_inf = 1.0f / (1.0f + exp(-(V - V_half) / k_slope));
    // Voltage-dependent tau: tau_deact при гиперполяризации, tau_act при деполяризации
    float tau_m = tau_deact + (tau_act - tau_deact) * m_inf;
    if (tau_m < 1.0f) tau_m = 1.0f;

    m_val += (m_inf - m_val) * dt / tau_m;
    if (m_val < 0.0f) m_val = 0.0f;
    if (m_val > 1.0f) m_val = 1.0f;
    m->state[0] = m_val;

    // PIP2-зависимость: при PIP2=0 канал закрыт независимо от V
    if (pip2 < 0.01f) return 0.0f;

    // GHK-поток K⁺ (PKC фосфорилирует KCNQ → подавление M-тока)
    float K_i = s->intra_ions[ION_K];  // моль/м³
    float K_o = s->extra_ions[ION_K];
    double P_single = ghk_single_channel_P(7.0f, 1.0f, GHK_CBAR_K_MM);  // γ KCNQ2/3 native ≈ 7 пС (Wang 1998)
    float pkc = segment_pkc_act(s);
    float pkc_suppress = 1.0f - PKC_KV7_INHIBIT * pkc;
    if (pkc_suppress < 0.0f) pkc_suppress = 0.0f;
    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * (m_val*pip2*pkc_suppress);
    double I_K = I_A * A_TO_PA;

    // Ионный баланс K⁺
    double vol  = seg_volume(s);
    double cpv  = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->extra_ions, ION_COUNT);

    return -I_K;  // K⁺ выходит = гиперполяризующий (GHK-масштаб, без 0.01f)
}

// ==================== PP1/Inhibitor-1/DARPP-32 (113): дофамин→LTP-модуляция ====================
// D1→Gs→cAMP→PKA→фосфорилирует DARPP-32 (Thr34) и Inhibitor-1 (Thr35)
// p-DARPP-32 и p-Inhibitor-1 → мощные ингибиторы PP1 (protein phosphatase 1)
// PP1 дефосфорилирует CaMKII-мишени → opposes LTP
// Когда PP1 ингибирован (D1 активен) → LTD подавлен → LTP облегчён
//
// Интеграция с AMPAR trafficking в simulation_host.cpp:
// PP1_active считывается, умножает ltd_rate (PP1 ↑ → больше LTD)
//
// state[0] = p_DARPP32 (0-1, phosphorylated DARPP-32)
// state[1] = p_Inhibitor1 (0-1, phosphorylated Inhibitor-1)
// state[2] = cAMP_level (0-1, input)
// state[3] = PP1_active (0-1, output: 1 = fully active PP1)
// state[4] = tau_phospho (ms, default 5000)
// state[5] = basal_PP1 (default 0.5)
// state[6] = PKA_active (0-1, read from cell state)

BIOCORE_DEVICE_CALLABLE double pp1_inhibitor_darpp32_current(Modifier* m, Segment* s, float dt) {
    double tau = (m->state[4] > 0.0f) ? m->state[4] : 5000.0f;  // slow phosphorylation
    double basal = (m->state[5] > 0.0f) ? m->state[5] : 0.5f;     // basal PP1
    dt *= m->density;  // enzyme_amount

    // ── PKA из честного пула MOD_CAMP_PKA (135) ──
    float PKA_act = segment_pka_act(s);  // PKA активность (0-1)

    // ── PKA phosphorylates DARPP-32 (Thr34) and Inhibitor-1 (Thr35) ──
    float p_DARPP_inf   = PKA_act;  // PKA → p-DARPP-32
    float p_Inhib1_inf  = PKA_act;  // PKA → p-Inhibitor-1

    m->state[0] += (p_DARPP_inf - m->state[0]) * dt / tau;
    m->state[1] += (p_Inhib1_inf - m->state[1]) * dt / tau;

    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    // ── PP1 inhibition: p-DARPP-32 (IC50~1nM) + p-Inhibitor-1 (IC50~1nM) ──
    // PP1_active = basal / (1 + 10*p_DARPP32 + 5*p_Inhibitor1)
    // At rest (DARPP=0, Inh1=0): PP1 = basal/1 = 0.5
    // With full D1 activation (DARPP=1, Inh1=1): PP1 = 0.5/(1+10+5) = 0.5/16 = 0.03
    double inhib_strength = 10.0f * m->state[0] + 5.0f * m->state[1];
    m->state[3] = basal / (1.0f + inhib_strength + 1e-9f);
    m->state[6] = PKA_act;  // expose PKA for other pathways

    return 0.0f;  // signaling only, no current
}

// ==================== MOD_CONNEXIN_CX36 (114): молекулярная биология gap junction ====================
// Cx36 (connexin-36) — основной нейрональный коннексин.
// Синтез в ER → сборка коннексона (6×Cx36 → гексамер) → транспорт → мембрана.
// Полужизнь ~1-3 часа (высокий оборот). Регуляция: фосфорилирование, pH.
//
// state[0] = connexon_count (число коннексонов, 0-1)
// state[1] = p_Cx36 (фосфорилированный Cx36, 0-1)
// state[2] = synthesis_rate (синтез в ER, default 0.001)
// state[3] = degradation_rate (лизосомальная деградация, default 0.0005)
// state[4] = P_open_single (проводимость одиночного канала, default 1.0)
// state[5] = pH_level (локальный pH, default 7.2)
// state[6] = cAMP_level (для PKA-опосредованного фосфорилирования)

BIOCORE_DEVICE_CALLABLE double connexin_cx36_current(Modifier* m, Segment* s, float dt) {
    double synth_rate = (m->state[2] > 0.0f) ? m->state[2] : 0.001f;
    double degrad_rate = (m->state[3] > 0.0f) ? m->state[3] : 0.0005f;
    double pH = (m->state[5] > 4.0f) ? m->state[5] : 7.2f;

    // ── Синтез/деградация коннексонов (медленная динамика, часы) ──
    m->state[0] += (synth_rate - degrad_rate * m->state[0]) * dt / 3600000.0f;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    // ── Фосфорилирование ──
    // CaMKII (активность на сегменте) → фосфорилирует Cx36 → снижает P_open
    float camkii_act = 0.0f;
    for (uint8_t j = 0; j < s->modifier_count; j++)
        if (s->modifiers[j].type == MOD_CAMKII)
            camkii_act = s->modifiers[j].state[0];
    // PKA (cAMP-зависимое) → фосфорилирует Cx36 → повышает P_open
    double pka_drive = m->state[6];  // cAMP уровень

    float p_target = camkii_act * 0.6f + pka_drive * 0.3f;
    m->state[1] += (p_target - m->state[1]) * dt / 5000.0f;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    // ── P_open: фосфорилирование + pH-модуляция ──
    float pH_factor = (pH < 6.8f) ? (pH - 6.0f) / 0.8f : 1.0f;  // закрытие при pH<6.8
    if (pH_factor < 0.0f) pH_factor = 0.0f;
    double camkii_P_open = 1.0f - m->state[1] * 0.5f;    // CaMKII-P → ↓P_open
    double pka_P_open = 1.0f + m->state[1] * 0.3f;    // PKA-P → ↑P_open
    m->state[4] = (camkii_P_open * 0.5f + pka_P_open * 0.5f) * pH_factor;
    if (m->state[4] < 0.05f) m->state[4] = 0.05f;
    if (m->state[4] > 2.0f)  m->state[4] = 2.0f;

    return 0.0f;  // сигнальный модификатор, ток не генерирует
}

// ==================== MOD_CONNEXIN_CX43 (198): молекулярная биология глиального gap junction ====================
// Cx43 (connexin-43, GJA1) — основной коннексин астроцитов/радиальной глии.
// Формирует межклеточный синцитий для пространственного K⁺-буферинга.
// Унитарная проводимость γ ≈ 100 пС (Veenstra 1995) — в 7× выше нейронального Cx36 (14 пС).
// Гейтинг: Vj-зависимый (не реализован — V_m глии однороден), pH-зависимый (закрытие pH<6.8),
// фосфорилирование (CaMKII ↓P_open, PKA ↑P_open). Полужизнь ~1-5 ч.
// Проводимость считается в gap_junction_g_eff: g_gap = density × connexon_count × P_open × γ × 1e-3.
// state[] — та же раскладка, что и Cx36 (см. connexin_cx36_current).
BIOCORE_DEVICE_CALLABLE double connexin_cx43_current(Modifier* m, Segment* s, float dt) {
    double synth_rate = (m->state[2] > 0.0f) ? m->state[2] : 0.001f;
    double degrad_rate = (m->state[3] > 0.0f) ? m->state[3] : 0.0005f;
    double pH = (m->state[5] > 4.0f) ? m->state[5] : 7.2f;

    // ── Синтез/деградация коннексонов (медленная динамика, часы) ──
    m->state[0] += (synth_rate - degrad_rate * m->state[0]) * dt / 3600000.0f;
    if (m->state[0] < 0.0f) m->state[0] = 0.0f;
    if (m->state[0] > 1.0f) m->state[0] = 1.0f;

    // ── Фосфорилирование (CaMKII ↓, PKA ↑ — как Cx36) ──
    float camkii_act = 0.0f;
    for (uint8_t j = 0; j < s->modifier_count; j++)
        if (s->modifiers[j].type == MOD_CAMKII)
            camkii_act = s->modifiers[j].state[0];
    double pka_drive = m->state[6];  // cAMP уровень

    float p_target = camkii_act * 0.6f + pka_drive * 0.3f;
    m->state[1] += (p_target - m->state[1]) * dt / 5000.0f;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;

    // ── P_open: фосфорилирование + pH-модуляция ──
    float pH_factor = (pH < 6.8f) ? (pH - 6.0f) / 0.8f : 1.0f;
    if (pH_factor < 0.0f) pH_factor = 0.0f;
    double camkii_P_open = 1.0f - m->state[1] * 0.5f;
    double pka_P_open = 1.0f + m->state[1] * 0.3f;
    m->state[4] = (camkii_P_open * 0.5f + pka_P_open * 0.5f) * pH_factor;
    if (m->state[4] < 0.05f) m->state[4] = 0.05f;
    if (m->state[4] > 2.0f)  m->state[4] = 2.0f;

    return 0.0f;  // сигнальный модификатор, ток не генерирует
}

// ==================== MOD_CAMP_PKA (135): Gs/Gi → AC → cAMP → PKA ====================
// ✅ ДОСТОВЕРНО — общий пул cAMP/PKA на сегмент (один модификатор). Gs-рецепторы
// стимулируют AC, Gi/o — ингибируют (Gαi). AC: ATP→cAMP (Михаэлис-Ментен по ATP).
// PDE: cAMP→5'-AMP (Михаэлис-Ментен по cAMP). PKA: Hill(cAMP)·(1−depth·desens).
// Молекулярные константы в units.h; V_max — калибровка клетки через density.
// state[0]=[cAMP] µM, state[1]=PKA (0-1), state[2]=desens (0-1),
// state[3..6]=overrides (AC Vmax, PDE Vmax, PKA EC50, Hill). state[7] зарезервирован.
BIOCORE_DEVICE_CALLABLE double camp_pka_current(Modifier* m, Segment* s, float dt) {
    return camp_pka_step(m, s, dt);  // единая реализация — в modifier_helpers.h
}

// ==================== MOD_PLC_IP3_DAG (136): Gq → PLC → IP3 + DAG ====================
// ✅ ДОСТОВЕРНО — общий пул IP3+DAG на сегмент. Gq-рецепторы стимулируют PLC → IP3+DAG (1:1).
// IP3→IP3R (Ca²⁺ из ЭПР), DAG→PKC. state[0]=IP3, [1]=DAG, [2]=desens, [3..4]=overrides.
BIOCORE_DEVICE_CALLABLE double plc_ip3_dag_current(Modifier* m, Segment* s, float dt) {
    return plc_ip3_dag_step(m, s, dt);  // единая реализация — в modifier_helpers.h
}

// ==================== MOD_PKC (137): DAG + Ca²⁺ → PKC-активность ====================
// ✅ ДОСТОВЕРНО — классическая PKC (α/β/γ): DAG (C1-домен) + Ca²⁺ (C2-домен) → активность.
// state[0]=PKC-активность (0-1). Читает DAG из MOD_PLC_IP3_DAG + цитозольный Ca²⁺.
BIOCORE_DEVICE_CALLABLE double pkc_current(Modifier* m, Segment* s, float dt) {
    return pkc_step(m, s, dt);  // единая реализация — в modifier_helpers.h
}

// ПРИМЕЧАНИЕ: init_biological_library() находится в receptors_biological.cpp (полная версия)

// ==================== MOD_GLIAL_KIR41 (109): Kir4.1 глиальный K⁺ канал ====================
// Порт из mod_109_glial_kir41.cpp. Kir4.1 (KCNJ10) — основной K⁺-канал астроцитов.
// Критичен для пространственного K⁺-буферинга: забирает избыток внеклеточного K⁺.
// GHK-поток K⁺ × inward rectification × [K]o-чувствительная проницаемость.
BIOCORE_DEVICE_CALLABLE double glial_kir41_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);

    // Kir4.1 — конститутивный inward rectifier: нет voltage-gated активации.
    // Single-channel P: γ=25 пС (state[3] переопределение) × GHK_GAMMA, как K2P/NALCN.
    double gamma_ps = (m->state[3] > 0.0f) ? m->state[3] : 25.0f;

    float K_i = s->intra_ions[ION_K];  // моль/м³
    float K_o = s->extra_ions[ION_K];

    float E_K = (GAS_CONST * CELSIUS_37 / FARADAY_CONST) * log(K_o / K_i) * 1000.0f;
    float rect = 1.0f / (1.0f + exp((V - E_K) / 12.0f));

    float K_o_baseline = 4.5f;
    float K_o_mM = s->extra_ions[ION_K];
    float ko_factor = sqrtf(K_o_mM / K_o_baseline);
    if (ko_factor < 0.3f) ko_factor = 0.3f;
    if (ko_factor > 5.0f) ko_factor = 5.0f;

    double P_single = ghk_single_channel_P(gamma_ps, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, K_i, K_o);
    double I_A = P_single * flux * (m->density * area) * ko_factor * rect;
    double I_K = I_A * A_TO_PA;

    // Объём глиальной оболочки: 1:1 с плагином (PI * r² * l / 20)
    double vol = seg_volume(s);
    double cpv = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_K] -= (I_K * dt) / cpv;
    s->extra_ions[ION_K] += (I_K * dt) / (cpv * EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT);

    return -I_K;
}

// ==================== MOD_GLIAL_NAK_PUMP (110): Глиальная Na/K-АТФаза (α2) ====================
// Порт из mod_110_glial_nak_pump.cpp. Глиальная Na/K-АТФаза (α2-изоформа).
// Отличия от нейрональной: низкая скорость (0.08 vs 0.15), высокое сродство к K⁺_o (Km=1.0 vs 1.5).
// Замыкает цикл K⁺-буферинга: Kir4.1 забирает K⁺ → синцитий разносит → Na/K-насос возвращает.
BIOCORE_DEVICE_CALLABLE double glial_nak_pump_current(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — глиальная Na/K-АТФаза (α2-изоформа). Тот же цикл Поста–Альберса, что α3,
    // но ниже I_max (2 µA/см²) и выше сродство к K⁺_o (K_m=1.0 мМ). Замыкает цикл K⁺-буферинга.
    float area = effective_area(m, s);

    float v_norm = nak_pump_vnorm(s->V_m, s->intra_ions[ION_NA], s->extra_ions[ION_K], s->ATP_level,
                                  NA_K_PUMP_K_NA_MM, NA_K_PUMP_GLIAL_K_K_MM);

    // ЕДИНИЦА — молекула: I = N·k_cat·v_norm·e·Δz (α2 тот же k_cat, различие — плотность, клетка).
    double N = m->density * area;
    float I_pump = N * v_norm * NA_K_PUMP_TURNOVER_MAX_S * ELEMENTARY_CHARGE_C * A_TO_PA; // пА

    // Перенос ионов: 3 Na⁺ наружу, 2 K⁺ внутрь (стехиометрия цикла).
    float volume = seg_volume(s);
    double cpm = FARADAY_CONST * volume / 1000.0;
    float transport = I_pump * dt / cpm;
    // Ограничить транспорт доступными ионами (как nak_pump_current: 3 Na intra, 2 K extra).
    // Без этого при 12 глиях насос дренирует Na_i/K_o до 0 → v_norm(Na=0,K=0)=NaN.
    float na_avail = (s->intra_ions[ION_NA] - 1e-9f) / 3.0f;
    float k_avail  = (s->extra_ions[ION_K]  - 1e-9f) * EXTRA_VOLUME_RATIO / 2.0f;
    if (na_avail < 0.0f) na_avail = 0.0f;
    if (k_avail  < 0.0f) k_avail  = 0.0f;
    float max_t = (na_avail < k_avail) ? na_avail : k_avail;
    if (transport > max_t) transport = max_t;
    s->intra_ions[ION_NA] -= 3.0f * transport;
    s->extra_ions[ION_NA] += 3.0f * transport / EXTRA_VOLUME_RATIO;
    s->intra_ions[ION_K]  += 2.0f * transport;
    s->extra_ions[ION_K]  -= 2.0f * transport / EXTRA_VOLUME_RATIO;
    clamp_ions(s->intra_ions, ION_COUNT);
    clamp_ions(s->extra_ions, ION_COUNT);

    // ATP: 1 ATP на цикл.
    s->ATP_level -= I_pump * dt * NA_K_PUMP_ATP_RATE_PA;
    if (s->ATP_level < 0.0f) s->ATP_level = 0.0f;

    return -I_pump;  // электрогенный (+1 наружу) → гиперполяризующий
}

void init_arbitrary_library() {
    for (int i = 0; i < MOD_TYPE_COUNT; i++) {
        arbitrary_library[i] = nullptr;
        arbitrary_modulators[i] = nullptr;
    }
}

void set_arbitrary_modulator(int type, ModifierFunc func) {
    if (type >= 0 && type < MOD_TYPE_COUNT)
        arbitrary_library[type] = func;
}

void set_arbitrary_modulator_release(int type, ReleaseModulatorFunc func) {
    if (type >= 0 && type < MOD_TYPE_COUNT)
        arbitrary_modulators[type] = func;
}

// ==================== ER Ca²⁺ handling (v10) ====================

// ==================== ER Ca²⁺ handling (BION v10) ====================

BIOCORE_DEVICE_CALLABLE double serca_current(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — SERCA Ca²⁺-АТФаза ЭПР (Verkhratsky 2005). 2 Ca²⁺ : 1 ATP, Hill-2, K_m=0.27µM.
    float area = effective_area(m, s);
    double K_m_Ca = (m->state[1] > 0.0f) ? m->state[1] : SERCA_K_M_CA_M_M;

    float ca = s->intra_ions[ION_CA];
    if (ca < 1e-9f) ca = 1e-9f;
    float f_Ca = (ca * ca) / (ca * ca + K_m_Ca * K_m_Ca);  // Hill-2

    float atp = s->ATP_level;
    if (atp < 0.0f) atp = 0.0f;
    float f_ATP = atp / (atp + SERCA_K_ATP_MM);  // Michaelis

    // ЕДИНИЦА — молекула: flux = N·k_cat·f_Ca·f_ATP·(2 Ca/цикл)·dt/N_A.
    double N = m->density * area;
    float flux_mol = N * SERCA_TURNOVER_MAX_S * f_Ca * f_ATP * 2.0f * dt * MS_TO_S / AVOGADRO;

    // Δc [мМ] = J / vol[m³]. Ca²⁺ из цитозоля → в ЭПР (2 Ca уже в V_max).
    float dconc = flux_mol / (seg_volume(s) * UM3_TO_M3);
    // Масса сохраняется: перемещаем не больше доступного Ca²⁺ (не уводим ниже пола 1e-9).
    float ca_avail = s->intra_ions[ION_CA] - 1e-9f;
    if (ca_avail < 0.0f) ca_avail = 0.0f;
    float actual = (dconc < ca_avail) ? dconc : ca_avail;
    s->intra_ions[ION_CA] -= actual;
    s->ER_Ca2 += actual;
    clamp_ions(s->intra_ions, ION_COUNT);
    if (s->ER_Ca2 < 0.0f) s->ER_Ca2 = 0.0f;

    // ATP: 1 ATP на 2 Ca²⁺.
    s->ATP_level -= flux_mol * SERCA_ATP_NORM_PER_MOL_CA;
    if (s->ATP_level < 0.0f) s->ATP_level = 0.0f;

    return 0.0f;  // внутриклеточная мембрана ЭПР — нет тока плазмалеммы
}

BIOCORE_DEVICE_CALLABLE double ip3r_current(Modifier* m, Segment* s, float dt) {
    // ✅ ДОСТОВЕРНО — IP3R (Li & Rinzel 1994). IP3 читается из общего пула MOD_PLC_IP3_DAG.
    float IP3 = segment_ip3_mm(s);  // IP3 (мМ) из пула Gq→PLC→IP3
    double h = m->state[1];        // инактивация (0..1)

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
    clamp_ions(s->intra_ions, ION_COUNT);
    if (s->ER_Ca2 < 0.0f) s->ER_Ca2 = 0.0f;

    m->state[1] = h;
    return 0.0f;
}

BIOCORE_DEVICE_CALLABLE double ryr_current(Modifier* m, Segment* s, float dt) {
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
    clamp_ions(s->intra_ions, ION_COUNT);
    if (s->ER_Ca2 < 0.0f) s->ER_Ca2 = 0.0f;

    m->state[0] = w;
    return 0.0f;
}

// ==================== HCN isoforms (129-132): Ih ====================
// Единая формула — modifier_helpers.h::hcn_generic_current (Single Source).
BIOCORE_DEVICE_CALLABLE double hcn1_current(Modifier* m, Segment* s, float dt){return hcn_generic_current(m,s,dt,HCN1_VHALF_MV,HCN1_TAU_OPEN_MS,HCN1_GAMMA_PS);}
BIOCORE_DEVICE_CALLABLE double hcn2_current(Modifier* m, Segment* s, float dt){return hcn_generic_current(m,s,dt,HCN2_VHALF_MV,HCN2_TAU_OPEN_MS,HCN2_GAMMA_PS);}
BIOCORE_DEVICE_CALLABLE double hcn3_current(Modifier* m, Segment* s, float dt){return hcn_generic_current(m,s,dt,HCN3_VHALF_MV,HCN3_TAU_OPEN_MS,HCN3_GAMMA_PS);}
BIOCORE_DEVICE_CALLABLE double hcn4_current(Modifier* m, Segment* s, float dt){return hcn_generic_current(m,s,dt,HCN4_VHALF_MV,HCN4_TAU_OPEN_MS,HCN4_GAMMA_PS);}

// ==================== NALCN (133): Na⁺-leak канал (non-selective) ====================
// NALCN (Sodium Leak Channel, Non-selective): НЕЗАВИСИМЫЙ от напряжения катионный
// канал, преимущественно Na⁺ (P_Na:P_K:P_Ca ≈ 0.6:0.3:0.1). Задаёт покоящуюся
// Na⁺-проводимость и потенциал покоя вместе с K2P. Реверс ≈ 0..-20 мВ.
// Мутации NALCN → тяжёлые неврологические синдромы (критичен для возбудимости покоя).
// Нет гейтинга (всегда открыт): покоящийся Na⁺-вход уравновешивает Na⁺/K⁺-насос.
BIOCORE_DEVICE_CALLABLE double nalcn_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    double gamma_ps = (m->state[0] > 0.0f) ? m->state[0] : 27.0f;  // single-channel γ (пС, Lu/2022)

    // ✅VERIFIED (Lu 2007): P_Na:P_K:P_Ca = 1.3:1.2:0.5 — nonselective (WT).
    // ✅ ДОСТОВЕРНО — менять ТОЛЬКО с согласования с пользователем (алгоритм аудита модификаторов).
    // Нормировано (сумма 3.0): 0.433:0.400:0.167.
    // Парциальные проницаемости [м³/с] (chord-conductance разложение γ по 1.3:1.2:0.5)
    double P_Na, P_K, P_Ca;
    nalcn_partial_P(gamma_ps, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    float Ina = (float)(P_Na * ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]) * N * A_TO_PA);
    float Ik  = (float)(P_K  * ghk_monovalent(V, s->intra_ions[ION_K],  s->extra_ions[ION_K])  * N * A_TO_PA);
    float Ica = (float)(P_Ca * ghk_divalent(V,  s->intra_ions[ION_CA], s->extra_ions[ION_CA]) * N * A_TO_PA);

    double vol = seg_volume(s);
    double cpm = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_NA] -= (Ina * dt) / cpm;   // Na⁺ внутрь (ghk отриц. → -= даёт рост)
    s->intra_ions[ION_K]  -= (Ik  * dt) / cpm;   // K⁺ наружу
    s->intra_ions[ION_CA] -= (Ica * dt) / (2.0f * cpm);  // Ca²⁺ внутрь (z=2)
    // Консервация ионов: внеклетка — резервуар ×10 (как HCN/K2P).
    s->extra_ions[ION_NA] += (Ina * dt) / (cpm * EXTRA_VOLUME_RATIO);
    s->extra_ions[ION_K]  += (Ik  * dt) / (cpm * EXTRA_VOLUME_RATIO);
    s->extra_ions[ION_CA] += (Ica * dt) / (2.0f * cpm * EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT);
    clamp_ions(s->extra_ions, ION_COUNT);

    return -(Ina - Ik + Ica);
}

// ==================== CAN (TRPM4/5): Ca²⁺-активируемый неселективный катионный ====================
// TRPM4/TRPM5: НЕселективный моновалентный Na⁺/K⁺ (НЕ проводит Ca²⁺ — Ca²⁺ активатор, не переносчик).
// Ca²⁺-зависимый Po (Hill n=2, Kd=0.5 µM). P_Na:P_K = 1:1. Реверс ≈ 0 мВ. Инспираторный драйв pre-BötC.
BIOCORE_DEVICE_CALLABLE double can_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float gamma_ps = 30.0f;  // γ ~25–35 пС (TRPM4/5)

    // Ca²⁺-зависимый Po (Hill n=2, Kd=0.5 µM = 5e-4 мМ)
    float ca = s->intra_ions[ION_CA];
    float Kd = 5e-4f;
    float Po = (ca * ca) / (ca * ca + Kd * Kd);
    // Ca²⁺-зависимая десенситизация (инактивация при длительном Ca²⁺, τ~2 с)
    float d_inf = ca / (ca + 0.01f);
    float tau_d = 2000.0f;
    m->state[1] += (d_inf - m->state[1]) * dt / tau_d;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;
    Po *= (1.0f - m->state[1]);  // Po·(1−d)

    // Неселективный Na⁺/K⁺ (P_Na:P_K = 1:1)
    double P_Na, P_K, P_Ca;
    ghk_partial_P(30.0f, 1.0f, 1.0f, 0.0f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double flux_K = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    float Ina = (float)(P_Na * flux_Na * N * Po * A_TO_PA);
    float Ik  = (float)(P_K * flux_K * N * Po * A_TO_PA);

    double vol = seg_volume(s);
    double cpm = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_NA] -= (Ina * dt) / cpm;
    s->intra_ions[ION_K]  -= (Ik  * dt) / cpm;
    s->extra_ions[ION_NA] += (Ina * dt) / (cpm * EXTRA_VOLUME_RATIO);
    s->extra_ions[ION_K]  += (Ik  * dt) / (cpm * EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT);
    clamp_ions(s->extra_ions, ION_COUNT);

    return -(Ina - Ik);
}

// ==================== PKD2L1 (TRPP3): Ca²⁺-предпочитающий механосенсор CSF ====================
// PKD2L1: Ca²⁺-предпочитающий катионный канал (PCa/PNa≈4–6), механо-активируемый потоком CSF.
// γ=120–200 пС. Po = state[4] (0..1, механический стимул). Одиночное открытие 26 пА → ПД (Sternberg 2018).
BIOCORE_DEVICE_CALLABLE double pkd2l1_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m;
    float area = effective_area(m, s);
    float gamma_ps = 150.0f;  // γ 120–200 пС (гомомер/гетеромер PKD2L1)

    // Механо-активация: stim = state[4] (скорость CSF, мкм/с), по умолчанию 0 (закрыт).
    // #32+#40: если state[4] не задан извне — интринсивный цилиарный осциллятор (скорость 0..10 мкм/с).
    // Нормализация скорости→Po через порог (PKD2L1_FLOW_HALF_UM_S, калибровка — порог сдвига не опубликован).
    double flow = (m->state[4] > 0.0f) ? m->state[4] : csf_ciliary_flow(m, dt);
    float stim = flow / (flow + PKD2L1_FLOW_HALF_UM_S);  // Hill n=1, полу-активация
    // Механо-адаптация (инактивация при длительном стимуле, τ~1 с)
    float adapt_inf = stim;
    float tau_a = 1000.0f;
    m->state[1] += (adapt_inf - m->state[1]) * dt / tau_a;
    if (m->state[1] < 0.0f) m->state[1] = 0.0f;
    if (m->state[1] > 1.0f) m->state[1] = 1.0f;
    double Po = stim * (1.0f - m->state[1]);  // stim·(1−adapt)
    if (Po <= 0.0f) return 0.0f;

    // Ca²⁺-предпочитающий (P_Ca:P_Na ≈ 5:1 → норм. 0.83:0.17)
    double P_Na, P_K, P_Ca;
    ghk_partial_P(150.0f, 0.17f, 0.0f, 0.83f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Ca = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    float Ica = (float)(P_Ca * flux_Ca * N * Po * A_TO_PA);
    float Ina = (float)(P_Na * flux_Na * N * Po * A_TO_PA);

    double vol = seg_volume(s);
    double cpm = FARADAY_CONST * vol / 1000.0;
    s->intra_ions[ION_CA] -= (Ica * dt) / (2.0f * cpm);
    s->intra_ions[ION_NA] -= (Ina * dt) / cpm;
    s->extra_ions[ION_CA] += (Ica * dt) / (2.0f * cpm * EXTRA_VOLUME_RATIO);
    s->extra_ions[ION_NA] += (Ina * dt) / (cpm * EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT);
    clamp_ions(s->extra_ions, ION_COUNT);

    return -(Ica + Ina);
}

// ═══════════════════════════════════════════════════════════════════════════════
// 8 НОВЫХ МОДИФИКАТОРОВ (169-176): ноцицепторные Nav + TRP-термосенсоры + KCNQ4.
// Согласованы 2026-09-03 (гейтинг). Справочники: notes/modifier_refs/NAV17_169.md и др.
// ═══════════════════════════════════════════════════════════════════════════════

// ==================== MOD_NAV17 (169): Nav1.7 (SCN9A) TTX-S ноцицепторный Na⁺ ====================
BIOCORE_DEVICE_CALLABLE double nav17_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m; float area = effective_area(m, s);
    float m_inf = 1.0f/(1.0f+exp(-(V - (-24.0f))/5.0f));
    float h_inf = 1.0f/(1.0f+exp((V - (-71.0f))/6.0f));
    m->state[0] += (m_inf - m->state[0]) * (dt/0.5f);
    m->state[1] += (h_inf - m->state[1]) * (dt/1.2f);
    if (m->state[0]<0) m->state[0]=0; if (m->state[0]>1) m->state[0]=1;
    if (m->state[1]<0) m->state[1]=0; if (m->state[1]>1) m->state[1]=1;
    double open = m->state[0]*m->state[0]*m->state[0]*m->state[1];
    double ttx = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    open /= (1.0f + ttx/10.0f);  // TTX-S IC50 ~10 нМ
    double P_single = ghk_single_channel_P(NAV17_GAMMA_PS, 1.0f, GHK_CBAR_NA_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double I_A = P_single * flux * (m->density * area) * open;
    float I = (float)(I_A * A_TO_PA);
    double cpv = FARADAY_CONST * seg_volume(s) / 1000.0f;
    s->intra_ions[ION_NA] -= (I*dt)/cpv; s->extra_ions[ION_NA] += (I*dt)/(cpv*EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT); clamp_ions(s->extra_ions, ION_COUNT);
    return -I;
}

// ==================== MOD_NAV18 (170): Nav1.8 (SCN10A) TTX-R ноцицепторный Na⁺ ====================
BIOCORE_DEVICE_CALLABLE double nav18_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m; float area = effective_area(m, s);
    float m_inf = 1.0f/(1.0f+exp(-(V - (-20.0f))/7.0f));
    float h_inf = 1.0f/(1.0f+exp((V - (-30.0f))/5.0f));
    m->state[0] += (m_inf - m->state[0]) * (dt/1.5f);
    m->state[1] += (h_inf - m->state[1]) * (dt/8.0f);
    if (m->state[0]<0) m->state[0]=0; if (m->state[0]>1) m->state[0]=1;
    if (m->state[1]<0) m->state[1]=0; if (m->state[1]>1) m->state[1]=1;
    double open = m->state[0]*m->state[0]*m->state[0]*m->state[1];
    double ttx = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    open /= (1.0f + ttx/60000.0f);  // TTX-R IC50 ~60 µM
    double P_single = ghk_single_channel_P(NAV18_GAMMA_PS, 1.0f, GHK_CBAR_NA_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double I_A = P_single * flux * (m->density * area) * open;
    float I = (float)(I_A * A_TO_PA);
    double cpv = FARADAY_CONST * seg_volume(s) / 1000.0f;
    s->intra_ions[ION_NA] -= (I*dt)/cpv; s->extra_ions[ION_NA] += (I*dt)/(cpv*EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT); clamp_ions(s->extra_ions, ION_COUNT);
    return -I;
}

// ==================== MOD_NAV19 (171): Nav1.9 (SCN11A) персистентный TTX-R Na⁺ ====================
BIOCORE_DEVICE_CALLABLE double nav19_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m; float area = effective_area(m, s);
    float m_inf = 1.0f/(1.0f+exp(-(V - (-47.0f))/5.5f));
    float h_inf = 1.0f/(1.0f+exp((V - (-44.0f))/6.0f));
    m->state[0] += (m_inf - m->state[0]) * (dt/10.0f);
    m->state[1] += (h_inf - m->state[1]) * (dt/43.0f);
    if (m->state[0]<0) m->state[0]=0; if (m->state[0]>1) m->state[0]=1;
    if (m->state[1]<0) m->state[1]=0; if (m->state[1]>1) m->state[1]=1;
    double open = m->state[0]*m->state[0]*m->state[0]*m->state[1];
    double ttx = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    open /= (1.0f + ttx/39000.0f);  // TTX-R Ki 39 µM
    double P_single = ghk_single_channel_P(NAV19_GAMMA_PS, 1.0f, GHK_CBAR_NA_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double I_A = P_single * flux * (m->density * area) * open;
    float I = (float)(I_A * A_TO_PA);
    double cpv = FARADAY_CONST * seg_volume(s) / 1000.0f;
    s->intra_ions[ION_NA] -= (I*dt)/cpv; s->extra_ions[ION_NA] += (I*dt)/(cpv*EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT); clamp_ions(s->extra_ions, ION_COUNT);
    return -I;
}

// ==================== MOD_TRPV1_CHANNEL (172): тепло/капсаицин, P_Ca/P_Na≈10 ====================
BIOCORE_DEVICE_CALLABLE double trpv1_channel_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m; float area = effective_area(m, s);
    double lig = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;
    float r = lig/(lig + 0.7f);  // капсаицин EC50 0.7 µM
    float Ca = s->intra_ions[ION_CA] * MM_TO_UM;
    float d_inf = Ca/(Ca + 0.5f);  // Ca²⁺-десенситизация Kd 0.5 µM
    m->state[0] += (r - m->state[0]) * (dt/10.0f);
    m->state[1] += (d_inf - m->state[1]) * (dt/150.0f);
    if (m->state[0]<0) m->state[0]=0; if (m->state[0]>1) m->state[0]=1;
    if (m->state[1]<0) m->state[1]=0; if (m->state[1]>1) m->state[1]=1;
    double open = m->state[0] * (1.0f - m->state[1]);
    double P_Na, P_K, P_Ca;
    ghk_partial_P(70.0f, 0.09f, 0.0f, 0.91f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Ca = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    float Ica = (float)(P_Ca * flux_Ca * N * open * A_TO_PA);
    float Ina = (float)(P_Na * flux_Na * N * open * A_TO_PA);
    double cpm = FARADAY_CONST * seg_volume(s) / 1000.0f;
    s->intra_ions[ION_CA] -= (Ica*dt)/(2.0f*cpm); s->intra_ions[ION_NA] -= (Ina*dt)/cpm;
    s->extra_ions[ION_CA] += (Ica*dt)/(2.0f*cpm*EXTRA_VOLUME_RATIO); s->extra_ions[ION_NA] += (Ina*dt)/(cpm*EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT); clamp_ions(s->extra_ions, ION_COUNT);
    return -(Ica + Ina);
}

// ==================== MOD_TRPM8 (173): холод/ментол канал ====================
BIOCORE_DEVICE_CALLABLE double trpm8_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m; float area = effective_area(m, s);
    double lig = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;
    float r = lig/(lig + 30.0f);  // ментол EC50 ~30 µM
    float Ca = s->intra_ions[ION_CA] * MM_TO_UM;
    float d_inf = Ca/(Ca + 0.5f);
    m->state[0] += (r - m->state[0]) * (dt/20.0f);
    m->state[1] += (d_inf - m->state[1]) * (dt/200.0f);
    if (m->state[0]<0) m->state[0]=0; if (m->state[0]>1) m->state[0]=1;
    if (m->state[1]<0) m->state[1]=0; if (m->state[1]>1) m->state[1]=1;
    double open = m->state[0] * (1.0f - m->state[1]);
    double P_Na, P_K, P_Ca;
    ghk_partial_P(63.0f, 0.5f, 0.0f, 0.5f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Ca = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    float Ica = (float)(P_Ca * flux_Ca * N * open * A_TO_PA);
    float Ina = (float)(P_Na * flux_Na * N * open * A_TO_PA);
    double cpm = FARADAY_CONST * seg_volume(s) / 1000.0f;
    s->intra_ions[ION_CA] -= (Ica*dt)/(2.0f*cpm); s->intra_ions[ION_NA] -= (Ina*dt)/cpm;
    s->extra_ions[ION_CA] += (Ica*dt)/(2.0f*cpm*EXTRA_VOLUME_RATIO); s->extra_ions[ION_NA] += (Ina*dt)/(cpm*EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT); clamp_ions(s->extra_ions, ION_COUNT);
    return -(Ica + Ina);
}

// ==================== MOD_TRPA1 (174): ирритант (AITC) канал ====================
BIOCORE_DEVICE_CALLABLE double trpa1_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m; float area = effective_area(m, s);
    double lig = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;
    float r = lig/(lig + 30.0f);  // AITC EC50 ~30 µM
    float Ca = s->intra_ions[ION_CA] * MM_TO_UM;
    float ca_act = Ca/(Ca + 6.0f);  // Ca²⁺-активация EC50 6 µM (анкириновые повторы) — Mrgpr-каскад
    float d_inf = Ca/(Ca + 0.5f);   // Ca²⁺-десенситизация (отдельный процесс)
    m->state[0] += (r - m->state[0]) * (dt/30.0f);
    m->state[1] += (d_inf - m->state[1]) * (dt/300.0f);
    if (m->state[0]<0) m->state[0]=0; if (m->state[0]>1) m->state[0]=1;
    if (m->state[1]<0) m->state[1]=0; if (m->state[1]>1) m->state[1]=1;
    double act = fminf(1.0f, m->state[0] + ca_act);  // AITC ИЛИ Ca²⁺-активация
    double open = act * (1.0f - m->state[1]);
    double P_Na, P_K, P_Ca;
    ghk_partial_P(90.0f, 0.54f, 0.0f, 0.46f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Ca = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    float Ica = (float)(P_Ca * flux_Ca * N * open * A_TO_PA);
    float Ina = (float)(P_Na * flux_Na * N * open * A_TO_PA);
    double cpm = FARADAY_CONST * seg_volume(s) / 1000.0f;
    s->intra_ions[ION_CA] -= (Ica*dt)/(2.0f*cpm); s->intra_ions[ION_NA] -= (Ina*dt)/cpm;
    s->extra_ions[ION_CA] += (Ica*dt)/(2.0f*cpm*EXTRA_VOLUME_RATIO); s->extra_ions[ION_NA] += (Ina*dt)/(cpm*EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT); clamp_ions(s->extra_ions, ION_COUNT);
    return -(Ica + Ina);
}

// ==================== MOD_TRPV3 (175): тепло канал (сенситизация) ====================
BIOCORE_DEVICE_CALLABLE double trpv3_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m; float area = effective_area(m, s);
    double lig = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;
    float r = lig/(lig + 50.0f);  // 2-APB EC50 ~50 µM
    m->state[0] += (r - m->state[0]) * (dt/20.0f);
    if (m->state[0]<0) m->state[0]=0; if (m->state[0]>1) m->state[0]=1;
    double open = m->state[0];  // сенситизация (НЕ десенс.)
    double P_Na, P_K, P_Ca;
    ghk_partial_P(185.0f, 0.5f, 0.0f, 0.5f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Ca = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    float Ica = (float)(P_Ca * flux_Ca * N * open * A_TO_PA);
    float Ina = (float)(P_Na * flux_Na * N * open * A_TO_PA);
    double cpm = FARADAY_CONST * seg_volume(s) / 1000.0f;
    s->intra_ions[ION_CA] -= (Ica*dt)/(2.0f*cpm); s->intra_ions[ION_NA] -= (Ina*dt)/cpm;
    s->extra_ions[ION_CA] += (Ica*dt)/(2.0f*cpm*EXTRA_VOLUME_RATIO); s->extra_ions[ION_NA] += (Ina*dt)/(cpm*EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT); clamp_ions(s->extra_ions, ION_COUNT);
    return -(Ica + Ina);
}

// ==================== MOD_PIEZO2 (189): механочувствительный катионный ====================
// Проприоцепция/лёгкое касание. Механо-стимул из state[4] (мН/м, VirtualBody-мост).
// Po = Hill(стимул, λ50=2 мН/м, k=2.3), rapidly-adapting инактивация τ~6 мс.
// γ=24 пС (Coste 2015), P_Ca:P_Na:P_K=0.40:0.27:0.32 (K проницаемее Na, Gnanasambandam 2015).
BIOCORE_DEVICE_CALLABLE double piezo2_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m; float area = effective_area(m, s);
    double stim = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;   // механо-стимул (мН/м)
    float s23 = pow(stim, 2.3f), l23 = pow(2.0f, 2.3f);
    float po_inf = (stim > 0.0f) ? s23 / (l23 + s23) : 0.0f;  // Hill λ50=2 мН/м
    m->state[0] += (po_inf - m->state[0]) * (dt / 1.0f);      // активация (суб-мс)
    m->state[1] += (po_inf - m->state[1]) * (dt / 6.0f);      // rapidly-adapting (τ~6 мс)
    if (m->state[0] < 0) m->state[0] = 0; if (m->state[0] > 1) m->state[0] = 1;
    if (m->state[1] < 0) m->state[1] = 0; if (m->state[1] > 1) m->state[1] = 1;
    double open = m->state[0] * (1.0f - m->state[1]);
    double P_Na, P_K, P_Ca;
    ghk_partial_P(24.0f, 0.27f, 0.32f, 0.40f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Ca = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    double flux_K = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    float Ica = (float)(P_Ca * flux_Ca * N * open * A_TO_PA);
    float Ik  = (float)(P_K * flux_K * N * open * A_TO_PA);
    float Ina = (float)(P_Na * flux_Na * N * open * A_TO_PA);
    double cpm = FARADAY_CONST * seg_volume(s) / 1000.0f;
    s->intra_ions[ION_CA] -= (Ica*dt)/(2.0f*cpm);
    s->intra_ions[ION_K]  -= (Ik*dt)/cpm;
    s->intra_ions[ION_NA] -= (Ina*dt)/cpm;
    s->extra_ions[ION_CA] += (Ica*dt)/(2.0f*cpm*EXTRA_VOLUME_RATIO);
    s->extra_ions[ION_K]  += (Ik*dt)/(cpm*EXTRA_VOLUME_RATIO);
    s->extra_ions[ION_NA] += (Ina*dt)/(cpm*EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT); clamp_ions(s->extra_ions, ION_COUNT);
    return -(Ica + Ik + Ina);
}

// ==================== MOD_TMC1 (190): MET канал стереоцилий ====================
// Mechanoelectrical transduction волосковых клеток. Механо-стимул (сдвиг пучка) из state[4] (0..1).
// Базальная Po ~10% (tip-link натяжение), адаптация fast Ca²⁺-зависимая (τ~2 мс), slow (myosin) — этап 6.
// γ=80 пС (50–110), P_Ca/P_Na≈3.8 (Beurg 2015).
BIOCORE_DEVICE_CALLABLE double tmc1_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m; float area = effective_area(m, s);
    double stim = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;   // сдвиг пучка 0..1
    float po_inf = 0.1f + 0.9f * (stim / (stim + 0.3f));      // базальная Po 10% + Hill
    m->state[0] += (po_inf - m->state[0]) * (dt / 1.0f);      // активация (суб-мс)
    float Ca = s->intra_ions[ION_CA] * MM_TO_UM;
    float adapt = Ca / (Ca + 5.0f);                            // Ca²⁺-зависимая адаптация
    m->state[1] += (adapt - m->state[1]) * (dt / 2.0f);        // fast (τ~2 мс)
    m->state[2] += (adapt - m->state[2]) * (dt / 45.0f);       // slow (myosin-1c, τ~45 мс — МЫШЬ утрикула PMID 12826658)
    if (m->state[0] < 0) m->state[0] = 0; if (m->state[0] > 1) m->state[0] = 1;
    if (m->state[1] < 0) m->state[1] = 0; if (m->state[1] > 1) m->state[1] = 1;
    if (m->state[2] < 0) m->state[2] = 0; if (m->state[2] > 1) m->state[2] = 1;
    double open = m->state[0] * (1.0f - m->state[1]) * (1.0f - m->state[2]);
    double P_Na, P_K, P_Ca;
    ghk_partial_P(80.0f, 0.21f, 0.0f, 0.79f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Ca = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    float Ica = (float)(P_Ca * flux_Ca * N * open * A_TO_PA);
    float Ina = (float)(P_Na * flux_Na * N * open * A_TO_PA);
    double cpm = FARADAY_CONST * seg_volume(s) / 1000.0f;
    s->intra_ions[ION_CA] -= (Ica*dt)/(2.0f*cpm);
    s->intra_ions[ION_NA] -= (Ina*dt)/cpm;
    s->extra_ions[ION_CA] += (Ica*dt)/(2.0f*cpm*EXTRA_VOLUME_RATIO);
    s->extra_ions[ION_NA] += (Ina*dt)/(cpm*EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT); clamp_ions(s->extra_ions, ION_COUNT);
    return -(Ica + Ina);
}

// ==================== MOD_KCNQ4 (176): Kv7.4 покойный K⁺ волосковых клеток ====================
BIOCORE_DEVICE_CALLABLE double kcnq4_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m; float area = effective_area(m, s);
    float m_inf = 1.0f/(1.0f+exp(-(V - (-78.0f))/14.0f));  // нативный V½ −78 (Kharkovets 2000)
    m->state[0] += (m_inf - m->state[0]) * (dt/212.0f);  // τ_акт 212 мс
    if (m->state[0]<0) m->state[0]=0; if (m->state[0]>1) m->state[0]=1;
    double open = m->state[0];  // без инактивации (Kv7)
    double P_single = ghk_single_channel_P(2.31f, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double I_A = P_single * flux * (m->density * area) * open;
    float I = (float)(I_A * A_TO_PA);
    double cpv = FARADAY_CONST * seg_volume(s) / 1000.0f;
    s->intra_ions[ION_K] -= (I*dt)/cpv; s->extra_ions[ION_K] += (I*dt)/(cpv*EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT); clamp_ions(s->extra_ions, ION_COUNT);
    return -I;
}

// ═══════════════════════════════════════════════════════════════════════════════
// 6 НОВЫХ МОДИФИКАТОРОВ (177-182). Согласованы 2026-09-03. Naₓ(183) — после поиска.
// ═══════════════════════════════════════════════════════════════════════════════

// ==================== MOD_NAV13 (177): Nav1.3 (SCN3A) TTX-S Na⁺ ====================
BIOCORE_DEVICE_CALLABLE double nav13_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m; float area = effective_area(m, s);
    float m_inf = 1.0f/(1.0f+exp(-(V - (-22.0f))/6.5f));   // V½_m −22, k 6.5 (Cummins 2001)
    float h_inf = 1.0f/(1.0f+exp((V - (-64.0f))/6.5f));    // V½_h −64, k 6.5
    m->state[0] += (m_inf - m->state[0]) * (dt/0.5f);
    m->state[1] += (h_inf - m->state[1]) * (dt/0.65f);
    if (m->state[0]<0) m->state[0]=0; if (m->state[0]>1) m->state[0]=1;
    if (m->state[1]<0) m->state[1]=0; if (m->state[1]>1) m->state[1]=1;
    double open = m->state[0]*m->state[0]*m->state[0]*m->state[1];
    double ttx = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    open /= (1.0f + ttx/4.0f);  // TTX-S IC50 4 нМ
    double P_single = ghk_single_channel_P(NAV13_GAMMA_PS, 1.0f, GHK_CBAR_NA_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double I_A = P_single * flux * (m->density * area) * open;
    float I = (float)(I_A * A_TO_PA);
    double cpv = FARADAY_CONST * seg_volume(s) / 1000.0f;
    s->intra_ions[ION_NA] -= (I*dt)/cpv; s->extra_ions[ION_NA] += (I*dt)/(cpv*EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT); clamp_ions(s->extra_ions, ION_COUNT);
    return -I;
}

// ==================== MOD_KIR71 (178): Kir7.1 фоновый K⁺ (γ=0.05 пС) ====================
BIOCORE_DEVICE_CALLABLE double kir71_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m; float area = effective_area(m, s);
    float open = 1.0f;  // time-independent утечка (нет инактивации)
    double P_single = ghk_single_channel_P(KIR71_GAMMA_PS, 1.0f, GHK_CBAR_K_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double I_A = P_single * flux * (m->density * area) * open;
    float I = (float)(I_A * A_TO_PA);
    double cpv = FARADAY_CONST * seg_volume(s) / 1000.0f;
    s->intra_ions[ION_K] -= (I*dt)/cpv; s->extra_ions[ION_K] += (I*dt)/(cpv*EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT); clamp_ions(s->extra_ions, ION_COUNT);
    return -I;
}

// ==================== MOD_TRPC67 (179): DAG-гейт, P_Ca/Na≈5 ====================
BIOCORE_DEVICE_CALLABLE double trpc67_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m; float area = effective_area(m, s);
    double dag = (m->state[4] > 0.0f) ? m->state[4] : 0.0f;  // DAG (Gq/PLC)
    float r = dag/(dag + 10.0f);  // DAG-активация (усл. ед.)
    m->state[0] += (r - m->state[0]) * (dt/50.0f);
    if (m->state[0]<0) m->state[0]=0; if (m->state[0]>1) m->state[0]=1;
    double open = m->state[0];
    double P_Na, P_K, P_Ca;
    ghk_partial_P(TRPC67_GAMMA_PS, 0.17f, 0.0f, 0.83f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Ca = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    float Ica = (float)(P_Ca * flux_Ca * N * open * A_TO_PA);
    float Ina = (float)(P_Na * flux_Na * N * open * A_TO_PA);
    double cpm = FARADAY_CONST * seg_volume(s) / 1000.0f;
    s->intra_ions[ION_CA] -= (Ica*dt)/(2.0f*cpm); s->intra_ions[ION_NA] -= (Ina*dt)/cpm;
    s->extra_ions[ION_CA] += (Ica*dt)/(2.0f*cpm*EXTRA_VOLUME_RATIO); s->extra_ions[ION_NA] += (Ina*dt)/(cpm*EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT); clamp_ions(s->extra_ions, ION_COUNT);
    return -(Ica + Ina);
}

// ==================== MOD_CALHM13 (180): V-зависимая АТФ-пора ====================
BIOCORE_DEVICE_CALLABLE double calhm13_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m; float area = effective_area(m, s);
    float m_inf = 1.0f/(1.0f+exp(-(V - 30.5f)/8.0f));  // V½ +30.5
    m->state[0] += (m_inf - m->state[0]) * (dt/20.0f);
    if (m->state[0]<0) m->state[0]=0; if (m->state[0]>1) m->state[0]=1;
    double open = m->state[0];
    // γ=25 пС (Ma 2012 PNAS), слабо селективна: P_Na:P_K=1:1.17, P_Ca/P_Na≈10 (Cl 0.56 — анион, малый вклад пропущен)
    double P_Na, P_K, P_Ca;
    ghk_partial_P(CALHM13_GAMMA_PS, 1.0f, 1.17f, 10.0f, &P_Na, &P_K, &P_Ca);
    double N = m->density * area;
    double flux_Na = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double flux_K = ghk_monovalent(V, s->intra_ions[ION_K], s->extra_ions[ION_K]);
    double flux_Ca = ghk_divalent(V, s->intra_ions[ION_CA], s->extra_ions[ION_CA]);
    float Ina = (float)(P_Na * flux_Na * N * open * A_TO_PA);
    float Ik  = (float)(P_K * flux_K * N * open * A_TO_PA);
    float Ica = (float)(P_Ca * flux_Ca * N * open * A_TO_PA);
    double cpv = FARADAY_CONST * seg_volume(s) / 1000.0f;
    s->intra_ions[ION_NA] -= (Ina*dt)/cpv; s->intra_ions[ION_K] -= (Ik*dt)/cpv; s->intra_ions[ION_CA] -= (Ica*dt)/(2.0f*cpv);
    s->extra_ions[ION_NA] += (Ina*dt)/(cpv*EXTRA_VOLUME_RATIO); s->extra_ions[ION_K] += (Ik*dt)/(cpv*EXTRA_VOLUME_RATIO); s->extra_ions[ION_CA] += (Ica*dt)/(2.0f*cpv*EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT); clamp_ions(s->extra_ions, ION_COUNT);
    return -(Ina + Ik + Ica);
}

// ==================== MOD_OTOP1 (181): H⁺-гейт (pH<6) ====================
BIOCORE_DEVICE_CALLABLE double otop1_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m; float area = effective_area(m, s);
    double ph = (m->state[4] > 0.0f) ? m->state[4] : 7.4f;  // pH внеклет.
    float r = (ph < 6.0f) ? 1.0f : 0.0f;  // порог pH<6 (крутая активация)
    m->state[0] += (r - m->state[0]) * (dt/10.0f);
    if (m->state[0]<0) m->state[0]=0; if (m->state[0]>1) m->state[0]=1;
    double open = m->state[0];
    // Одномолекулярная GHK H⁺: [H⁺]_o из pH (state[4]) → 10^(3−pH) мМ.
    double c_H_in  = s->intra_ions[ION_H];      // pH 7.2 → 6.3e-5 мМ
    double c_H_out = pow(10.0, 3.0 - ph);       // pH<6 → >1e-3 мМ (движущая сила H⁺-входа)
    double P_single = ghk_single_channel_P(OTOP1_GAMMA_PS, 1.0f, GHK_CBAR_H_MM);
    double flux = ghk_monovalent(V, c_H_in, c_H_out);
    double I_A = P_single * flux * (m->density * area) * open;
    float I = (float)(I_A * A_TO_PA);
    double zn = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;  // Zn²⁺ (µМ)
    I /= (1.0f + zn / OTOP1_ZN_IC50_UM);  // Zn²⁺ IC50 0.19 мМ (Tu 2018)
    // Ионная консервация H⁺ (входящий H⁺ = деполяризующий).
    double vol = segment_volume(s);
    double cpm = charge_per_mole(vol);
    s->intra_ions[ION_H] -= (I * dt) / cpm;
    s->extra_ions[ION_H] += (I * dt) / (cpm * EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT);
    clamp_ions(s->extra_ions, ION_COUNT);
    return -I;
}

// ==================== MOD_ENAC (182): Na⁺-утечка (γ 4-5, Po 0.5), амилорид ====================
BIOCORE_DEVICE_CALLABLE double enac_current(Modifier* m, Segment* s, float dt) {
    float V = s->V_m; float area = effective_area(m, s);
    float open = ENAC_PO;  // конститутивный Po ~0.5
    double amil = (m->state[5] > 0.0f) ? m->state[5] : 0.0f;
    open /= (1.0f + amil/100.0f);  // амилорид IC50 100 нМ
    double P_single = ghk_single_channel_P(ENAC_GAMMA_PS, 1.0f, GHK_CBAR_NA_MM);
    double flux = ghk_monovalent(V, s->intra_ions[ION_NA], s->extra_ions[ION_NA]);
    double I_A = P_single * flux * (m->density * area) * open;
    float I = (float)(I_A * A_TO_PA);
    double cpv = FARADAY_CONST * seg_volume(s) / 1000.0f;
    s->intra_ions[ION_NA] -= (I*dt)/cpv; s->extra_ions[ION_NA] += (I*dt)/(cpv*EXTRA_VOLUME_RATIO);
    clamp_ions(s->intra_ions, ION_COUNT); clamp_ions(s->extra_ions, ION_COUNT);
    return -I;
}

// ==================== MOD_AQP4 (186): аквапорин-4 — водный канал (не ионный) ====================
// Осмотический транспорт: вода входит в гипертоничную клетку → ионы разбавляются →
// Δπ падает → поток затухает (равновесие Σ[intra]=Σ[extra]). Самодостаточный (без BION bump).
// P_f single-channel = 24e-14 см³/с (Yang & Verkman 1997, PMID 9195910). См. AQP4_186.md.
BIOCORE_DEVICE_CALLABLE double aqp4_current(Modifier* m, Segment* s, float dt) {
    float area = effective_area(m, s);
    // Осмотический градиент (мМ ≈ мОсм/л): разница суммарных концентраций intra/extra
    float sum_intra = 0.0f, sum_extra = 0.0f;
    for (int i = 0; i < ION_COUNT; i++) { sum_intra += s->intra_ions[i]; sum_extra += s->extra_ions[i]; }
    float dpi = sum_intra - sum_extra;
    // J_water (фракция объёма/мс) = density · P_f · area · Δπ · scale.
    // P_f single-channel = 24e-14 см³/с; mean-field scale → калибровка этап 6.
    double J = m->density * AQP4_PF_CM3_S * area * dpi * AQP4_OSMOTIC_SCALE;
    // Разбавление: вода входит (Δπ>0) → объём растёт → ионы разбавляются (factor<1)
    float factor = 1.0f / (1.0f + J * dt);
    if (factor < 0.95f) factor = 0.95f;   // ограничение (не более 5%/тик)
    if (factor > 1.0f)  factor = 1.0f;    // вода только входит при Δπ>0
    for (int i = 0; i < ION_COUNT; i++) s->intra_ions[i] *= factor;
    return 0.0f;  // нет ионного тока (только водный поток)
}

// ==================== MOD_CILIA (187): цилиарный генератор CSF-потока ====================
// Эпендима: биение ресничек → скалярный поток 0..1 (state[0]). #32+#40 минимальная версия.
// КАК СДЕЛАНО: скалярный поток (state[0]), PKD2L1 читает state[4] (механо-стимул). Нет ионного тока.
// КАК ДОЛЖНО БЫТЬ: гидродинамическое поле (вектор скорости + сдвиг на ресничках) — см. CSF_FLOW.md.
BIOCORE_DEVICE_CALLABLE double cilia_current(Modifier* m, Segment* s, float dt) {
    (void)s;
    m->state[0] = csf_ciliary_flow(m, dt);  // 12 Гц (5–20 Гц лягушка) → скалярный поток 0..1
    return 0.0f;  // нет ионного тока (структурный/генераторный модификатор)
}
