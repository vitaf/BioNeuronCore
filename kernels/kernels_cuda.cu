// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// kernels_cuda.cu — нативные CUDA-ядра (порт из kernels_part1.sycl + kernels_part2.sycl)
// Компилируется nvcc, включается из gpu_backend_cuda.cu (единая единица трансляции).
// Биология сохранена 1:1 с SYCL-версией.

#include "types.h"
#include "kernel_dispatch.h"
#include "run_tick_common.h"
#include "modifier_helpers.h"
#include "delay_buffer.h"

// Xorshift32 RNG для вероятностного выброса (device-совместимый, без состояния)
static inline __device__ float xorshift32_rnd(uint32_t seed) {
    uint32_t x = seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return (float)(x & 0x7FFFFFFFu) / (float)0x7FFFFFFFu;
}

// ── Ядро 1: вычисление токов модификаторов для каждого сегмента ──

__global__ void kernel_compute_modifier_currents(Segment* segs, uint32_t N,
                                                  uint8_t mode, float dt,
                                                  const GpuStimulusData* stim_data) {
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= N) return;
    Segment* s = &segs[i];
    uint32_t sid = s->id;
    uint64_t tick = stim_data ? stim_data->current_tick : 0;

    // ── Применение стимулов (CurrentStimulus, RampStimulus, Ion, Ligand) ──
    if (stim_data) {
        for (uint32_t st = 0; st < stim_data->current_stim_count; st++) {
            const auto* cs = &stim_data->current_stim[st];
            if (cs->segment_id != sid) continue;
            uint64_t dur_ticks = (uint64_t)(cs->duration_ms / dt);
            if (tick >= cs->start_tick && tick < cs->start_tick + dur_ticks)
                s->I_buffer += cs->amplitude_pA;
        }
        for (uint32_t r = 0; r < stim_data->ramp_count; r++) {
            const auto* rs = &stim_data->ramps[r];
            if (rs->segment_id != sid) continue;
            if (tick >= rs->start_tick && tick < rs->end_tick) {
                float frac = (rs->end_tick > rs->start_tick)
                    ? (float)(tick - rs->start_tick) / (float)(rs->end_tick - rs->start_tick)
                    : 1.0f;
                s->I_buffer += (rs->start_pA + (rs->end_pA - rs->start_pA) * frac);
            }
        }
        for (uint32_t j = 0; j < stim_data->ion_inj_count; j++) {
            const auto* io = &stim_data->ion_inj[j];
            if (io->segment_id != sid || io->ion_idx < 0 || io->ion_idx >= ION_COUNT) continue;
            double* ions = io->extracellular ? s->extra_ions : s->intra_ions;
            ions[io->ion_idx] += io->amount_mM;
        }
        for (uint32_t l = 0; l < stim_data->lig_inj_count; l++) {
            const auto* li = &stim_data->lig_inj[l];
            if (li->segment_id != sid || li->nt_idx < 0 || li->nt_idx >= NT_COUNT) continue;
            float* target = li->intracellular ? s->intra_nt : s->neurotransmitter;
            target[li->nt_idx] += li->amount;
        }
    }

    // ── Токи модификаторов ──────────────────────────────────────────
    bool myelinated = segment_has_myelin(s);
    double I_total = 0.0;
    uint8_t mc = s->modifier_count;
    if (mc > MAX_MODIFIERS_PER_SEGMENT) mc = 0;
    for (uint8_t j = 0; j < mc; j++) {
        uint8_t type = s->modifiers[j].type;
        if (myelinated && type != MOD_MYELIN && is_myelin_blocked(type))
            continue;
        set_modifier_effective_area(&s->modifiers[j], s);
        I_total += dispatch_modifier_current(mode, type,
                                              &s->modifiers[j], s, dt);
        for (int k = 0; k < 8; k++)
            if (s->modifiers[j].state[k] != s->modifiers[j].state[k])
                s->modifiers[j].state[k] = 0.0f;
    }
    if (I_total == I_total)
        s->I_buffer += I_total;
}

// ── Ядро 3: обновление мембранного потенциала + VoltageClamp ──

__global__ void kernel_update_membrane(Segment* segs, uint32_t N, float dt, uint64_t tick,
                                        const GpuStimulusData* stim_data) {
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= N) return;
    Segment* s = &segs[i];
    float area = (s->segment_type == 1 || s->segment_type == 3 || s->segment_type == 4)
        ? 4.0f * PI * s->radius * s->radius
        : 2.0f * PI * s->radius * s->length;

    uint8_t mc = s->modifier_count;
    if (mc > MAX_MODIFIERS_PER_SEGMENT) mc = 0;

    bool myelinated = false;
    for (uint8_t j = 0; j < mc; j++) {
        if (s->modifiers[j].type == MOD_MYELIN) {
            myelinated = true;
            break;
        }
    }

    float C_m = area * C_SPECIFIC_PF_PER_UM2;
    if (C_m < 1e-4f) C_m = 1e-4f;
    if (myelinated) C_m /= 100.0f;

    if (!(s->I_buffer == s->I_buffer)) s->I_buffer = 0.0f;
    if (s->I_buffer >  10000.0f) s->I_buffer =  10000.0f;
    if (s->I_buffer < -10000.0f) s->I_buffer = -10000.0f;

    s->V_m += s->I_buffer / C_m * dt;
    volatile double* vm = &s->V_m;
    if (!(*vm == *vm)) { *vm = -65.0f; }
    if (*vm < VM_CLAMP_MIN) { *vm = VM_CLAMP_MIN; }
    if (*vm > VM_CLAMP_MAX) { *vm = VM_CLAMP_MAX; }

    // ── VoltageClamp ПОСЛЕ обновления V_m ────────────────────────
    if (stim_data) {
        for (uint32_t v = 0; v < stim_data->vclamp_count; v++) {
            const auto* vc = &stim_data->vclamps[v];
            if (vc->segment_id == s->id && tick >= vc->start_tick && tick < vc->end_tick)
                s->V_m = vc->target_mV;
        }
    }

    s->I_buffer = 0.0f;

    if (!(s->position.x == s->position.x)) s->position.x = 0.0f;
    if (!(s->position.y == s->position.y)) s->position.y = 50.0f;
    if (!(s->position.z == s->position.z)) s->position.z = 0.0f;
    if (!(s->radius == s->radius)) s->radius = 5.0f;
    if (!(s->length == s->length)) s->length = 10.0f;
}

// ── Ядро 4: синаптические токи — выброс, модуляция, постсинаптический ответ, клиренс ──

__global__ void kernel_compute_synaptic_currents(Segment* segs, Synapse* syns,
                                                  uint32_t K, uint32_t num_segs, uint8_t mode, uint64_t tick,
                                                  float dt, DelayedRelease* delay_buffer,
                                                  uint32_t* glial_indices, uint32_t glial_count) {
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= K) return;
    Synapse* syn = &syns[i];
    if (syn->pre_segment >= num_segs || syn->post_segment >= num_segs) return;
    Segment* pre = &segs[syn->pre_segment];
    Segment* post = &segs[syn->post_segment];

    // Электрический синапс (gap junction): быстрый Vj-гейт + медленный Ca²⁺/CaM-гейт
    if (syn->type != SYNAPSE_TYPE_CHEMICAL) {
        float Vj = pre->V_m - post->V_m;

        // Медленный Ca²⁺/CaM-гейт (single-source: gap_junction_cam_update)
        float m_slow = gap_junction_cam_update(syn->depression,
                                               pre->intra_ions[ION_CA], post->intra_ions[ION_CA], dt);
        syn->depression = m_slow;

        // Эффективная проводимость (single-source: gap_junction_g_eff)
        float g_eff = gap_junction_g_eff(pre, post, m_slow, Vj);
        float I_gap_raw = g_eff * Vj;

        // Gap junction delay (RC-фильтр первого порядка, tau=delay_ms)
        float I_gap = I_gap_raw;
        if (syn->delay > 0.001f) {
            I_gap = syn->cleft_neurotransmitter[0];
            I_gap += (I_gap_raw - I_gap) * dt / syn->delay;
            syn->cleft_neurotransmitter[0] = I_gap;
        }

        if (I_gap == I_gap) {  // NaN guard
            atomicAdd(&pre->I_buffer, -I_gap);
            atomicAdd(&post->I_buffer, I_gap);
        }
        return;
    }

    // ── Обработка задержанных выбросов (до проверки pre-spike) ──
    if (syn->delay > 0.0f && delay_buffer) {
        DelayedRelease* slots = &delay_buffer[i * MAX_DELAY_SLOTS];
        process_delayed_releases(slots, MAX_DELAY_SLOTS, tick, syn->cleft_neurotransmitter);
    }

    // ── RRP recovery (везикулярная динамика, BION v7) ──────────
    rrp_do_recovery(syn, pre, dt);

    // ── Пресинаптический микродомен Ca²⁺ (бутон, single-source) ──
    bouton_ca_step(pre, syn, dt);

    // Ca²⁺-зависимый выброс (синаптотагмин, Hill n=4) — по bouton_Ca, НЕ V_m-порог
    if (syn->refractory == 0) {
        float Ca_uM = syn->bouton_Ca * MM_TO_UM;
        float ca_act = powf(Ca_uM, SYT_HILL) / (powf(Ca_uM, SYT_HILL) + powf(SYT_KD_UM, SYT_HILL));
        float p = syn->release_probability * ca_act;  // БЕЗ (1+fac)/(1+dep) — поля хранят BCM-пластичность (паритет CPU:422)

        // Применение пресинаптических модуляторов (авторецепторов)
        for (int j = 0; j < syn->pre_autoreceptor_count; j++) {
            Modifier* ar = &syn->pre_autoreceptors[j];
            ar->state[7] = syn->pre_area;
            float mod = dispatch_release_modulator(mode, ar->type, ar,
                                                    syn->cleft_neurotransmitter, dt);
            p *= mod;
        }

        // Вероятностный выброс: xorshift32 с seed = id ^ tick ^ V_m
        uint32_t seed = syn->id ^ ((uint32_t)tick) ^ ((uint32_t)(pre->V_m * 1000.0f));
        if (xorshift32_rnd(seed) < p) {
            // Определяем нейромедиатор по типу рецептора
            uint8_t nt_type = NT_GLUTAMATE;
            if (syn->post_receptor_count > 0) {
                uint8_t rt = syn->post_receptors[0].type;
                if (rt == MOD_NT_RECEPTOR_GABAA || rt == MOD_NT_RECEPTOR_GABAB)
                    nt_type = NT_GABA;
                else if (rt == MOD_NT_RECEPTOR_GLYCINE)
                    nt_type = NT_GLYCINE;
                else if (rt == MOD_NT_RECEPTOR_5HT3)
                    nt_type = NT_SEROTONIN;
                else if (rt == MOD_NT_RECEPTOR_NICOTINIC)
                    nt_type = NT_ACETYLCHOLINE;
            }
            float release_amt = rrp_do_release(syn, syn->release_amount, p);
            uint64_t delay_ticks = delay_ms_to_ticks(syn->delay, dt);
            if (delay_ticks > 0 && delay_buffer) {
                DelayedRelease* slots = &delay_buffer[i * MAX_DELAY_SLOTS];
                enqueue_delayed_release(slots, MAX_DELAY_SLOTS,
                                        tick + delay_ticks, release_amt, nt_type);
            } else {
                syn->cleft_neurotransmitter[nt_type] += release_amt;
            }
            syn->refractory = (uint32_t)(20.0f / dt + 0.5f);  // 20 мс рефрактерность (dt-инвариантно, паритет CPU:452)
        }
    }

    // Перенос нейромедиатора из щели на постсегмент (атомарное накопление)
    for (int n = 0; n < NT_COUNT; n++) {
        float val = syn->cleft_neurotransmitter[n];
        if (val == val && val >= 0.0f)  // NaN guard + negative clamp
            atomicAdd(&post->neurotransmitter[n], val);
    }

    // ── Глиальный захват NT (посегментная связь, паритет с CPU step 3) ──
    // Глутамат/глицин/GABA из щели → ближайшая глиальная клетка (по position.z),
    // где их утилизируют GLAST/GlyT1/GAT3 (шаг 1). Заменяет tau_clearance.
    if (glial_count > 0) {
        for (int k = 0; k < 3; k++) {
            int n = (k == 0) ? NT_GLUTAMATE : (k == 1) ? NT_GLYCINE : NT_GABA;
            float nt_val = syn->cleft_neurotransmitter[n];
            if (nt_val <= 0.0f) continue;
            float best_dz = 1e30f;
            uint32_t best_g = glial_indices[0];
            for (uint32_t gi = 0; gi < glial_count; gi++) {
                float dz = fabsf(segs[glial_indices[gi]].position.z - post->position.z);
                if (dz < best_dz) { best_dz = dz; best_g = glial_indices[gi]; }
            }
            // rate-limited захват (паритет CPU:477): capture = cleft × (1−e^(−dt/τ)), НЕ мгновенный cleft=0
            float tau = (syn->tau_clearance > 0.0f) ? syn->tau_clearance : 5.0f;
            float capture = nt_val * (1.0f - expf(-dt / tau));
            atomicAdd(&segs[best_g].neurotransmitter[n], capture);
            syn->cleft_neurotransmitter[n] = nt_val - capture;
        }
    }

    // Вычисление постсинаптических токов через рецепторы
    bool myelinated_post = segment_has_myelin(post);
    double I_syn = 0.0;
    for (int j = 0; j < syn->post_receptor_count; j++) {
        uint8_t rtype = syn->post_receptors[j].type;
        if (myelinated_post && is_ionotropic_type(rtype))
            continue;
        syn->post_receptors[j].state[7] = syn->post_area;
        I_syn += dispatch_modifier_current(mode, rtype,
                                            &syn->post_receptors[j], post, dt);
    }

    // Атомарное накопление тока в I_buffer постсегмента
    if (I_syn == I_syn)  // NaN guard
        atomicAdd(&post->I_buffer, I_syn);

    spine_ca_step(syn, I_syn, dt);  // Ca²⁺ шипика из постсинаптического тока (локально)

    // Клиренс нейромедиатора из щели (экспоненциальный спад)
    // Глутамат/глицин/GABA — НЕ через tau_clearance, ЕСЛИ есть глия (забраны выше).
    for (int n = 0; n < NT_COUNT; n++) {
        if ((n == NT_GLUTAMATE || n == NT_GLYCINE || n == NT_GABA) && glial_count > 0) continue;
        syn->cleft_neurotransmitter[n] *= expf(-dt / syn->tau_clearance);
        if (syn->cleft_neurotransmitter[n] < 0.0f) syn->cleft_neurotransmitter[n] = 0.0f;
    }

    // Рефрактерный период
    if (syn->refractory > 0) {
        syn->refractory--;
    }
}

// ── Ядро 5a: межсегментная диффузия — ВЫЧИСЛЕНИЕ дельт (шаг 2.5, проход 1) ──
// Двухпроходной подход (как CPU simulation_host.cpp):
//   Проход 1 (это ядро): читает ОРИГИНАЛЬНЫЕ концентрации, вычисляет delta-C,
//                        пишет atomicAdd в temp-буферы (не трогая реальные данные).
//   Проход 2 (kernel_diffusion_apply): применяет накопленные дельты к сегментам.
// Это устраняет race condition в оригинальном kernel_diffusion, где
// atomicAdd-чтение частично обновлённых extra_ions давало неверные дельты.
// Результат: CPU-GPU паритет для CPG alternation (K+ клиренс глии).

__global__ void kernel_diffusion_compute(
    Segment* segs, IntraConnection* conns, uint32_t M, uint32_t num_segs, float dt,
    float* extra_delta, float* intra_delta, float* intra_nt_delta, float* nt_delta,
    float* atp_delta, float* temp_delta, float* txn_delta
) {
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= M) return;

    IntraConnection* conn = &conns[i];
    uint32_t a = conn->segment_a;
    uint32_t b = conn->segment_b;
    if (a >= num_segs || b >= num_segs) return;

    float r_min = segs[a].radius < segs[b].radius ? segs[a].radius : segs[b].radius;
    float area_conn = PI * r_min * r_min;
    if (area_conn < 1e-6f) area_conn = 1e-6f;

    float dist = (segs[a].length + segs[b].length) * 0.5f;
    if (dist < 0.1f) dist = 0.1f;

    float vol_a = PI * segs[a].radius * segs[a].radius * segs[a].length;
    float vol_b = PI * segs[b].radius * segs[b].radius * segs[b].length;
    if (vol_a < 1e-6f) vol_a = 1e-6f;
    if (vol_b < 1e-6f) vol_b = 1e-6f;

    bool extra_only = (conn->is_intracellular == 0);

    float diff_coeff[ION_COUNT] = { DIFF_NA, DIFF_K, DIFF_CA, DIFF_CL, DIFF_MG };
    const float nt_mass[NT_COUNT] = {147,103,153,176,146,267,169,75,111,348,507,232,24,162};

    // (внутриклеточный Нернст-Планк вынесен в kernel_intra_np_compute/apply — сабстеппинг)

    // Внеклеточная ионная диффузия (всегда, включая EXTRA_ONLY для глии)
    for (int ion = 0; ion < ION_COUNT; ion++) {
        float dC = segs[a].extra_ions[ion] - segs[b].extra_ions[ion];
        if (dC > -1e-12f && dC < 1e-12f) continue;
        float flux = diff_coeff[ion] * area_conn * dC * dt / dist;
        atomicAdd(&extra_delta[a * ION_COUNT + ion], -flux / vol_a);
        atomicAdd(&extra_delta[b * ION_COUNT + ion],  flux / vol_b);
    }

    if (!extra_only) {
        for (int nt = 0; nt < NT_COUNT; nt++) {
            float dC = segs[a].intra_nt[nt] - segs[b].intra_nt[nt];
            if (dC > -1e-12f && dC < 1e-12f) continue;
            float mass = nt_mass[nt]; if (mass < 1.0f) mass = 1.0f; float D = DIFF_NA * powf(23.0f / mass, 1.0f / 3.0f);
            float flux = D * area_conn * dC * dt / dist;
            atomicAdd(&intra_nt_delta[a * NT_COUNT + nt], -flux / vol_a);
            atomicAdd(&intra_nt_delta[b * NT_COUNT + nt],  flux / vol_b);
        }
    }

    for (int nt = 0; nt < NT_COUNT; nt++) {
        float dC = segs[a].neurotransmitter[nt] - segs[b].neurotransmitter[nt];
        if (dC > -1e-12f && dC < 1e-12f) continue;
        float mass = nt_mass[nt]; if (mass < 1.0f) mass = 1.0f; float D = DIFF_NA * powf(23.0f / mass, 1.0f / 3.0f);
        float flux = D * area_conn * dC * dt / dist;
        if (flux == flux) {
            float f_a = flux / vol_a; float f_b = flux / vol_b;
            if (f_a == f_a) atomicAdd(&nt_delta[a * NT_COUNT + nt], -f_a);
            if (f_b == f_b) atomicAdd(&nt_delta[b * NT_COUNT + nt],  f_b);
        }
    }

    if (!extra_only) {
        float dATP = segs[a].ATP_level - segs[b].ATP_level;
        if (dATP > 1e-12f || dATP < -1e-12f) {
            float flux_atp = DIFF_ATP * area_conn * dATP * dt / dist;
            atomicAdd(&atp_delta[a], -flux_atp / vol_a);
            atomicAdd(&atp_delta[b],  flux_atp / vol_b);
        }

        float dT = segs[a].temperature - segs[b].temperature;
        if (dT > 0.001f || dT < -0.001f) {
            float flux_heat = DIFF_HEAT * area_conn * dT * dt / dist;
            atomicAdd(&temp_delta[a], -flux_heat / vol_a);
            atomicAdd(&temp_delta[b],  flux_heat / vol_b);
        }

        float dTxn = segs[a].transcription_signal - segs[b].transcription_signal;
        if (dTxn > 1e-9f || dTxn < -1e-9f) {
            float flux_txn = 0.01f * area_conn * dTxn * dt / dist;
            atomicAdd(&txn_delta[a], -flux_txn / vol_a);
            atomicAdd(&txn_delta[b],  flux_txn / vol_b);
        }
    }
}

// ── Ядро 5b: применение накопленных дельт диффузии (шаг 2.5, проход 2) ──
// Один поток на сегмент. Добавляет накопленные дельты к реальным данным
// и обнуляет temp-буферы для следующего тика.

__global__ void kernel_diffusion_apply(
    Segment* segs, uint32_t num_segs,
    float* extra_delta, float* intra_delta, float* intra_nt_delta, float* nt_delta,
    float* atp_delta, float* temp_delta, float* txn_delta
) {
    uint32_t s = blockIdx.x * blockDim.x + threadIdx.x;
    if (s >= num_segs) return;

    // Применяем накопленные дельты и обнуляем буферы (внутриклеточные ионы — в сабстеппинге)
    for (int ion = 0; ion < ION_COUNT; ion++) {
        int idx_i = s * ION_COUNT + ion;
        segs[s].extra_ions[ion] += extra_delta[idx_i];
        extra_delta[idx_i] = 0.0f;
    }
    for (int nt = 0; nt < NT_COUNT; nt++) {
        int idx_nt = s * NT_COUNT + nt;
        segs[s].intra_nt[nt] += intra_nt_delta[idx_nt];
        intra_nt_delta[idx_nt] = 0.0f;

        segs[s].neurotransmitter[nt] += nt_delta[idx_nt];
        nt_delta[idx_nt] = 0.0f;
    }
    segs[s].ATP_level            += atp_delta[s];  atp_delta[s] = 0.0f;
    segs[s].temperature          += temp_delta[s]; temp_delta[s] = 0.0f;
    segs[s].transcription_signal += txn_delta[s];  txn_delta[s] = 0.0f;
}

// ── Ядро 5c: внутриклеточный Нернст-Планк (сабстеппинг, проход 1 — вычисление) ──
// Один поток на соединение. Ионный флюкс (диффузия + электромиграция) за dt_sub,
// atomicAdd в intra_delta (ионы) и i_np_accum (заряд-флюкс, НЕ I_buffer).
__global__ void kernel_intra_np_compute(
    Segment* segs, IntraConnection* conns, uint32_t M, uint32_t num_segs, float dt_sub,
    float* intra_delta, float* i_np_accum
) {
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= M) return;

    IntraConnection* conn = &conns[i];
    uint32_t a = conn->segment_a;
    uint32_t b = conn->segment_b;
    if (a >= num_segs || b >= num_segs) return;
    if (conn->is_intracellular == 0) return;

    float r_min = segs[a].radius < segs[b].radius ? segs[a].radius : segs[b].radius;
    float area_conn = PI * r_min * r_min;
    if (area_conn < 1e-6f) area_conn = 1e-6f;
    float dist = (segs[a].length + segs[b].length) * 0.5f;
    if (dist < 0.1f) dist = 0.1f;
    float vol_a = PI * segs[a].radius * segs[a].radius * segs[a].length;
    float vol_b = PI * segs[b].radius * segs[b].radius * segs[b].length;
    if (vol_a < 1e-6f) vol_a = 1e-6f;
    if (vol_b < 1e-6f) vol_b = 1e-6f;

    const float diff_coeff[ION_COUNT] = { DIFF_NA, DIFF_K, DIFF_CA, DIFF_CL, DIFF_MG };
    const float np_z[ION_COUNT] = { NP_Z_NA, NP_Z_K, NP_Z_CA, NP_Z_CL, NP_Z_MG };
    float I_np = 0.0f;
    for (int ion = 0; ion < ION_COUNT; ion++) {
        float C_a = segs[a].intra_ions[ion];
        float C_b = segs[b].intra_ions[ion];
        float dC = C_a - C_b;
        float dV = segs[a].V_m - segs[b].V_m;
        float C_avg = (C_a + C_b) * 0.5f;
        float driving = dC + np_z[ion] * NP_F_RT_MV * C_avg * dV;
        float flux = diff_coeff[ion] * area_conn * dt_sub / dist * driving;
        atomicAdd(&intra_delta[a * ION_COUNT + ion], -flux / vol_a);
        atomicAdd(&intra_delta[b * ION_COUNT + ion],  flux / vol_b);
        I_np += np_z[ion] * NP_CHARGE_TO_PA * flux / dt_sub;
    }
    atomicAdd(&i_np_accum[a], -I_np);
    atomicAdd(&i_np_accum[b],  I_np);
}

// ── Ядро 5d: внутриклеточный Нернст-Планк (сабстеппинг, проход 2 — применение) ──
// Один поток на сегмент. Ионы (intra_delta) + заряд-флюкс (i_np_accum → V_m), затем обнуление.
__global__ void kernel_intra_np_apply(
    Segment* segs, uint32_t num_segs, float dt_sub,
    float* intra_delta, float* i_np_accum
) {
    uint32_t s = blockIdx.x * blockDim.x + threadIdx.x;
    if (s >= num_segs) return;

    for (int ion = 0; ion < ION_COUNT; ion++) {
        int idx = s * ION_COUNT + ion;
        segs[s].intra_ions[ion] += intra_delta[idx];
        intra_delta[idx] = 0.0f;
    }
    float C_m = segment_capacitance(&segs[s]);
    segs[s].V_m += i_np_accum[s] / C_m * dt_sub;
    i_np_accum[s] = 0.0f;
}

// ── Ядро 6: синаптическая пластичность (шаг 3.5) ──
// Порт из simulation_host.cpp:525-601. Один поток на Synapse (только CHEMICAL).
// CaMKII/Calcineurin STDP + eCB retrograde signaling.
// Все формулы и константы 1:1 с CPU-версией.

__global__ void kernel_plasticity(Segment* segs, Synapse* syns, uint32_t K, float dt) {
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= K) return;

    Synapse* syn = &syns[i];
    if (syn->type != SYNAPSE_TYPE_CHEMICAL) return;

    uint32_t post_idx = syn->post_segment;
    Segment* post = &segs[post_idx];

    // AMPAR trafficking: CaMKII/Calcineurin
    float Ca_uM = post->intra_ions[ION_CA] * 1000.0f;
    float Ca_sq = Ca_uM * Ca_uM;
    syn->facilitation += (Ca_sq - syn->facilitation) * dt / 300000.0f;
    if (syn->facilitation < 0.0f) syn->facilitation = 0.0f;
    syn->depression = syn->facilitation + 0.01f;

    float camkii_act = 0.0f, calcineurin_act = 0.0f;
    for (uint8_t j = 0; j < post->modifier_count; j++) {
        if (post->modifiers[j].type == MOD_CAMKII)
            camkii_act = post->modifiers[j].state[0] + post->modifiers[j].state[1] * 0.5f;
        if (post->modifiers[j].type == MOD_CALCINEURIN)
            calcineurin_act = post->modifiers[j].state[0];
    }

    bool is_inhibitory = false;
    for (int j = 0; j < syn->post_receptor_count; j++) {
        uint8_t rt = syn->post_receptors[j].type;
        if (rt == MOD_NT_RECEPTOR_GABAA || rt == MOD_NT_RECEPTOR_GABAB ||
            rt == MOD_NT_RECEPTOR_GLYCINE)
            { is_inhibitory = true; break; }
    }

    float dw = 0.0f;
    if (camkii_act > 0.0f)
        dw += 0.0003f * camkii_act * dt;

    float pp1_active = 0.5f;
    for (uint8_t j = 0; j < post->modifier_count; j++) {
        if (post->modifiers[j].type == MOD_PP1_INHIBITOR_DARPP32)
            pp1_active = post->modifiers[j].state[3];
    }
    if (calcineurin_act > 0.0f)
        dw -= 0.0001f * calcineurin_act * pp1_active * dt;

    if (is_inhibitory) dw = -dw;
    syn->weight += dw;
    if (!(syn->weight == syn->weight)) syn->weight = 0.5f;  // NaN guard
    if (syn->weight < 0.01f) syn->weight = 0.01f;
    if (syn->weight > 10.0f)  syn->weight = 10.0f;

    // NaN guard для facilitation/depression (могут стать NaN через Ca²⁺)
    if (!(syn->facilitation == syn->facilitation)) syn->facilitation = 0.0f;
    if (!(syn->depression == syn->depression))     syn->depression = 0.01f;

    // eCB retrograde signaling
    for (uint8_t j = 0; j < post->modifier_count; j++) {
        if (post->modifiers[j].type == MOD_ECB_SYNTHESIS) {
            double ecb = post->modifiers[j].state[0];
            if (ecb > 0.0f) {
                syn->cleft_neurotransmitter[NT_ENDOCANNABINOID] += (float)(ecb * 0.1f * dt);
                post->modifiers[j].state[0] *= 0.9f;
            }
            break;
        }
    }
}

// ── Ядро 7: clamp всех полей сегмента (защита от NaN/переполнения) ──
// Один поток на сегмент — параллельно, практически бесплатно

__global__ void kernel_clamp_segments(Segment* segs, uint32_t N) {
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= N) return;
    Segment* s = &segs[i];

    // V_m: clamp [VM_CLAMP_MIN, VM_CLAMP_MAX] mV (паритет с CPU clamp_V_m)
    if (!(s->V_m == s->V_m)) s->V_m = -65.0f;
    if (s->V_m < VM_CLAMP_MIN) s->V_m = VM_CLAMP_MIN;
    if (s->V_m > VM_CLAMP_MAX) s->V_m = VM_CLAMP_MAX;

    // I_buffer: NaN → 0
    if (!(s->I_buffer == s->I_buffer)) s->I_buffer = 0.0f;

    // Ионы: NaN/<1e-12 → 1e-6 (нижний предел). БЕЗ потолка: ионы в мМ (K=140 мМ) — потолок 10 мМ обрезал физиологию (паритет с CPU clamp_ions)
    for (int j = 0; j < 5; j++) {
        if (!(s->intra_ions[j] == s->intra_ions[j]) || s->intra_ions[j] < 1e-12f) s->intra_ions[j] = 1e-6f;
        if (!(s->extra_ions[j] == s->extra_ions[j]) || s->extra_ions[j] < 1e-12f) s->extra_ions[j] = 1e-6f;
    }

    // Нейромедиаторы: NaN/<0 → 0
    for (int j = 0; j < 14; j++) {
        if (!(s->neurotransmitter[j] == s->neurotransmitter[j]) || s->neurotransmitter[j] < 0.0f) s->neurotransmitter[j] = 0.0f;
        if (!(s->intra_nt[j] == s->intra_nt[j]) || s->intra_nt[j] < 0.0f) s->intra_nt[j] = 0.0f;
    }

    // ATP/temp/txn
    if (!(s->ATP_level == s->ATP_level) || s->ATP_level < 0.0f) s->ATP_level = 1.0f;
    if (!(s->temperature == s->temperature) || s->temperature < 10.0f || s->temperature > 50.0f) s->temperature = CELSIUS_37;
    if (!(s->transcription_signal == s->transcription_signal)) s->transcription_signal = 0.0f;

    // Modifier state[]
    for (uint8_t j = 0; j < s->modifier_count && j < MAX_MODIFIERS_PER_SEGMENT; j++) {
        for (int k = 0; k < 8; k++) {
            if (!(s->modifiers[j].state[k] == s->modifiers[j].state[k]))
                s->modifiers[j].state[k] = 0.0f;
        }
    }
}

void compute_clamp_segments(void* stream, Segment* segs, uint32_t N) {
    if (N == 0) return;
    int block = 256;
    int grid = (N + block - 1) / block;
    kernel_clamp_segments<<<grid, block, 0, (cudaStream_t)stream>>>(segs, N);
}

// ── Host-лаунчеры (вызываются из Simulation::run_tick) ──

void compute_modifier_currents(void* stream, Segment* segs, uint32_t N,
                                uint8_t mode, float dt,
                                const GpuStimulusData* stim_data) {
    if (N == 0 || N > 1000000 || segs == nullptr || stream == nullptr) return;
    int block = 256;
    int grid = (N + block - 1) / block;
    kernel_compute_modifier_currents<<<grid, block, 0, (cudaStream_t)stream>>>(segs, N, mode, dt, stim_data);
}

void update_membrane(void* stream, Segment* segs, uint32_t N, float dt, uint64_t tick,
                     const GpuStimulusData* stim_data) {
    if (N == 0) return;
    int block = 256;
    int grid = (N + block - 1) / block;
    kernel_update_membrane<<<grid, block, 0, (cudaStream_t)stream>>>(segs, N, dt, tick, stim_data);
}

void compute_synaptic_currents(void* stream, Segment* segs, Synapse* syns,
                                uint32_t K, uint32_t num_segs, uint8_t mode, uint64_t tick, float dt,
                                DelayedRelease* delay_buffer,
                                uint32_t* glial_indices, uint32_t glial_count) {
    if (K == 0) return;
    int block = 256;
    int grid = (K + block - 1) / block;
    kernel_compute_synaptic_currents<<<grid, block, 0, (cudaStream_t)stream>>>(segs, syns, K, num_segs, mode, tick, dt, delay_buffer, glial_indices, glial_count);
}

void compute_diffusion(void* stream, Segment* segs, IntraConnection* conns, uint32_t M, uint32_t num_segs, float dt,
                       float* extra_delta, float* intra_delta, float* intra_nt_delta, float* nt_delta,
                       float* atp_delta, float* temp_delta, float* txn_delta, float* i_np_accum) {
    if (M == 0) return;
    cudaStream_t s = (cudaStream_t)stream;

    int block_c = 256;
    int grid_c = (M + block_c - 1) / block_c;
    int block_a = 256;
    int grid_a = (num_segs + block_a - 1) / block_a;

    // ── Сабстеппинг внутриклеточного Нернст-Планка (жёсткая электромиграция) ──
    const float dt_sub = dt / (float)NP_SUBSTEPS;
    for (int sub = 0; sub < NP_SUBSTEPS; sub++) {
        kernel_intra_np_compute<<<grid_c, block_c, 0, s>>>(
            segs, conns, M, num_segs, dt_sub, intra_delta, i_np_accum);
        kernel_intra_np_apply<<<grid_a, block_a, 0, s>>>(
            segs, num_segs, dt_sub, intra_delta, i_np_accum);
    }

    // Проход 1 (внеклеточная диффузия + молекулярные): вычисление дельт
    kernel_diffusion_compute<<<grid_c, block_c, 0, s>>>(
        segs, conns, M, num_segs, dt,
        extra_delta, intra_delta, intra_nt_delta, nt_delta,
        atp_delta, temp_delta, txn_delta
    );

    // Проход 2: применение дельт к сегментам + обнуление буферов
    kernel_diffusion_apply<<<grid_a, block_a, 0, s>>>(
        segs, num_segs,
        extra_delta, intra_delta, intra_nt_delta, nt_delta,
        atp_delta, temp_delta, txn_delta
    );
}

void compute_plasticity(void* stream, Segment* segs, Synapse* syns, uint32_t K, float dt) {
    if (K == 0) return;
    int block = 256;
    int grid = (K + block - 1) / block;
    kernel_plasticity<<<grid, block, 0, (cudaStream_t)stream>>>(segs, syns, K, dt);
}
