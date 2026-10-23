// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#include "simulation.h"
#include "modifier_interface.h"
#include "kernel_dispatch.h"
#include "run_tick_common.h"
#include "diffusion_kernel.h"  // единый шаг 2.5 (Нернст-Планк диффузия)
#include "modifier_helpers.h"  // spine_plasticity_step, spine_ca_step
#include <cmath>

// Декларации из kernels_part1.sycl
void compute_modifier_currents(sycl::queue& q, Segment* segs, uint32_t N,
                                uint8_t mode, float dt,
                                const GpuStimulusData* stim_data);
void update_membrane(sycl::queue& q, Segment* segs, uint32_t N, float dt, uint64_t tick,
                     const GpuStimulusData* stim_data);
void compute_clamp_segments(sycl::queue& q, Segment* segs, uint32_t N);

// Декларации из kernels_part2.sycl
void compute_synaptic_currents(sycl::queue& q, Segment* segs, Synapse* syns,
                                uint32_t K, uint32_t num_segs, uint8_t mode, uint64_t tick, float dt,
                                DelayedRelease* delay_buffer,
                                uint32_t* glial_indices, uint32_t glial_count);

Simulation::Simulation(sycl::device dev) : q(dev) {
    segments = nullptr;
    connections = nullptr;
    synapses = nullptr;
    neurons = nullptr;
    delay_buffer = nullptr;
    stim_data = nullptr;
    glial_indices = nullptr;
    glial_count = 0;
    glial_capacity = 0;
    num_segments = 0;
    num_connections = 0;
    num_synapses = 0;
    num_neurons = 0;
    tick_count = 0;
    bion_loaded = false;
    memset(&header, 0, sizeof(header));
    memset(&bion_data, 0, sizeof(bion_data));
}

Simulation::~Simulation() {
    if (delay_buffer) { sycl::free(delay_buffer, q); delay_buffer = nullptr; }
    if (stim_data)    { sycl::free(stim_data, q);    stim_data = nullptr; }
    if (glial_indices) { sycl::free(glial_indices, q); glial_indices = nullptr; }
    if (bion_loaded) {
        bion_unload_usm(&bion_data, q);
    } else {
        if (segments)    sycl::free(segments, q);
        if (connections) sycl::free(connections, q);
        if (synapses)    sycl::free(synapses, q);
        if (neurons)     sycl::free(neurons, q);
    }
}

void Simulation::upload_stimuli(const GpuStimulusData* src) {
    if (!stim_data) {
        stim_data = (GpuStimulusData*)sycl::malloc_shared(sizeof(GpuStimulusData), q);
        if (!stim_data) return;
    }
    memcpy(stim_data, src, sizeof(GpuStimulusData));
}

int Simulation::load_network(const char* filename) {
    return load_bion(filename);
}

int Simulation::load_bion(const char* filename) {
    // ── Читаем заголовок чтобы узнать размеры сети ─────────────────────
    FILE* f = fopen(filename, "rb");
    if (!f) return -1;
    NetworkHeader hdr;
    size_t base_hdr_size = sizeof(NetworkHeader) - 16;
    fread(&hdr, base_hdr_size, 1, f);
    if (hdr.version >= 5) fread(((uint8_t*)&hdr) + base_hdr_size, 16, 1, f);
    fclose(f);

    if (hdr.magic != BION_MAGIC) return -2;
    if (hdr.num_segments == 0) return -4;

    // ── Переиспользуем GPU-буферы если ёмкости хватает ────────────────
    bool can_reuse = (segments && connections && synapses &&
                      seg_capacity >= hdr.num_segments &&
                      conn_capacity >= hdr.num_intra_connections &&
                      syn_capacity >= hdr.num_synapses);

    if (can_reuse) {
        // НЕ освобождаем — затираем и переиспользуем
        memset(segments,    0, seg_capacity * sizeof(Segment));
        memset(connections, 0, conn_capacity * sizeof(IntraConnection));
        memset(synapses,    0, syn_capacity * sizeof(Synapse));
        if (neurons) { delete[] neurons; neurons = nullptr; }
        if (bion_data.nuclei) { delete[] bion_data.nuclei; bion_data.nuclei = nullptr; }
        bion_loaded = false;
        memset(&bion_data, 0, sizeof(bion_data));

        // Загружаем данные в ПРЕДВЫДЕЛЕННЫЕ буферы (без аллокаций)
        bion_data.segments    = segments;
        bion_data.connections = connections;
        bion_data.synapses    = synapses;
        int ret = bion_load_into_buffers(filename, &bion_data);
        if (ret != 0) return ret;
    } else {
        // Буферов нет или сеть выросла — полная переаллокация
        if (bion_loaded) {
            bion_unload_usm(&bion_data, q);
        } else {
            if (segments)    { sycl::free(segments, q);    segments = nullptr; }
            if (connections) { sycl::free(connections, q); connections = nullptr; }
            if (synapses)    { sycl::free(synapses, q);    synapses = nullptr; }
            if (neurons)     { sycl::free(neurons, q);     neurons = nullptr; }
        }
        bion_loaded = false;
        memset(&bion_data, 0, sizeof(bion_data));

        int ret = bion_load_usm(filename, &bion_data, q);
        if (ret != 0) return ret;

        // Обновляем ёмкости
        seg_capacity  = bion_data.num_segments;
        conn_capacity = bion_data.num_connections;
        syn_capacity  = bion_data.num_synapses;
    }

    // Копируем указатели и счётчики
    header         = bion_data.header;
    neurons        = bion_data.neurons;
    segments       = bion_data.segments;
    connections    = bion_data.connections;
    synapses       = bion_data.synapses;
    num_neurons    = bion_data.num_neurons;
    num_segments   = bion_data.num_segments;
    num_connections = bion_data.num_connections;
    num_synapses   = bion_data.num_synapses;
    bion_loaded    = true;

    // ── Delay buffer: переиспользуем если хватает ─────────────────────
    size_t db_size = num_synapses > 0 ? (size_t)num_synapses * MAX_DELAY_SLOTS : 0;
    if (db_size > delay_capacity) {
        if (delay_buffer) { sycl::free(delay_buffer, q); delay_buffer = nullptr; }
        if (db_size > 0) {
            delay_buffer = sycl::malloc_shared<DelayedRelease>(db_size, q);
            delay_capacity = db_size;
        }
    }
    if (delay_buffer && db_size > 0)
        memset(delay_buffer, 0, db_size * sizeof(DelayedRelease));

    // ── Глиальные индексы (сегменты с MOD_GLAST) для шага 3 ───────────
    if (num_segments > glial_capacity) {
        if (glial_indices) { sycl::free(glial_indices, q); glial_indices = nullptr; }
        glial_indices = sycl::malloc_shared<uint32_t>(num_segments, q);
        glial_capacity = num_segments;
    }
    glial_count = 0;

    active_library = biological_library;
    active_modulators = biological_modulators;

    return validate_bion_data();
}

int Simulation::validate_bion_data() {
    // Дополнительная проверка согласованности уже загруженных данных
    if (num_segments == 0) return -1;
    return 0;
}

int Simulation::save_checkpoint(const char* filename) {
    header.num_segments = num_segments;
    header.num_intra_connections = num_connections;
    header.num_synapses = num_synapses;
    header.num_neurons = num_neurons;

    BionData save_data;
    memset(&save_data, 0, sizeof(save_data));
    save_data.header       = header;
    save_data.neurons      = neurons;
    save_data.segments     = segments;
    save_data.connections  = connections;
    save_data.synapses     = synapses;
    save_data.num_neurons    = num_neurons;
    save_data.num_segments   = num_segments;
    save_data.num_connections = num_connections;
    save_data.num_synapses   = num_synapses;

    return bion_save(filename, &save_data);
}

int Simulation::export_bion(const char* filename) {
    return save_checkpoint(filename);
}

void Simulation::run_tick(float dt) {
    uint8_t mode = header.mode;
    uint64_t tick = tick_count++;

    // Шаг 0: pre-kernel санитизация — предотвращает GPU page fault от NaN/overflow
    for (uint32_t i = 0; i < num_segments; i++) {
        Segment* s = &segments[i];
        if (!(s->V_m == s->V_m) || s->V_m < VM_CLAMP_MIN || s->V_m > VM_CLAMP_MAX) {
            s->V_m = (s->V_m < VM_CLAMP_MIN) ? VM_CLAMP_MIN : ((s->V_m > VM_CLAMP_MAX) ? VM_CLAMP_MAX : -65.0f);
        }
        s->I_buffer = 0.0f;  // сброс перед каждым тиком (паритет с CPU: строка 658)
        if (s->modifier_count > MAX_MODIFIERS_PER_SEGMENT || s->modifier_count == 0) {
            // Не трогаем modifier_count==0 (глия), только >MAX
            if (s->modifier_count > MAX_MODIFIERS_PER_SEGMENT) s->modifier_count = 0;
        }
        for (int ion = 0; ion < ION_COUNT; ion++) {
            if (!(s->intra_ions[ion] == s->intra_ions[ion])) s->intra_ions[ion] = 1e-6f;
            if (!(s->extra_ions[ion] == s->extra_ions[ion])) s->extra_ions[ion] = 1e-6f;
        }
        if (!(s->temperature == s->temperature) || s->temperature < 10.0f || s->temperature > 50.0f)
            s->temperature = CELSIUS_37;
        if (!(s->ATP_level == s->ATP_level) || s->ATP_level < 0.0f) s->ATP_level = 1.0f;
        // Геометрия: NaN/нулевой radius/length → C_m=0 → крах GPU при I_buffer/C_m
        if (!(s->radius == s->radius) || s->radius < 0.1f) s->radius = 1.0f;
        if (!(s->length == s->length) || s->length < 0.1f) s->length = 10.0f;
        if (!(s->position.x == s->position.x)) s->position.x = 0.0f;
        if (!(s->position.y == s->position.y)) s->position.y = 50.0f;
        if (!(s->position.z == s->position.z)) s->position.z = 0.0f;
        if (!(s->direction.x == s->direction.x)) s->direction.x = 0.0f;
        if (!(s->direction.y == s->direction.y)) s->direction.y = 1.0f;
        if (!(s->direction.z == s->direction.z)) s->direction.z = 0.0f;
    }

    // Шаг 1: вычислить токи модификаторов — всегда GPU
    if (num_segments > 0) {
        if (stim_data) stim_data->current_tick = tick_count;
        compute_modifier_currents(q, segments, num_segments, mode, dt, stim_data);
    }
    q.wait();

    // Шаг 2 (Осевой ток): убран Ohm-суррогат I=dV/R. Заряд-флюкс теперь в шаге 2.5
    // (электромиграция Нернст-Планка), применяется в host-петле диффузии.

    // Шаг 2.5: ионная электродиффузия (Нернст-Планк) — единый источник (diffusion_kernel.h)
    run_diffusion_step(segments, num_segments, connections, num_connections, dt);

    // NaN-защита ионов после диффузии (только при реальном NaN)
    for (uint32_t i = 0; i < num_segments; i++) {
        for (int ion = 0; ion < ION_COUNT; ion++) {
            if (std::isnan(segments[i].intra_ions[ion]))
                segments[i].intra_ions[ion] = 1e-6f;
            if (std::isnan(segments[i].extra_ions[ion]))
                segments[i].extra_ions[ion] = 1e-6f;
            // Не clamp'аем нормальные значения — даём физике работать
        }
    }

    // Шаг 3: синаптические токи — всегда GPU
    if (num_synapses > 0) {
        // Обнуляем постсинаптические NT перед атомарным накоплением в GPU-ядре
        for (uint32_t i = 0; i < num_segments; i++) {
            for (int n = 0; n < NT_COUNT; n++) segments[i].neurotransmitter[n] = 0.0f;
        }

        // Protein→parameter mapping (BION v7): CPU-side loop перед GPU-ядром.
        // Обновляет RRP_max, release_probability, RRP_recovery из nucleus protein_level.
        // USM-память доступна с CPU — копирование не требуется.
        if (bion_data.nuclei && bion_data.num_nuclei > 0) {
            for (uint32_t i = 0; i < num_synapses; i++) {
                Synapse* syn = &synapses[i];
                if (syn->id == 0) continue;
                uint32_t pre_idx = syn->pre_segment;
                if (pre_idx < num_segments) {
                    uint32_t nid = segments[pre_idx].neuron_id;
                    if (nid < bion_data.num_nuclei) {
                        rrp_apply_protein_mapping(syn, bion_data.nuclei[nid].protein_level);
                    }
                }
            }
        }

        // ── Глиальные индексы (сегменты с MOD_GLAST) для посегментного захвата NT ──
        glial_count = 0;
        for (uint32_t g = 0; g < num_segments && glial_count < glial_capacity; g++)
            for (uint8_t j = 0; j < segments[g].modifier_count; j++)
                if (segments[g].modifiers[j].type == MOD_GLAST) { glial_indices[glial_count++] = g; break; }

compute_synaptic_currents(q, segments, synapses, num_synapses, num_segments, mode, tick, dt, delay_buffer,
                          glial_indices, glial_count);
    }
    q.wait();

    // Шаг 3.5: синаптическая пластичность (CPU — STDP, eCB, timing traces)
    // Портировано из simulation_host.cpp:443-509. GPU-ядро НЕ делает пластичность.
    if (num_synapses > 0) {
        for (uint32_t i = 0; i < num_synapses; i++) {
            Synapse* syn = &synapses[i];
            if (syn->type != SYNAPSE_TYPE_CHEMICAL) continue;
            uint32_t post_idx = syn->post_segment;
            if (post_idx >= num_segments) continue;
            Segment* post = &segments[post_idx];

            // ── AMPAR trafficking: CaMKII/Calcineurin (биологическая пластичность) ──
            // Суррогатные timing traces УДАЛЕНЫ. CaMKII (Kd=1μM) vs Calcineurin
            // (Kd=0.05μM) естественно разделяют LTP/LTD через разные Ca²⁺-пороги.
            // BCM θ_m = скользящее среднее Ca²⁺² (хранится в syn->depression).

            float Ca_uM = syn->spine_Ca * 1000.0f;  // Ca²⁺ шипика (локально), не дендритного ствола
            float Ca_sq = Ca_uM * Ca_uM;
            syn->facilitation += (Ca_sq - syn->facilitation) * dt / 300000.0f;
            if (syn->facilitation < 0.0f) syn->facilitation = 0.0f;
            syn->depression = syn->facilitation + 0.01f;

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
            if (std::isnan(syn->weight)) syn->weight = 0.5f;
            if (std::isnan(syn->facilitation)) syn->facilitation = 0.0f;
            if (std::isnan(syn->depression))   syn->depression = 0.01f;
            if (syn->weight < 0.01f) syn->weight = 0.01f;
            if (syn->weight > 10.0f) syn->weight = 10.0f;

            // ── eCB retrograde signaling ──
            for (uint8_t j = 0; j < post->modifier_count; j++) {
                if (post->modifiers[j].type == MOD_ECB_SYNTHESIS) {
                    float ecb = post->modifiers[j].state[0];
                    if (ecb > 0.0f) {
                        syn->cleft_neurotransmitter[NT_ENDOCANNABINOID] += ecb * 0.1f * dt;
                        post->modifiers[j].state[0] *= 0.9f;
                    }
                    break;
                }
            }
        }
    }

    // Шаг 4: обновить мембранный потенциал (не зависит от function pointers — всегда GPU)
    if (num_segments > 0) {
update_membrane(q, segments, num_segments, dt, tick, stim_data);
    }
    q.wait();
    // Шаг 5: clamp всех полей сегмента (NaN/overflow защита, паритет с CUDA)
    if (num_segments > 0) {
compute_clamp_segments(q, segments, num_segments);
    }
    q.wait();
}
