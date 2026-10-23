// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// diffusion_kernel.h — ЕДИНЫЙ шаг 2.5 (межсегментная электродиффузия) для CPU и GPU-хоста.
// Внутриклеточный Нернст-Планк (сабстеппинг) + внеклеточная диффузия + NT/ATP/температура/
// транскрипция. SINGLE SOURCE: вызывается из simulation_host.cpp (CPU) и simulation.cpp
// (SYCL host-петля после q.wait()).
// HOST-ONLY: использует std::vector → НЕ подключать в device-код (.cu / .sycl).

#pragma once
#include "run_tick_common.h"   // segment_capacitance, NP_*, DIFF_*
#include <vector>
#include <algorithm>
#include <cstdio>

// ── #42 ионодинамика: внеклеточный потенциал φ_e из мембранных токов ──
// ∇·(σ_e·∇φ_e) = −I_m, где σ_e — внеклеточная проводимость, вычисленная из ионов
// (подвижность Нернст-Планка): σ_e = F²/(RT)·Σ z_i²·D_i·[i]. Это макроскопический предел
// полного PNP (электронейтральность + Дебаевское экранирование на ~1 нм).
// Дискретно: (g_bath,i + Σ_j g_e,ij)·φ_i − Σ_j g_e,ij·φ_j = −I_m,i.
// g_bath = σ_e·A/d_bath (связь с bath/землёй), g_e,ij = σ_e·area_conn/dist (внеклеточный зазор).
// Источник I_m,i = segments[i].I_buffer (суммарный мембранный ток, шаг 1).
#define SIGMA_E_FACTOR 3.612f   // F²/(RT)·1e3 → σ_e[пА/(мВ·мкм)] из Σ z²·D[мкм²/мс]·c[мМ]
#define NP_JACOBI_ITER 50       // Якоби-итерации полу-неявного осевого V_m-уравнения
#define D_BATH_UM      1.0f    // характерное расстояние до bath (мкм) — калибровка этап 6 (было 10, NaN-фикс: сильнее заземление)
#define PHI_E_ITER     50       // Якоби-итерации (граф сегментов)

// Bath-релаксация концентраций extra_ions к физиологическому резервуару (CSF/кровь).
// Внеклетка = бесконечный резервуар, держит [Na]=140, [K]=3, [Cl]=130, [Ca]=1.2, [Mg]=1.
// Без этого extra-ионы дрейфуют к заряд-нейтральному «солевому» равновесию (~500 мМ) → runaway.
static const float BATH_ION[ION_COUNT] = { 140.0f, 3.0f, 1.2f, 130.0f, 1.0f, 0.0f, 4.0e-5f };
#define BATH_TAU_MS     1.0f    // время выравнивания с резервуаром (мс) — калибровка

static inline float segment_area_compute(const Segment* s) {
    if (s->segment_type == 1 || s->segment_type == 3 || s->segment_type == 4)
        return 4.0f * PI * s->radius * s->radius;   // сома/бутон/шипик — сфера
    return 2.0f * PI * s->radius * s->length;       // аксон/дендрит/AIS — цилиндр
}

static inline void solve_phi_e(Segment* segments, uint32_t num_segments,
                               IntraConnection* connections, uint32_t num_connections,
                               std::vector<double>& phi_e) {
    phi_e.assign(num_segments, 0.0);
    if (num_segments == 0) return;

    const float np_z[ION_COUNT] = { NP_Z_NA, NP_Z_K, NP_Z_CA, NP_Z_CL, NP_Z_MG, NP_Z_A, NP_Z_H };
    const float diff_coeff[ION_COUNT] = { DIFF_NA, DIFF_K, DIFF_CA, DIFF_CL, DIFF_MG, DIFF_A, DIFF_H };

    static std::vector<double> sigma_e, g_bath, diag, phi_new, g_offdiag;
    sigma_e.assign(num_segments, 0.0);
    g_bath.assign(num_segments, 0.0);
    diag.assign(num_segments, 0.0);
    phi_new.assign(num_segments, 0.0);
    g_offdiag.assign(num_connections, 0.0);

    // Внеклеточная проводимость из ионов (подвижность): σ_e = F²/(RT)·Σ z²·D·c.
    for (uint32_t i = 0; i < num_segments; i++) {
        double s = 0.0;
        for (int ion = 0; ion < ION_COUNT; ion++)
            s += np_z[ion] * np_z[ion] * diff_coeff[ion] * segments[i].extra_ions[ion];
        sigma_e[i] = SIGMA_E_FACTOR * s;
        g_bath[i] = sigma_e[i] * segment_area_compute(&segments[i]) / D_BATH_UM;
        diag[i] = g_bath[i];
    }
    // Внеклеточные связи (все, включая EXTRA_ONLY): g_e,ij = σ_e·area_conn/dist.
    for (uint32_t i = 0; i < num_connections; i++) {
        uint32_t a = connections[i].segment_a, b = connections[i].segment_b;
        if (a >= num_segments || b >= num_segments) continue;
        float r_min = segments[a].radius < segments[b].radius ? segments[a].radius : segments[b].radius;
        float area_conn = PI * r_min * r_min;
        if (area_conn < 1e-6f) area_conn = 1e-6f;
        float dist = (segments[a].length + segments[b].length) * 0.5f;
        if (dist < 0.1f) dist = 0.1f;
        double g = 0.5 * (sigma_e[a] + sigma_e[b]) * area_conn / dist;
        g_offdiag[i] = g;
        diag[a] += g; diag[b] += g;
    }

    // Якоби: φ_i = (Σ_j g_e,ij·φ_j − I_m,i) / diag_i.
    for (int iter = 0; iter < PHI_E_ITER; iter++) {
        phi_new.assign(num_segments, 0.0);
        for (uint32_t i = 0; i < num_connections; i++) {
            uint32_t a = connections[i].segment_a, b = connections[i].segment_b;
            if (a >= num_segments || b >= num_segments) continue;
            double g = g_offdiag[i];
            if (g <= 0.0) continue;
            phi_new[a] += g * phi_e[b];
            phi_new[b] += g * phi_e[a];
        }
        for (uint32_t i = 0; i < num_segments; i++) {
            phi_e[i] = (diag[i] > 1e-9) ? (phi_new[i] - segments[i].I_buffer) / diag[i] : 0.0;
            if (phi_e[i] > 10.0) phi_e[i] = 10.0;   // cap: φ_e ~мВ (полевой потенциал), иначе дрейф раздувает ионы
            if (phi_e[i] < -10.0) phi_e[i] = -10.0;
        }
    }
    // Диагностика: max |phi_e| (расходимость Якоби)
    double mphi = 0.0; double mib = 0.0;
    for (uint32_t i = 0; i < num_segments; i++) {
        double a = phi_e[i]; if (a < 0) a = -a; if (a > mphi) mphi = a;
        double b = segments[i].I_buffer; if (b < 0) b = -b; if (b > mib) mib = b;
    }
    if (mphi > 50.0) printf("[PHI-E] max|phi_e|=%.3g max|I_buffer|=%.3g\n", mphi, mib);
}

// Бернулли B(x) = x/(e^x − 1) — для Scharfetter-Gummel (устойчивая дискретизация Нернст-Планка).
// Устойчива при больших |x|: B(+∞)→0, B(−∞)→−x. Small-x: B(x)≈1−x/2.
static inline float sg_bernoulli(float x) {
    if (fabsf(x) < 1e-3f) return 1.0f - 0.5f * x;  // small-x: 1−x/2 (иначе exp(x)−1 округляется в 0 при x~1e-6)
    if (x > 50.0f) return 0.0f;   // B(x)→0 (было x·exp(−x) → Inf·0=NaN при x=Inf)
    if (x < -50.0f) return -x;    // → −x
    return x / (exp(x) - 1.0f);
}

static inline void run_diffusion_step(Segment* segments, uint32_t num_segments,
                                      IntraConnection* connections, uint32_t num_connections,
                                      float dt) {
    if (num_connections == 0) return;

    float diff_coeff[ION_COUNT] = { DIFF_NA, DIFF_K, DIFF_CA, DIFF_CL, DIFF_MG, DIFF_A, DIFF_H };

    // Temp-буферы для накопления изменений концентраций
    // (двухпроходное ядро ≡ parallel atomicAdd, static — без переаллокаций)
    static std::vector<float> intra_nt_delta;
    static std::vector<float> nt_delta, atp_delta, temp_delta, txn_delta;
    static std::vector<float> g_diag, g_offdiag, i_np_diff, V_old, V_new, offdiag;
    static std::vector<double> intra_delta_acc;  // double: накопление за тик (float теряет диффузию)
    static std::vector<double> extra_delta;      // double: внеклеточные ионы тоже (иначе NaN на масштабе)
    extra_delta.assign(num_segments * ION_COUNT, 0.0);
    intra_delta_acc.assign(num_segments * ION_COUNT, 0.0);
    intra_nt_delta.assign(num_segments * NT_COUNT, 0.0f);
    nt_delta.assign(num_segments * NT_COUNT, 0.0f);
    atp_delta.assign(num_segments, 0.0f);
    temp_delta.assign(num_segments, 0.0f);
    txn_delta.assign(num_segments, 0.0f);
    g_diag.assign(num_segments, 0.0f);
    g_offdiag.assign(num_connections, 0.0f);
    i_np_diff.assign(num_segments, 0.0f);
    V_old.assign(num_segments, 0.0f);
    V_new.assign(num_segments, 0.0f);
    offdiag.assign(num_segments, 0.0f);

    // #42 volume conductor: внеклеточный потенциал φ_e из мембранных токов (перед флюксом)
    static std::vector<double> phi_e;
    solve_phi_e(segments, num_segments, connections, num_connections, phi_e);

    // ── Внутриклеточная электродиффузия (Нернст-Планк), полу-неявная ──
    // Диффузия (Scharfetter-Gummel flux) → концентрации (явная, медленная/стабильная).
    // Электромиграция (заряд-флюкс I_np) → V_m ПОЛУ-НЕЯВНО: полный I_np(V^n) явно +
    // chord-conductance g_axial·(V^{n+1}−V^n) неявно (Newton-1 шаг), без двойного счёта.
    {
        const float np_z[ION_COUNT] = { NP_Z_NA, NP_Z_K, NP_Z_CA, NP_Z_CL, NP_Z_MG, NP_Z_A, NP_Z_H };
        // Сабстеппинг внутриклеточной осевой связи: при dt=0.1 резкий ПД (dV ~100 мВ/тик)
        // не передаётся сома→аксон за один полу-неявный Якоби-шаг (split-step с явным Эйлером
        // шага 4) → бутон аксона не видит спайка → bouton_Ca не растёт → нет глицина → нет торможения.
        const int INTRA_SUBSTEPS = 5;
        const float dt_sub = dt / INTRA_SUBSTEPS;

        for (int sub = 0; sub < INTRA_SUBSTEPS; sub++) {
            std::fill(intra_delta_acc.begin(), intra_delta_acc.end(), 0.0);
            std::fill(g_diag.begin(), g_diag.end(), 0.0f);
            std::fill(i_np_diff.begin(), i_np_diff.end(), 0.0f);

            // Шаг A: Scharfetter-Gummel flux → концентрации + полный I_np + chord g_axial
            for (uint32_t i = 0; i < num_connections; i++) {
                IntraConnection* conn = &connections[i];
                uint32_t a = conn->segment_a;
                uint32_t b = conn->segment_b;
                if (a >= num_segments || b >= num_segments) continue;
                if (conn->is_intracellular == 0) continue;  // только внутриклеточные (осевая связь)

                float r_min = segments[a].radius < segments[b].radius
                            ? segments[a].radius : segments[b].radius;
                float area_conn = PI * r_min * r_min;
                if (area_conn < 1e-6f) area_conn = 1e-6f;
                float dist = (segments[a].length + segments[b].length) * 0.5f;
                if (dist < 0.1f) dist = 0.1f;
                float vol_a = segment_volume_compute(&segments[a]);
                float vol_b = segment_volume_compute(&segments[b]);
                if (vol_a < 1e-6f) vol_a = 1e-6f;
                if (vol_b < 1e-6f) vol_b = 1e-6f;

                // chord-conductance g_axial = σ_i·A/L, σ_i = F²/(RT)·Σ z²·D·c (внутриклеточная проводимость)
                float sigma_i = 0.0f;
                for (int ion = 0; ion < ION_COUNT; ion++) {
                    float C_bar = 0.5f * (segments[a].intra_ions[ion] + segments[b].intra_ions[ion]);
                    sigma_i += np_z[ion] * np_z[ion] * diff_coeff[ion] * C_bar;
                }
                float g_ax = SIGMA_E_FACTOR * sigma_i * area_conn / dist;  // нС
                g_offdiag[i] = g_ax;
                g_diag[a] += g_ax;
                g_diag[b] += g_ax;

                // Scharfetter-Gummel flux → концентрации + полный заряд-флюкс I_np
                float dV = (segments[a].V_m + phi_e[a]) - (segments[b].V_m + phi_e[b]);  // ∇φ_i = ∇V_m + ∇φ_e (#15)
                float I_np = 0.0f;
                for (int ion = 0; ion < ION_COUNT; ion++) {
                    float C_a = segments[a].intra_ions[ion];
                    float C_b = segments[b].intra_ions[ion];
                    float x = np_z[ion] * NP_F_RT_MV * dV;
                    float flux = diff_coeff[ion] * area_conn * dt_sub / dist * (C_a * sg_bernoulli(-x) - C_b * sg_bernoulli(x));
                    intra_delta_acc[a * ION_COUNT + ion] -= (double)flux / vol_a;
                    intra_delta_acc[b * ION_COUNT + ion] += (double)flux / vol_b;
                    I_np += np_z[ion] * NP_CHARGE_TO_PA * flux / dt_sub;
                }
                // Диффузионный заряд = полный − электромиграционный (вычитаем chord-ток на V^n).
                i_np_diff[a] += I_np - g_ax * dV;
                i_np_diff[b] -= I_np - g_ax * dV;
            }

            // Шаг B: полу-неявный backward-Euler для V_m (Якоби)
            // V_i^{n+1} = (C_i/dt·V_i^n + i_np_diff_i + Σ_j g_ij·V_j^{n+1}) / (C_i/dt + Σ_j g_ij)
            for (uint32_t i = 0; i < num_segments; i++) { V_old[i] = segments[i].V_m; V_new[i] = segments[i].V_m; }
            for (int iter = 0; iter < NP_JACOBI_ITER; iter++) {
                std::fill(offdiag.begin(), offdiag.end(), 0.0f);
                for (uint32_t i = 0; i < num_connections; i++) {
                    IntraConnection* conn = &connections[i];
                    uint32_t a = conn->segment_a;
                    uint32_t b = conn->segment_b;
                    if (a >= num_segments || b >= num_segments) continue;
                    if (conn->is_intracellular == 0) continue;
                    offdiag[a] += g_offdiag[i] * V_new[b];
                    offdiag[b] += g_offdiag[i] * V_new[a];
                }
                for (uint32_t i = 0; i < num_segments; i++) {
                    float C_dt = segment_capacitance(&segments[i]) / dt_sub;  // пФ/мс = нС
                    if (C_dt < 1e-9f) C_dt = 1e-9f;
                    V_new[i] = (C_dt * V_old[i] + i_np_diff[i] + offdiag[i]) / (C_dt + g_diag[i]);
                }
            }
            for (uint32_t i = 0; i < num_segments; i++) segments[i].V_m = V_new[i];

            // Применить накопленную ионную delta (внутри подшага — g_axial из свежих концентраций)
            for (uint32_t i = 0; i < num_segments; i++)
                for (int ion = 0; ion < ION_COUNT; ion++)
                    segments[i].intra_ions[ion] += (float)intra_delta_acc[i * ION_COUNT + ion];
        }
    }

    // ── Внеклеточная ионная диффузия + электромиграция (#15 PNP), субстеппинг ──
    // Явный forward-Euler при dt=0.1 нарушает CFL на многосвязных сегментах (глия↔все:
    // D·dt·degree/(dist·vol·EVR) > 0.5) → extra_ions уходят в минус → нижний clamp
    // уничтожает отрицательный K → НЕКОНСЕРВАТИВНОСТЬ (net K-источник) → флип. Субстеппинг
    // устраняет перелёт, ионы не уходят в минус, консервация восстанавливается.
    {
        const int EXTRA_SUBSTEPS = 5;
        const float dt_sub = dt / EXTRA_SUBSTEPS;
        const float np_z[ION_COUNT] = { NP_Z_NA, NP_Z_K, NP_Z_CA, NP_Z_CL, NP_Z_MG, NP_Z_A, NP_Z_H };
        for (int sub = 0; sub < EXTRA_SUBSTEPS; sub++) {
            std::fill(extra_delta.begin(), extra_delta.end(), 0.0);
            for (uint32_t i = 0; i < num_connections; i++) {
                IntraConnection* conn = &connections[i];
                uint32_t a = conn->segment_a;
                uint32_t b = conn->segment_b;
                if (a >= num_segments || b >= num_segments) continue;

                float r_min = segments[a].radius < segments[b].radius ? segments[a].radius : segments[b].radius;
                float area_conn = PI * r_min * r_min;
                if (area_conn < 1e-6f) area_conn = 1e-6f;
                float dist = (segments[a].length + segments[b].length) * 0.5f;
                if (dist < 0.1f) dist = 0.1f;
                float vol_a = segment_volume_compute(&segments[a]);
                float vol_b = segment_volume_compute(&segments[b]);
                if (vol_a < 1e-6f) vol_a = 1e-6f;
                if (vol_b < 1e-6f) vol_b = 1e-6f;

                for (int ion = 0; ion < ION_COUNT; ion++) {
                    double Ca = segments[a].extra_ions[ion];
                    double Cb = segments[b].extra_ions[ion];
                    double dphi = phi_e[a] - phi_e[b];
                    float x = np_z[ion] * NP_F_RT_MV * dphi;
                    double flux = diff_coeff[ion] * area_conn * dt_sub / dist * (Ca * sg_bernoulli(-x) - Cb * sg_bernoulli(x));
                    extra_delta[a * ION_COUNT + ion] -= flux / (vol_a * EXTRA_VOLUME_RATIO);
                    extra_delta[b * ION_COUNT + ion] += flux / (vol_b * EXTRA_VOLUME_RATIO);
                }
            }
            for (uint32_t i = 0; i < num_segments; i++)
                for (int ion = 0; ion < ION_COUNT; ion++)
                    segments[i].extra_ions[ion] += extra_delta[i * ION_COUNT + ion];
        }
    }

    // ── Первый проход: NT/ATP/температура/транскрипция (внеклеточные ионы — в субстеппинге выше) ──
    for (uint32_t i = 0; i < num_connections; i++) {
        IntraConnection* conn = &connections[i];
        uint32_t a = conn->segment_a;
        uint32_t b = conn->segment_b;
        if (a >= num_segments || b >= num_segments) continue;

        float r_min = segments[a].radius < segments[b].radius
                    ? segments[a].radius : segments[b].radius;
        float area_conn = PI * r_min * r_min;
        if (area_conn < 1e-6f) area_conn = 1e-6f;

        float dist = (segments[a].length + segments[b].length) * 0.5f;
        if (dist < 0.1f) dist = 0.1f;

        float vol_a = segment_volume_compute(&segments[a]);
        float vol_b = segment_volume_compute(&segments[b]);
        if (vol_a < 1e-6f) vol_a = 1e-6f;
        if (vol_b < 1e-6f) vol_b = 1e-6f;

        bool extra_only = (conn->is_intracellular == 0);

        if (!extra_only) {
            for (int nt = 0; nt < NT_COUNT; nt++) {
                float dC = segments[a].intra_nt[nt] - segments[b].intra_nt[nt];
                if (dC < 1e-12f && dC > -1e-12f) continue;
                float D = nt_diffusion_coeff(nt);
                float flux = D * area_conn * dC * dt / dist;
                intra_nt_delta[a * NT_COUNT + nt] -= flux / vol_a;
                intra_nt_delta[b * NT_COUNT + nt] += flux / vol_b;
            }
        }

        for (int nt = 0; nt < NT_COUNT; nt++) {
            float dC = segments[a].neurotransmitter[nt] - segments[b].neurotransmitter[nt];
            if (dC < 1e-12f && dC > -1e-12f) continue;
            float D = nt_diffusion_coeff(nt);
            float flux = D * area_conn * dC * dt / dist;
            nt_delta[a * NT_COUNT + nt] -= flux / vol_a;
            nt_delta[b * NT_COUNT + nt] += flux / vol_b;
        }

        if (!extra_only) {
            float dATP = segments[a].ATP_level - segments[b].ATP_level;
            if (dATP > 1e-12f || dATP < -1e-12f) {
                float flux_atp = DIFF_ATP * area_conn * dATP * dt / dist;
                atp_delta[a] -= flux_atp / vol_a;
                atp_delta[b] += flux_atp / vol_b;
            }
            float dT = segments[a].temperature - segments[b].temperature;
            if (dT > 0.001f || dT < -0.001f) {
                float flux_heat = DIFF_HEAT * area_conn * dT * dt / dist;
                temp_delta[a] -= flux_heat / vol_a;
                temp_delta[b] += flux_heat / vol_b;
            }
            float dTxn = segments[a].transcription_signal - segments[b].transcription_signal;
            if (dTxn > 1e-9f || dTxn < -1e-9f) {
                float flux_txn = 0.01f * area_conn * dTxn * dt / dist;
                txn_delta[a] -= flux_txn / vol_a;
                txn_delta[b] += flux_txn / vol_b;
            }
        }
    }

    // ── Второй проход: применить NT/ATP/temp/txn + bath-релаксацию ──
    // (внеклеточные ионы применены в субстеппинге выше)
    for (uint32_t i = 0; i < num_segments; i++) {
        for (int ion = 0; ion < ION_COUNT; ion++) {
            // Bath-релаксация: внеклетка — бесконечный резервуар (не даёт дрейфовать ионам).
            segments[i].extra_ions[ion] += (BATH_ION[ion] - segments[i].extra_ions[ion]) * dt / BATH_TAU_MS;
            if (segments[i].extra_ions[ion] < 1e-9) segments[i].extra_ions[ion] = 1e-9;   // нижняя граница
            if (segments[i].extra_ions[ion] > 1000.0) segments[i].extra_ions[ion] = 1000.0;  // верхняя (внеклетка ≤1 М)
        }
        for (int nt = 0; nt < NT_COUNT; nt++) {
            segments[i].intra_nt[nt] += intra_nt_delta[i * NT_COUNT + nt];
            segments[i].neurotransmitter[nt] += nt_delta[i * NT_COUNT + nt];
        }
        segments[i].ATP_level += atp_delta[i];
        segments[i].temperature += temp_delta[i];
        segments[i].transcription_signal += txn_delta[i];
    }
}
