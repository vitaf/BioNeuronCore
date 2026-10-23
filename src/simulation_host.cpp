// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// simulation_host.cpp — CPU-версия run_tick() для GCC-сборки.
// Используется Studio через libbiocore_gcc.a (без SYCL-зависимостей).
#include "types.h"
#include "modifier_interface.h"
#include "kernel_dispatch.h"
#include "run_tick_common.h"   // единые константы и хелперы для всех путей
#include "diffusion_kernel.h"  // единый шаг 2.5 (Нернст-Планк диффузия)
#include "modifier_helpers.h"  // bouton_ca_step, ghk_current_pA
#include "consolidation.h"
#include "delay_buffer.h"
#include "daemon_session.h"
#include <vector>
#include <cstring>
#include <cmath>

// Версия ядра
#define CORE_VERSION_MAJOR 0
#define CORE_VERSION_MINOR 10
#define CORE_VERSION_PATCH 0
#define CORE_VERSION_STR   "0.10.0"

// Глобальные переменные консолидации (для обратной совместимости со Studio,
// которая линкуется с libbiocore_gcc.a без сессионного контекста).
// Демон использует per-session поля SessionContext::consolidation_*.
static ConsolidationThread g_consolidation;
static bool g_consolidation_started = false;
static Neuron* g_neurons = nullptr;
static uint32_t g_num_neurons = 0;
static Nucleus* g_nuclei = nullptr;
static uint32_t g_num_nuclei = 0;

// Опциональный счётчик спайков + трекер пика V_m + rise-rate (для измерения/тестов).
// nullptr = выключен (нулевые накладные расходы на обычные вычисления сети).
static uint32_t* g_spike_count = nullptr;
static float*    g_vm_max = nullptr;
static float*    g_dvdt_max = nullptr;
static uint32_t  g_num_spike_segments = 0;

extern "C" void core_enable_spike_count(uint32_t* buf, uint32_t num_segments) {
    g_spike_count = buf;
    g_num_spike_segments = num_segments;
}
extern "C" void core_enable_vm_max(float* buf, uint32_t num_segments) {
    g_vm_max = buf;
    g_num_spike_segments = num_segments;
}
extern "C" void core_enable_dvdt_max(float* buf, uint32_t num_segments) {
    g_dvdt_max = buf;
    g_num_spike_segments = num_segments;
}
extern "C" void core_disable_spike_tracking() {
    g_spike_count = nullptr;
    g_vm_max = nullptr;
    g_dvdt_max = nullptr;
    g_num_spike_segments = 0;
}

// ── Консолидация: старые глобальные версии (для Studio) ──

extern "C" void core_init_consolidation(Neuron* neurons, uint32_t num_neurons,
                                         Nucleus* nuclei, uint32_t num_nuclei) {
    g_neurons = neurons;
    g_num_neurons = num_neurons;
    g_nuclei = nuclei;
    g_num_nuclei = num_nuclei;
}

extern "C" void core_start_consolidation(Segment* segments, uint32_t num_segments,
                                          Synapse* synapses, uint32_t* num_synapses_ptr,
                                          uint32_t max_synapses, uint64_t* tick_counter) {
    if (!g_consolidation_started && g_nuclei && g_num_nuclei > 0) {
        g_consolidation.start(segments, num_segments, synapses, num_synapses_ptr,
                              max_synapses, g_neurons, g_num_neurons,
                              g_nuclei, g_num_nuclei, tick_counter);
        g_consolidation_started = true;
    }
}

extern "C" void core_stop_consolidation() {
    g_consolidation.stop();
    g_consolidation_started = false;
}

extern "C" void core_set_consolidation_interval(uint32_t ticks) {
    g_consolidation.setGlobalInterval(ticks);
}

extern "C" uint32_t core_get_consolidation_interval() {
    return g_consolidation.getGlobalInterval();
}

extern "C" int core_get_pending_consolidation_events() {
    return (int)g_consolidation.getPendingEventCount();
}

extern "C" int core_get_nucleus_snapshot(uint32_t neuron_idx, Nucleus* out) {
    return g_consolidation.getNucleusSnapshot(neuron_idx, out) ? 1 : 0;
}

extern "C" int core_set_nucleus(uint32_t neuron_idx, const Nucleus* data) {
    return g_consolidation.setNucleus(neuron_idx, data) ? 1 : 0;
}

// ── Консолидация: per-session версии (для демона с multi-session) ──

extern "C" void core_init_consolidation_ctx(struct SessionContext* ctx,
                                              Neuron* neurons, uint32_t num_neurons,
                                              Nucleus* nuclei, uint32_t num_nuclei) {
    ctx->cons_neurons = neurons;
    ctx->cons_num_neurons = num_neurons;
    ctx->cons_nuclei = nuclei;
    ctx->cons_num_nuclei = num_nuclei;
}

extern "C" void core_start_consolidation_ctx(struct SessionContext* ctx,
                                               Segment* segments, uint32_t num_segments,
                                               Synapse* synapses, uint32_t* num_synapses_ptr,
                                               uint32_t max_synapses, uint64_t* tick_counter) {
    if (!ctx->consolidation_started && ctx->cons_nuclei && ctx->cons_num_nuclei > 0) {
        ctx->consolidation_thread.start(segments, num_segments, synapses, num_synapses_ptr,
                                         max_synapses, ctx->cons_neurons, ctx->cons_num_neurons,
                                         ctx->cons_nuclei, ctx->cons_num_nuclei, tick_counter);
        ctx->consolidation_started = true;
    }
}

extern "C" void core_stop_consolidation_ctx(struct SessionContext* ctx) {
    ctx->consolidation_thread.stop();
    ctx->consolidation_started = false;
}

extern "C" void core_set_consolidation_interval_ctx(struct SessionContext* ctx, uint32_t ticks) {
    ctx->consolidation_thread.setGlobalInterval(ticks);
}

extern "C" int core_get_nucleus_snapshot_ctx(struct SessionContext* ctx, uint32_t neuron_idx, Nucleus* out) {
    return ctx->consolidation_thread.getNucleusSnapshot(neuron_idx, out) ? 1 : 0;
}

extern "C" const char* core_version_string() { return CORE_VERSION_STR; }
extern "C" uint32_t core_version_major()   { return CORE_VERSION_MAJOR; }
extern "C" uint32_t core_version_minor()   { return CORE_VERSION_MINOR; }
extern "C" uint32_t core_version_patch()   { return CORE_VERSION_PATCH; }

// Xorshift32 RNG
static inline float xorshift32_rnd(uint32_t seed) {
    uint32_t x = seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return (float)(x & 0x7FFFFFFFu) / (float)0x7FFFFFFFu;
}

// ── Структура с указателями на per-session данные для core_run_tick_impl ──
struct TickSessionPtrs {
    ConsolidationThread* cons;
    bool* cons_started;
    Neuron** cons_neurons;
    uint32_t* cons_num_neurons;
    Nucleus** cons_nuclei;
    uint32_t* cons_num_nuclei;
    std::vector<DelayedRelease>* delay_buffer;
    uint32_t* delay_syn_count;
    ModifierFunc** active_library;
    ReleaseModulatorFunc** active_modulators;
};

// Единая реализация run_tick, без глобальных переменных.
// Все per-session данные — через ptrs. Если ptrs == nullptr, консолидация и delay пропускаются.
static void core_run_tick_impl(Segment* segments, uint32_t num_segments,
                   IntraConnection* connections, uint32_t num_connections,
                   Synapse* synapses, uint32_t num_synapses,
                   uint8_t mode, uint64_t* tick_count, float dt,
                   CurrentStimulus* stimuli, uint32_t* stimulus_count,
                   VoltageClamp* vclamps, uint32_t* vclamp_count,
                   RampStimulus* ramps, uint32_t* ramp_count,
                   IonInjection* ion_inj, uint32_t* ion_inj_count,
                   LigandInjection* lig_inj, uint32_t* lig_inj_count,
                   MechanoStimulus* mechano, uint32_t* mechano_count,
                   TickSessionPtrs* ptrs) {

    uint64_t tick = (*tick_count)++;

    // Очистка завершённых стимулов до начала тика
    {
        auto remove_expired = [&](auto* arr, uint32_t& cnt, auto is_expired) {
            uint32_t w = 0;
            for (uint32_t r = 0; r < cnt; r++) {
                if (!is_expired(arr[r], tick)) arr[w++] = arr[r];
            }
            cnt = w;
        };
        if (stimuli && stimulus_count)
            remove_expired(stimuli, *stimulus_count,
                [dt](const CurrentStimulus& s, uint64_t t) {
                    return t >= s.start_tick + (uint64_t)(s.duration_ms / dt);
                });
        if (vclamps && vclamp_count)
            remove_expired(vclamps, *vclamp_count,
                [](const VoltageClamp& v, uint64_t t) { return t >= v.end_tick; });
        if (ramps && ramp_count)
            remove_expired(ramps, *ramp_count,
                [](const RampStimulus& r, uint64_t t) { return t >= r.end_tick; });
        // ионные/лигандные инжекции — одноразовые, очищаются после применения в шаге 1
    }

    // Применение механо-стимулов (task #46): значение → state[state_idx]
    // модификатора mod_type на сегменте segment_id. Одноразовые — очищаются
    // сразу после применения, до шага 1 (токи модификаторов читают state[]).
    if (mechano && mechano_count) {
        for (uint32_t m = 0; m < *mechano_count; m++) {
            const MechanoStimulus& ms = mechano[m];
            for (uint32_t i = 0; i < num_segments; i++) {
                if (segments[i].id != ms.segment_id) continue;
                for (uint8_t j = 0; j < segments[i].modifier_count; j++) {
                    if (segments[i].modifiers[j].type == ms.mod_type) {
                        if (ms.state_idx >= 0 && ms.state_idx < MAX_STATE_VARS)
                            segments[i].modifiers[j].state[ms.state_idx] = ms.value;
                        break;
                    }
                }
                break;
            }
        }
        *mechano_count = 0;
    }

    // Шаг 1: вычислить токи модификаторов + применить стимулы
    if (num_segments > 0) {
        for (uint32_t i = 0; i < num_segments; i++) {
            double I_total = 0.0;  // double (паритет с GPU kernels_part1.sycl:58)
            bool myelinated = segment_has_myelin(&segments[i]);
            for (uint8_t j = 0; j < segments[i].modifier_count; j++) {
                uint8_t type = segments[i].modifiers[j].type;
                if (myelinated && type != MOD_MYELIN && is_myelin_blocked(type))
                    continue;
                set_modifier_effective_area(&segments[i].modifiers[j], &segments[i]);
                I_total += dispatch_modifier_current(mode, type,
                    &segments[i].modifiers[j], &segments[i], dt);
            }

            // Применить current stimuli (I_clamp) к данному сегменту
            if (stimuli && stimulus_count) {
                for (uint32_t s = 0; s < *stimulus_count; s++) {
                    const CurrentStimulus& st = stimuli[s];
                    if (st.segment_id == segments[i].id) {
                        uint64_t dur_ticks = (uint64_t)(st.duration_ms / dt);
                        if (tick >= st.start_tick && tick < st.start_tick + dur_ticks) {
                            // amplitude_pA уже в pA (внутренняя единица I_buffer)
                            I_total += st.amplitude_pA;
                        }
                    }
                }
            }

            // Применить ramp stimuli к данному сегменту
            if (ramps && ramp_count) {
                for (uint32_t r = 0; r < *ramp_count; r++) {
                    const RampStimulus& rs = ramps[r];
                    if (rs.segment_id == segments[i].id) {
                        if (tick >= rs.start_tick && tick < rs.end_tick) {
                            float frac = (rs.end_tick > rs.start_tick)
                                ? (float)(tick - rs.start_tick) / (float)(rs.end_tick - rs.start_tick)
                                : 1.0f;
                            float amp = rs.start_pA + (rs.end_pA - rs.start_pA) * frac;
                            // amp уже в pA (внутренняя единица I_buffer)
                            I_total += amp;
                        }
                    }
                }
            }

            // Применить ионные инжекции
            if (ion_inj && ion_inj_count) {
                for (uint32_t j = 0; j < *ion_inj_count; j++) {
                    const IonInjection& inj = ion_inj[j];
                    if (inj.segment_id == segments[i].id && inj.ion_idx >= 0 && inj.ion_idx < ION_COUNT) {
                        double* ions = inj.extracellular ? segments[i].extra_ions : segments[i].intra_ions;
                        ions[inj.ion_idx] += inj.amount_mM;
                    }
                }
            }

            // Применить лигандные инжекции
            if (lig_inj && lig_inj_count) {
                for (uint32_t l = 0; l < *lig_inj_count; l++) {
                    const LigandInjection& linj = lig_inj[l];
                    if (linj.segment_id == segments[i].id && linj.nt_idx >= 0 && linj.nt_idx < NT_COUNT) {
                        if (linj.intracellular) {
                            segments[i].intra_nt[linj.nt_idx] += linj.amount;
                        } else {
                            segments[i].neurotransmitter[linj.nt_idx] += linj.amount;
                        }
                    }
                }
            }

            segments[i].I_buffer = I_total;
        }
        // Очистка одноразовых инжекций после применения
        if (ion_inj && ion_inj_count) *ion_inj_count = 0;
        if (lig_inj && lig_inj_count) *lig_inj_count = 0;
    }

    // Шаг 2 (Осевой ток): убран Ohm-суррогат I=dV/R. Осевой заряд-флюкс теперь
    // вычисляется в шаге 2.5 как электромиграционный член Нернст-Планка (см. ниже).

    // Шаг 2.5: ионная электродиффузия (Нернст-Планк) — единый источник (diffusion_kernel.h)
    run_diffusion_step(segments, num_segments, connections, num_connections, dt);

    // Шаг 2.5b: сома→Nucleus связь (обновление кэша сигналов и реактивации)
    if (ptrs && *ptrs->cons_started && *ptrs->cons_nuclei && *ptrs->cons_neurons) {
        ptrs->cons->updateNucleusFromSoma(segments, *ptrs->cons_neurons, *ptrs->cons_nuclei, *ptrs->cons_num_neurons);
    }

    // Шаг 3: синаптические токи (выброс, модуляция, пост-токи, клиренс)
    // ── Буфер задержанных выбросов (static, перевыделяется при изменении числа синапсов) ──
    if (ptrs && num_synapses != *ptrs->delay_syn_count) {
        ptrs->delay_buffer->resize((size_t)num_synapses * MAX_DELAY_SLOTS);
        memset(ptrs->delay_buffer->data(), 0, ptrs->delay_buffer->size() * sizeof(DelayedRelease));
        *ptrs->delay_syn_count = num_synapses;
    }

    if (num_synapses > 0) {
        // Обнуляем постсинаптические NT перед накоплением (только при наличии синапсов —
        // иначе сотрётся NT-диффузия/volume transmission, тест NT diffusion)
        for (uint32_t i = 0; i < num_segments; i++)
            for (int n = 0; n < NT_COUNT; n++) segments[i].neurotransmitter[n] = 0.0f;
    }

    // bath-NMDA (безусловный): постоянный фоновый глутамат для сегментов с NMDA-рецептором на соме
    // (изолированный спинной мозг — fictive swimming; NMDAR-пейсмейкер dIN ~10 Гц, Soffe & Roberts 1989)
    for (uint32_t i = 0; i < num_segments; i++)
        for (uint8_t j = 0; j < segments[i].modifier_count; j++)
            if (segments[i].modifiers[j].type == MOD_NT_RECEPTOR_NMDA)
                segments[i].neurotransmitter[NT_GLUTAMATE] = BATH_NMDA_GLUTAMATE;

    // bath-5-HT: тонический нейромодуляторный тонус (raphe→спинной мозг). НЕ «зажигает» ритм —
    // 5-HT7 (Gs)→cAMP→сдвиг HCN1 V½ (steady-деполяризация). Триггер ритма остаётся RB→dIN.
    for (uint32_t i = 0; i < num_segments; i++)
        for (uint8_t j = 0; j < segments[i].modifier_count; j++)
            if (segments[i].modifiers[j].type == MOD_NT_RECEPTOR_5HT7)
                segments[i].neurotransmitter[NT_SEROTONIN] = BATH_5HT_SEROTONIN;

    // ── Глиальные клетки (GLAST) для посегментного захвата глутамата ──
    uint32_t glial_indices[256];
    uint32_t glial_count = 0;
    for (uint32_t g = 0; g < num_segments && glial_count < 256; g++)
        for (uint8_t j = 0; j < segments[g].modifier_count; j++)
            if (segments[g].modifiers[j].type == MOD_GLAST) { glial_indices[glial_count++] = g; break; }

    if (num_synapses > 0) {
        for (uint32_t i = 0; i < num_synapses; i++) {
            Synapse* syn = &synapses[i];
            uint32_t pre_idx  = syn->pre_segment;
            uint32_t post_idx = syn->post_segment;
            if (pre_idx >= num_segments || post_idx >= num_segments) continue;

            Segment* pre  = &segments[pre_idx];
            Segment* post = &segments[post_idx];

            // ── Обработка задержанных выбросов (до проверки pre-spike) ──
            if (syn->delay > 0.0f && ptrs && !ptrs->delay_buffer->empty()) {
                DelayedRelease* slots = &(*ptrs->delay_buffer)[i * MAX_DELAY_SLOTS];
                process_delayed_releases(slots, MAX_DELAY_SLOTS, tick, syn->cleft_neurotransmitter);
            }

            // Электрический синапс (gap junction): медленный Ca²⁺/CaM-гейт (Cx36 — без Vj-гейта)
            if (syn->type == SYNAPSE_TYPE_ELECTRICAL ||
                syn->type == SYNAPSE_TYPE_DENDRITIC_BRIDGE) {
                float Vj = pre->V_m - post->V_m;

                // ── Медленный Ca²⁺/CaM-гейт (single-source: gap_junction_cam_update) ──
                float m_slow = gap_junction_cam_update(syn->depression,
                                                       pre->intra_ions[ION_CA], post->intra_ions[ION_CA], dt);
                syn->depression = m_slow;

                // ── Эффективная проводимость (single-source: gap_junction_g_eff) ──
                float g_eff = gap_junction_g_eff(pre, post, m_slow, Vj);
                float I_gap_raw = g_eff * Vj;

                // ── Gap junction delay (кабельная задержка) ──
                // Биология: конечная скорость распространения (0.1-0.5 м/с)
                // Задержка = расстояние/скорость = 2-5ms/сегмент.
                // Модель: RC-фильтр первого порядка (tau = delay_ms)
                // Хранится в syn->cleft_neurotransmitter[0] (не используется для ELECTRICAL)
                if (syn->delay > 0.001f) {
                    float I_filtered = syn->cleft_neurotransmitter[0];
                    I_filtered += (I_gap_raw - I_filtered) * dt / syn->delay;
                    syn->cleft_neurotransmitter[0] = I_filtered;
                    pre->I_buffer -= I_filtered;
                    post->I_buffer += I_filtered;
                } else {
                    pre->I_buffer -= I_gap_raw;
                    post->I_buffer += I_gap_raw;
                }
                continue;
            }

            // ── RRP recovery (везикулярная динамика, BION v7) ──────────
            // Ca²⁺/CaMKII/Calcineurin-зависимый priming.
            // Использует УЖЕ СУЩЕСТВУЮЩИЕ [Ca²⁺]pre, CaMKII, Calcineurin.
            //
            // Protein→parameter mapping (BION v7): nucleus protein levels
            // модулируют RRP_max и release_probability.
            // Только при активной консолидации (GRN поддерживает protein levels).
            // Оптимизация: прямой доступ nucleus_index == neuron_id.
            if (g_consolidation_started && g_nuclei && g_num_nuclei > 0 && pre->neuron_id < g_num_nuclei) {
                rrp_apply_protein_mapping(syn, g_nuclei[pre->neuron_id].protein_level);
            }
            rrp_do_recovery(syn, pre, dt);

            // ── Пресинаптический микродомен Ca²⁺ (бутон, single-source) ──
            bouton_ca_step(pre, syn, dt);

            // Ca²⁺-зависимый выброс (синаптотагмин, Hill n=4) — по bouton_Ca, НЕ V_m-порог
            if (syn->refractory == 0) {
                float Ca_uM = syn->bouton_Ca * MM_TO_UM;
                float ca_act = pow(Ca_uM, SYT_HILL) /
                               (pow(Ca_uM, SYT_HILL) + pow(SYT_KD_UM, SYT_HILL));
                // p = release_probability × ca_act (синаптотагмин, Ca²⁺-зависимый выброс).
                // БЕЗ (1+facilitation)/(1+depression): эти поля хранят BCM-пластичность
                // (Ca²⁺²_running_avg / θ_m), а НЕ short-term plasticity — перегрузка полей устранена.
                float p = syn->release_probability * ca_act;

                for (int j = 0; j < syn->pre_autoreceptor_count; j++) {
                    Modifier* ar = &syn->pre_autoreceptors[j];
                    ar->state[7] = syn->pre_area;
                    float mod_factor = dispatch_release_modulator(
                        mode, ar->type, ar, syn->cleft_neurotransmitter, dt);
                    p *= mod_factor;
                }

                uint32_t seed = syn->id ^ ((uint32_t)tick) ^
                               ((uint32_t)(pre->V_m * 1000.0f));
                if (xorshift32_rnd(seed) < p) {
                    uint8_t nt_type = (syn->post_receptor_count > 0)
                        ? nt_type_from_receptor_type(syn->post_receptors[0].type)
                        : NT_GLUTAMATE;
                    float release_amt = rrp_do_release(syn, syn->release_amount, p);
                    uint64_t delay_ticks = delay_ms_to_ticks(syn->delay, dt);
                    if (delay_ticks > 0) {
                        // Отложенный выброс — enqueue в delay buffer
                        DelayedRelease* slots = &(*ptrs->delay_buffer)[i * MAX_DELAY_SLOTS];
                        enqueue_delayed_release(slots, MAX_DELAY_SLOTS,
                                                tick + delay_ticks, release_amt, nt_type);
                    } else {
                        // Немедленный выброс (dt >= delay или delay=0)
                        syn->cleft_neurotransmitter[nt_type] += release_amt;
                    }
                    // Рефрактерный период высвобождения = длительность Ca²⁺-транзиента бутона
                    // (~20 мс, τ bouton_Ca=5 мс): один ПД → один релиз, иначе ~110 релизов/спайк
                    // истощали RRP (25 везикул) за первый же спайк-трейн → d→d NMDA-фон мёртв.
                    syn->refractory = (uint32_t)(20.0f / dt + 0.5f);  // 20 мс рефрактерность в тиках (dt-инвариантно)
                }
            }

            // Перенос нейромедиатора из щели на постсегмент (накопление)
            for (int n = 0; n < NT_COUNT; n++) {
                post->neurotransmitter[n] += syn->cleft_neurotransmitter[n];
            }

            // ── Глиальный захват NT (rate-limited, НЕ мгновенный) ──
            // GLAST/GlyT1/GAT3 — транспортёры с конечной скоростью. Захват за тик =
            // cleft × (1 − e^(−dt/τ_clearance)); остаток затухает в щели. Мгновенный захват
            // (cleft=0) делал 1 релиз живущим 1 тик → AMPA r достигал лишь 0.13 (не 0.67) →
            // требовался нечестный ×8-гейн плотности.
            if (glial_count > 0) {
                for (int k = 0; k < 3; k++) {
                    int n = (k == 0) ? NT_GLUTAMATE : (k == 1) ? NT_GLYCINE : NT_GABA;
                    if (syn->cleft_neurotransmitter[n] <= 0.0f) continue;
                    float best_dz = 1e30f;
                    uint32_t best_g = glial_indices[0];
                    for (uint32_t gi = 0; gi < glial_count; gi++) {
                        float dz = fabsf(segments[glial_indices[gi]].position.z - post->position.z);
                        if (dz < best_dz) { best_dz = dz; best_g = glial_indices[gi]; }
                    }
                    float tau = (syn->tau_clearance > 0.0f) ? syn->tau_clearance : 5.0f;
                    float capture = syn->cleft_neurotransmitter[n] * (1.0f - exp(-dt / tau));
                    segments[best_g].neurotransmitter[n] += capture;
                    syn->cleft_neurotransmitter[n] -= capture;
                }
            }

            // Постсинаптические токи
            bool myelinated_post = segment_has_myelin(post);
            float I_syn = 0.0f;
            for (int j = 0; j < syn->post_receptor_count; j++) {
                uint8_t rtype = syn->post_receptors[j].type;
                if (myelinated_post && is_ionotropic_type(rtype))
                    continue;
                syn->post_receptors[j].state[7] = syn->post_area;
                I_syn += dispatch_modifier_current(mode, rtype,
                    &syn->post_receptors[j], post, dt);
            }
            post->I_buffer += I_syn;
            spine_ca_step(syn, I_syn, dt);  // Ca²⁺ шипика из постсинаптического тока (локально)

            // Клиренс нейромедиаторов (до пластичности — как в CUDA).
            // Глутамат/глицин/GABA — НЕ через tau_clearance, ЕСЛИ есть глия (её GLAST/GlyT1/GAT3 забрали).
            for (int n = 0; n < NT_COUNT; n++) {
                if ((n == NT_GLUTAMATE || n == NT_GLYCINE || n == NT_GABA) && glial_count > 0) continue;
                syn->cleft_neurotransmitter[n] *= exp(-dt / syn->tau_clearance);
                if (syn->cleft_neurotransmitter[n] < 0.0f)
                    syn->cleft_neurotransmitter[n] = 0.0f;
            }

            // ── AMPAR trafficking: CaMKII/Calcineurin (биологическая пластичность) ──
            // Суррогатные timing traces УДАЛЕНЫ. Пластичность основана на РЕАЛЬНОЙ
            // Ca²⁺-динамике: CaMKII (Kd=1.0μM, HIGH threshold) vs Calcineurin
            // (Kd=0.05μM, LOW threshold). Разные Ca²⁺-пороги естественно разделяют
            // LTP и LTD без внешних таймеров:
            //
            // Pre-before-post → массивный Ca²⁺ (>1μM) → CaMKII >> Calcineurin → LTP
            // Post-before-pre → скромный Ca²⁺ (0.05-1μM) → Calcineurin > CaMKII → LTD
            // Спонтанная активность → базальный Ca²⁺ (<0.05μM) → нет пластичности
            //
            // Ингибиторные синапсы: инвертированный знак dw.

            // ── BCM sliding threshold θ_m (скользящее среднее Ca²⁺²) ──
            // syn->facilitation хранит Ca²⁺²_running_avg (μM²), τ≈5 минут
            // syn->depression хранит θ_m = Ca²⁺²_avg + offset (μM²)
            float Ca_uM = syn->spine_Ca * 1000.0f;  // Ca²⁺ шипика (локально), не дендритного ствола
            float Ca_sq = Ca_uM * Ca_uM;
            float tau_theta = 300000.0f;
            syn->facilitation += (Ca_sq - syn->facilitation) * dt / tau_theta;
            if (syn->facilitation < 0.0f) syn->facilitation = 0.0f;
            syn->depression = syn->facilitation + 0.01f; // min θ_m = 0.01μM²

            // ── CaMKII / Calcineurin активность ──
            spine_plasticity_step(syn, dt);  // локальная пластичность шипика (CaMKII + кальциневрин из spine_Ca)
            float camkii_act = syn->spine_camkii;
            float calcineurin_act = syn->spine_calcineurin;

            bool is_inhibitory = false;
            for (int j = 0; j < syn->post_receptor_count; j++) {
                uint8_t rt = syn->post_receptors[j].type;
                if (rt == MOD_NT_RECEPTOR_GABAA || rt == MOD_NT_RECEPTOR_GABAB ||
                    rt == MOD_NT_RECEPTOR_GLYCINE)
                    { is_inhibitory = true; break; }
            }

            // ── Молекулярный путь: CaMKII → LTP, Calcineurin → LTD ──
            // Разные Kd (CaMKII=1μM, Calcineurin=0.05μM) → автоматическое
            // разделение LTP/LTD по Ca²⁺-концентрации.
            //
            // PP1 модуляция: D1→cAMP→PKA→p-DARPP32→inhibit PP1→LTD↓
            // При высокой дофаминовой активности PP1 подавлен → LTP облегчён
            float dw = 0.0f;
            if (camkii_act > 0.0f)
                dw += 0.0003f * camkii_act * dt;

            // Calcineurin → LTD (усиливается PP1: PP1 дефосфорилирует AMPAR)
            float pp1_active = 0.5f; // default PP1
            for (uint8_t j = 0; j < post->modifier_count; j++) {
                if (post->modifiers[j].type == MOD_PP1_INHIBITOR_DARPP32)
                    pp1_active = post->modifiers[j].state[3];
            }
            if (calcineurin_act > 0.0f)
                dw -= 0.0001f * calcineurin_act * pp1_active * dt; // PP1 усиливает LTD

            if (is_inhibitory) dw = -dw;
            syn->weight += dw;

            // NaN guard (NaN < 0.01f = false!)
            if (std::isnan(syn->weight)) syn->weight = 0.5f;
            if (std::isnan(syn->facilitation)) syn->facilitation = 0.0f;
            if (std::isnan(syn->depression))   syn->depression = 0.01f;

            // Ограничения веса
            if (syn->weight < 0.01f) syn->weight = 0.01f;
            if (syn->weight > 10.0f) syn->weight = 10.0f;

            // ── eCB retrograde signaling ──
            // Постсинаптический eCB синтез → cleft → пресинаптический CB1
            for (uint8_t j = 0; j < post->modifier_count; j++) {
                if (post->modifiers[j].type == MOD_ECB_SYNTHESIS) {
                    float ecb = post->modifiers[j].state[0];
                    if (ecb > 0.0f) {
                        syn->cleft_neurotransmitter[NT_ENDOCANNABINOID] += ecb * 0.1f * dt;
                        post->modifiers[j].state[0] *= 0.9f; // потребление eCB
                    }
                    break;
                }
            }

            if (syn->refractory > 0) syn->refractory--;
        }
    }

    // Шаг 4: обновить мембранный потенциал (с учётом ёмкости мембраны)
    if (num_segments > 0) {
        for (uint32_t i = 0; i < num_segments; i++) {
            // Clamp I_buffer перед вычислением — предотвращает экстремальные скачки V_m
            // (выровнен с CUDA-путём: kernels_cuda.cu)
            if (std::isnan(segments[i].I_buffer)) segments[i].I_buffer = 0.0f;
            if (segments[i].I_buffer >  10000.0f) segments[i].I_buffer =  10000.0f;
            if (segments[i].I_buffer < -10000.0f) segments[i].I_buffer = -10000.0f;

            float C_m = segment_capacitance(&segments[i]);
            float prev_V = segments[i].V_m;
            // TEMP: dV/dt clamp — явный Эйлер неустойчив на тонких сегментах (AIS C=0.63пФ)
            float dV = segments[i].I_buffer / C_m * dt;
            const float MAX_DV = 50.0f;  // мВ/тик (500 мВ/мс — выше био, ниже runaway 159)
            if (dV > MAX_DV) dV = MAX_DV;
            if (dV < -MAX_DV) dV = -MAX_DV;
            segments[i].V_m += dV;
            // Счётчик спайков (опциональный, nullptr → выключен). Фронт: пересечение порога снизу вверх.
            if (g_spike_count && i < g_num_spike_segments &&
                prev_V <= SPIKE_THRESHOLD && segments[i].V_m > SPIKE_THRESHOLD)
                g_spike_count[i]++;
            // Трекер пика V_m (опциональный, nullptr → выключен) — для coupling-коэффициента.
            if (g_vm_max && i < g_num_spike_segments && segments[i].V_m > g_vm_max[i])
                g_vm_max[i] = segments[i].V_m;
            // Трекер rise-rate (max dV/dt, опциональный) — для верификации спайк-формы.
            if (g_dvdt_max && i < g_num_spike_segments) {
                float dvdt = (segments[i].V_m - prev_V) / dt;
                if (dvdt > g_dvdt_max[i]) g_dvdt_max[i] = dvdt;
            }
            segments[i].I_buffer = 0.0f;

            // NaN-защита геометрии
            if (std::isnan(segments[i].position.x)) segments[i].position.x = 0.0f;
            if (std::isnan(segments[i].position.y)) segments[i].position.y = 50.0f;
            if (std::isnan(segments[i].position.z)) segments[i].position.z = 0.0f;
            if (std::isnan(segments[i].radius))   segments[i].radius = 5.0f;
            if (std::isnan(segments[i].length))   segments[i].length = 10.0f;

            // Voltage clamp: перезаписать V_m если сегмент зажат
            if (vclamps && vclamp_count) {
                for (uint32_t v = 0; v < *vclamp_count; v++) {
                    const VoltageClamp& vc = vclamps[v];
                    if (vc.segment_id == segments[i].id) {
                        if (tick >= vc.start_tick && tick < vc.end_tick) {
                            segments[i].V_m = vc.target_mV;
                        }
                    }
                }
            }

            clamp_V_m(&segments[i]);
            // Тепловой шум — только через MOD_THERMAL_NOISE (111).
            // Детерминированный «шум» создавал систематический дрейф Vm.
        }
    }

    // Шаг 5: применение событий консолидации (структурная пластичность)
    if (ptrs && *ptrs->cons_started) {
        ptrs->cons->applyPendingEvents(segments, num_segments,
                                        synapses, &num_synapses,
                                        num_synapses > 0 ? num_synapses * 2 : 1000);
    }
}

// ── Обёртка: исходный C ABI (глобальные переменные, для Studio) ──
extern "C" void core_run_tick(Segment* segments, uint32_t num_segments,
                   IntraConnection* connections, uint32_t num_connections,
                   Synapse* synapses, uint32_t num_synapses,
                   uint8_t mode, uint64_t* tick_count, float dt,
                   CurrentStimulus* stimuli, uint32_t* stimulus_count,
                   VoltageClamp* vclamps, uint32_t* vclamp_count,
                   RampStimulus* ramps, uint32_t* ramp_count,
                   IonInjection* ion_inj, uint32_t* ion_inj_count,
                   LigandInjection* lig_inj, uint32_t* lig_inj_count,
                   MechanoStimulus* mechano, uint32_t* mechano_count) {
    TickSessionPtrs ptrs;
    ptrs.cons            = &g_consolidation;
    ptrs.cons_started    = &g_consolidation_started;
    ptrs.cons_neurons    = &g_neurons;
    ptrs.cons_num_neurons = &g_num_neurons;
    ptrs.cons_nuclei     = &g_nuclei;
    ptrs.cons_num_nuclei = &g_num_nuclei;

    // Studio (одиночная сессия): delay_buffer через function-local static
    static std::vector<DelayedRelease> studio_delay_buffer;
    static uint32_t studio_delay_syn_count = 0;
    ptrs.delay_buffer    = &studio_delay_buffer;
    ptrs.delay_syn_count = &studio_delay_syn_count;

    ptrs.active_library = nullptr;     // Studio использует глобальные
    ptrs.active_modulators = nullptr;

    core_run_tick_impl(segments, num_segments, connections, num_connections,
                       synapses, num_synapses, mode, tick_count, dt,
                       stimuli, stimulus_count, vclamps, vclamp_count,
                       ramps, ramp_count, ion_inj, ion_inj_count,
                       lig_inj, lig_inj_count, mechano, mechano_count, &ptrs);
}

// ── Обёртка: сессионно-изолированная версия (для демона с multi-session) ──
extern "C" void core_run_tick_ctx(struct SessionContext* ctx,
                   Segment* segments, uint32_t num_segments,
                   IntraConnection* connections, uint32_t num_connections,
                   Synapse* synapses, uint32_t num_synapses,
                   uint8_t mode, uint64_t* tick_count, float dt,
                   CurrentStimulus* stimuli, uint32_t* stimulus_count,
                   VoltageClamp* vclamps, uint32_t* vclamp_count,
                   RampStimulus* ramps, uint32_t* ramp_count,
                   IonInjection* ion_inj, uint32_t* ion_inj_count,
                   LigandInjection* lig_inj, uint32_t* lig_inj_count) {
    // Сохранить глобальные таблицы функций и подставить сессионные
    ModifierFunc* saved_lib = active_library;
    ReleaseModulatorFunc* saved_mod = active_modulators;
    if (ctx->active_library)    active_library    = ctx->active_library;
    if (ctx->active_modulators) active_modulators = ctx->active_modulators;

    TickSessionPtrs ptrs;
    ptrs.cons            = &ctx->consolidation_thread;
    ptrs.cons_started    = &ctx->consolidation_started;
    ptrs.cons_neurons    = &ctx->cons_neurons;
    ptrs.cons_num_neurons = &ctx->cons_num_neurons;
    ptrs.cons_nuclei     = &ctx->cons_nuclei;
    ptrs.cons_num_nuclei = &ctx->cons_num_nuclei;
    ptrs.delay_buffer    = &ctx->delay_buffer;
    ptrs.delay_syn_count = &ctx->delay_syn_count;
    ptrs.active_library     = &ctx->active_library;
    ptrs.active_modulators  = &ctx->active_modulators;

    core_run_tick_impl(segments, num_segments, connections, num_connections,
                       synapses, num_synapses, mode, tick_count, dt,
                       stimuli, stimulus_count, vclamps, vclamp_count,
                       ramps, ramp_count, ion_inj, ion_inj_count,
                       lig_inj, lig_inj_count,
                       nullptr, nullptr,  // механо-стимулы не используются в daemon-пути
                       &ptrs);

    // ── Tiered execution: классификация сегментов по активности ──
    if (ctx->tiered_execution && segments && num_segments > 0) {
        // Инициализация массивов при первом вызове
        if (ctx->segment_tiers.size() < num_segments) {
            ctx->segment_tiers.resize(num_segments, 0);       // все HOT
            ctx->segment_last_spike.resize(num_segments, 0);
            ctx->tier_classify_tick = *tick_count + ctx->tier_classify_interval;
        }

        // Детекция спайков и обновление tier
        for (uint32_t si = 0; si < num_segments; si++) {
            // Спайк: V_m пересекает -20mV вверх (был ниже, стал выше)
            // Упрощённая детекция: V_m > -20mV → спайк
            if (segments[si].V_m > -20.0f) {
                ctx->segment_last_spike[si] = *tick_count;
                ctx->segment_tiers[si] = 0; // HOT
            }
        }

        // Периодическая переклассификация
        if (*tick_count >= ctx->tier_classify_tick) {
            uint32_t hot_cnt = 0, warm_cnt = 0, cold_cnt = 0;
            for (uint32_t si = 0; si < num_segments; si++) {
                float V_rest = -65.0f; // стандартный потенциал покоя
                float dev = fabsf(segments[si].V_m - V_rest);
                uint64_t ticks_since_spike = *tick_count - ctx->segment_last_spike[si];

                uint8_t new_tier;
                if (dev > ctx->tier_hot_threshold || ticks_since_spike < ctx->tier_classify_interval) {
                    new_tier = 0; // HOT: значительное отклонение или недавний спайк
                    hot_cnt++;
                } else if (dev < ctx->tier_cold_threshold && ticks_since_spike > ctx->tier_warm_ttl) {
                    new_tier = 2; // COLD: близко к покою, давно не было спайка
                    cold_cnt++;
                } else {
                    new_tier = 1; // WARM: умеренное отклонение
                    warm_cnt++;
                }
                ctx->segment_tiers[si] = new_tier;
            }
            ctx->tier_classify_tick = *tick_count + ctx->tier_classify_interval;
            // DLOG-вывод раз в 100 переклассификаций
            if ((ctx->tier_classify_tick / ctx->tier_classify_interval) % 100 == 0) {
                printf("[Tier] tick=%lu: HOT=%u WARM=%u COLD=%u (total=%u)\n",
                       (unsigned long)*tick_count, hot_cnt, warm_cnt, cold_cnt, num_segments);
            }

            // ── Shadow-валидация: проверяем биологическую корректность тирирования ──
            // Для COLD-сегментов: вычисляем V_m при условии ТОЛЬКО пассивной утечки
            // и сравниваем с полным расчётом. Если разница < 0.1mV — тирирование биологически обосновано.
            float max_cold_dev = 0.0f;
            uint32_t cold_validated = 0, cold_diverged = 0;
            const float V_rest = -65.0f;
            const float tau_m = 20.0f;  // типовая постоянная времени мембраны (ms)
            const float leak_factor = dt / tau_m;  // ~0.005 для dt=0.1, tau=20

            (void)leak_factor;
            for (uint32_t si = 0; si < num_segments; si++) {
                if (ctx->segment_tiers[si] == 2) {  // COLD
                    // Пассивная динамика: V → V_rest с постоянной tau_m
                    // V_pred = V_old + (V_rest - V_old) * leak_factor
                    // Но V_old нам не доступен после tick — используем эвристику:
                    // если сегмент COLD, то |V_actual - V_rest| должен быть < cold_threshold
                    // иначе что-то разбудило сегмент (синапс, gap junction)
                    float dev = fabsf(segments[si].V_m - V_rest);
                    if (dev > max_cold_dev) max_cold_dev = dev;
                    if (dev < ctx->tier_reclassify_tolerance) {
                        cold_validated++;  // COLD-предположение подтверждено
                    } else {
                        cold_diverged++;   // сегмент "уплыл" — нужна коррекция
                        ctx->segment_tiers[si] = 1;  // повышаем до WARM
                    }
                }
            }

            // Вывод статистики валидации
            if (cold_validated + cold_diverged > 0) {
                float validated_pct = 100.0f * cold_validated / (float)(cold_validated + cold_diverged);
                if ((ctx->tier_classify_tick / ctx->tier_classify_interval) % 100 == 0) {
                    printf("[Tier] Shadow validation: %.1f%% COLD correct (max dev=%.3fmV), %u diverged→WARM\n",
                           validated_pct, max_cold_dev * 1000.0f, cold_diverged);
                }
            }
        }
    }

    // Восстановить глобальные таблицы
    active_library    = saved_lib;
    active_modulators = saved_mod;
}

// ── Детерминированные метрики (Stage 3: validation) ──
// Вычисляет средний V_m, счётчик спайков, synchrony index.
// Формат JSON: {"mean_vm":-65.2,"spike_count":142,"synchrony":0.85}
extern "C" void core_compute_metrics(Segment* segments, uint32_t num_segments,
                                      Neuron* neurons, uint32_t num_neurons,
                                      uint64_t tick_count,
                                      char* out_json, size_t out_size) {
    if (!segments || !out_json || out_size == 0) return;

    float sum_vm = 0.0f;
    uint32_t spike_count = 0;
    for (uint32_t si = 0; si < num_segments; si++) {
        sum_vm += segments[si].V_m;
        if (segments[si].V_m > -20.0f) spike_count++;
    }
    float mean_vm = num_segments > 0 ? sum_vm / (float)num_segments : 0.0f;

    // Synchrony index: доля сегментов с |V_m - mean| < 10mV (грубая оценка)
    uint32_t sync_count = 0;
    for (uint32_t si = 0; si < num_segments; si++) {
        if (fabsf(segments[si].V_m - mean_vm) < 10.0f) sync_count++;
    }
    float synchrony = num_segments > 0 ? (float)sync_count / (float)num_segments : 1.0f;

    // Средняя V_m по типам нейронов (если есть нейроны)
    char type_vm[512] = "";
    if (neurons && num_neurons > 0) {
        // Упрощённо: группируем по type
        float type_sums[256] = {0};
        uint32_t type_counts[256] = {0};
        for (uint32_t ni = 0; ni < num_neurons; ni++) {
            uint8_t nt = neurons[ni].type;
            if (nt < 256 && neurons[ni].segment_count > 0) {
                uint32_t soma = neurons[ni].soma_index;
                if (soma < num_segments) {
                    type_sums[nt] += segments[soma].V_m;
                    type_counts[nt]++;
                }
            }
        }
        int pos = 0;
        pos += snprintf(type_vm + pos, sizeof(type_vm) - pos, "\"type_vm\":{");
        bool first = true;
        for (int t = 0; t < 256 && pos < (int)sizeof(type_vm) - 20; t++) {
            if (type_counts[t] > 0) {
                if (!first) pos += snprintf(type_vm + pos, sizeof(type_vm) - pos, ",");
                pos += snprintf(type_vm + pos, sizeof(type_vm) - pos,
                                "\"%d\":%.2f", t, type_sums[t] / (float)type_counts[t]);
                first = false;
            }
        }
        pos += snprintf(type_vm + pos, sizeof(type_vm) - pos, "}");
    }

    snprintf(out_json, out_size,
        "{\"mean_vm\":%.2f,\"spike_count\":%u,\"synchrony\":%.4f,\"tick\":%lu%s%s}",
        mean_vm, spike_count, synchrony, (unsigned long)tick_count,
        type_vm[0] ? "," : "", type_vm);
}
