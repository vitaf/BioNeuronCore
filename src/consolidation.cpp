// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.
//
// Реализация потока консолидации/реконсолидации.
// Моделирует: ген→белок→структурная пластичность с биологическими временами.

#include "consolidation.h"
#include "kernel_dispatch.h"
#include <cstring>
#include <cmath>

ConsolidationThread::ConsolidationThread() {}

ConsolidationThread::~ConsolidationThread() {
    stop();
    delete[] nucleus_snapshot_;
    delete[] reactivation_buffers_;
}

void ConsolidationThread::start(Segment* segments, uint32_t num_segments,
                                 Synapse* synapses, uint32_t* num_synapses_ptr,
                                 uint32_t max_synapses,
                                 Neuron* neurons, uint32_t num_neurons,
                                 Nucleus* nuclei, uint32_t num_nuclei,
                                 uint64_t* tick_counter) {
    if (running_.load()) return;
    if (!nuclei || num_nuclei == 0) return;  // нечего консолидировать

    segments_ = segments;
    synapses_ = synapses;
    neurons_ = neurons;
    nuclei_ = nuclei;
    num_segments_ = num_segments;
    num_synapses_ptr_ = num_synapses_ptr;
    max_synapses_ = max_synapses;
    num_neurons_ = num_neurons;
    num_nuclei_ = num_nuclei;
    tick_counter_ = tick_counter;
    event_count_.store(0);

    // Снимки ядер для Studio
    delete[] nucleus_snapshot_;
    nucleus_snapshot_ = new (std::nothrow) Nucleus[num_nuclei];
    if (nucleus_snapshot_)
        memcpy(nucleus_snapshot_, nuclei, num_nuclei * sizeof(Nucleus));

    // Буферы реактивации
    delete[] reactivation_buffers_;
    reactivation_buffers_ = new (std::nothrow) ReactivationBuffer[num_neurons];
    if (reactivation_buffers_)
        memset(reactivation_buffers_, 0, num_neurons * sizeof(ReactivationBuffer));

    running_.store(true);
    thread_ = std::thread(&ConsolidationThread::threadLoop, this);
}

void ConsolidationThread::stop() {
    running_.store(false);
    if (thread_.joinable()) thread_.join();
}

void ConsolidationThread::setGlobalInterval(uint32_t ticks) {
    global_interval_.store(ticks);
}

bool ConsolidationThread::getNucleusSnapshot(uint32_t neuron_idx, Nucleus* out) const {
    if (!out) return false;
    std::lock_guard<std::mutex> lock(nucleus_mutex_);
    if (!nucleus_snapshot_ || neuron_idx >= num_nuclei_) return false;
    *out = nucleus_snapshot_[neuron_idx];
    return true;
}

bool ConsolidationThread::setNucleus(uint32_t neuron_idx, const Nucleus* data) {
    if (!data || !nuclei_ || !nucleus_snapshot_ || neuron_idx >= num_nuclei_) return false;
    std::lock_guard<std::mutex> lock(nucleus_mutex_);
    nuclei_[neuron_idx] = *data;
    nucleus_snapshot_[neuron_idx] = *data;  // синхронизируем снапшот сразу
    return true;
}

void ConsolidationThread::updateNucleusFromSoma(Segment* segments, Neuron* neurons,
                                                  Nucleus* nuclei, uint32_t num_neurons) {
    if (!segments || !neurons || !nuclei) return;
    for (uint32_t n = 0; n < num_neurons; n++) {
        if (!nuclei[n].enabled) continue;
        uint32_t soma_idx = neurons[n].segment_start + neurons[n].soma_index;
        if (soma_idx >= num_segments_) continue;
        Segment* soma = &segments[soma_idx];

        // Кэш: сигнальные состояния из соматических модификаторов
        for (uint8_t j = 0; j < soma->modifier_count; j++) {
            Modifier* mod = &soma->modifiers[j];
            switch (mod->type) {
                case MOD_NB_CREB_TRANSCRIPTION:
                    nuclei[n].pcreb_level = mod->state[0];
                    break;
                case MOD_MAPK_ERK:
                    nuclei[n].erk_level = mod->state[3];  // pERK_nuclear
                    break;
                case MOD_IKK_NFKB:
                    nuclei[n].nfkb_level = mod->state[3]; // pNFkB_nuclear
                    break;
                case MOD_MTORC1:
                    nuclei[n].mtorc1_level = mod->state[0];
                    break;
            }
        }
        // Ca²⁺→CREB: прямой Ca²⁺-зависимый вход (intrinsic homeostasis)
        // CREB активируется при Ca²⁺ > 0.1μM, насыщается при >1μM
        float Ca_uM = soma->intra_ions[ION_CA] * 1000.0f;
        float ca_drive = Ca_uM * Ca_uM / (Ca_uM * Ca_uM + 0.25f + 1e-9f); // Hill n=2, Kd=0.5μM
        if (ca_drive > nuclei[n].pcreb_level)
            nuclei[n].pcreb_level += (ca_drive - nuclei[n].pcreb_level) * 0.1f;
        else
            nuclei[n].pcreb_level += (ca_drive - nuclei[n].pcreb_level) * 0.01f; // медленный спад

        nuclei[n].amino_acid_level = soma->ATP_level / 5.0f;
        if (nuclei[n].amino_acid_level > 1.0f) nuclei[n].amino_acid_level = 1.0f;
        if (nuclei[n].amino_acid_level < 0.0f) nuclei[n].amino_acid_level = 0.0f;

        // Реактивация: пишем V_m сомы в кольцевой буфер
        if (reactivation_buffers_) {
            ReactivationBuffer* rb = &reactivation_buffers_[n];
            rb->v_m_history[rb->head] = soma->V_m;
            rb->head = (rb->head + 1) % REACTIVATION_WINDOW;
            if (rb->count < REACTIVATION_WINDOW) rb->count++;
        }
    }
}

uint32_t ConsolidationThread::applyPendingEvents(Segment* segments, uint32_t num_segments,
                                                   Synapse* synapses, uint32_t* num_synapses,
                                                   uint32_t max_synapses) {
    uint32_t count = event_count_.load();
    if (count == 0) return 0;

    uint32_t applied = 0;
    std::lock_guard<std::mutex> lock(event_mutex_);

    for (uint32_t i = 0; i < count; i++) {
        ConsolidationEvent* evt = &pending_events_[i];

        switch (evt->event_type) {
            case CONSOL_EVENT_DIAMETER_CHANGE: {
                if (evt->segment_a < num_segments) {
                    float new_r = segments[evt->segment_a].radius + evt->param1;
                    if (new_r < 0.1f) new_r = 0.1f;
                    if (new_r > 100.0f) new_r = 100.0f;
                    segments[evt->segment_a].radius = new_r;
                    applied++;
                }
                break;
            }
            case CONSOL_EVENT_RECEPTOR_CHANGE: {
                // param1 = delta_density, param2 = receptor_type
                // Ищем синапсы где post_segment == segment_b
                for (uint32_t s = 0; s < *num_synapses; s++) {
                    if (synapses[s].id == 0) continue; // tombstone
                    if (synapses[s].post_segment == evt->segment_b) {
                        uint8_t rtype = (uint8_t)evt->param2;
                        for (uint8_t j = 0; j < synapses[s].post_receptor_count; j++) {
                            if (synapses[s].post_receptors[j].type == rtype) {
                                float new_d = synapses[s].post_receptors[j].density + evt->param1;
                                if (new_d < 0.0f) new_d = 0.0f;
                                if (new_d > 1.0f) new_d = 1.0f;
                                synapses[s].post_receptors[j].density = new_d;
                                break;
                            }
                        }
                        applied++;
                        break; // один синапс на пост-сегмент
                    }
                }
                break;
            }
            case CONSOL_EVENT_DELETE_SYNAPSE: {
                for (uint32_t s = 0; s < *num_synapses; s++) {
                    if (synapses[s].id == 0) continue;
                    if (synapses[s].pre_segment == evt->segment_a &&
                        synapses[s].post_segment == evt->segment_b) {
                        synapses[s].id = 0; // tombstone
                        applied++;
                        break;
                    }
                }
                break;
            }
            case CONSOL_EVENT_NEW_SYNAPSE: {
                // Ищем свободный слот (tombstone или за пределами num_synapses)
                bool found = false;
                for (uint32_t s = 0; s < *num_synapses; s++) {
                    if (synapses[s].id == 0) {
                        // Переиспользуем tombstone
                        synapses[s].id = (uint32_t)((*num_synapses_ptr_ + 1) * 1000 + s);
                        synapses[s].pre_segment = evt->segment_a;
                        synapses[s].post_segment = evt->segment_b;
                        synapses[s].type = SYNAPSE_TYPE_CHEMICAL;
                        synapses[s].weight = evt->param1;
                        synapses[s].pre_area = evt->param3;
                        synapses[s].post_area = evt->param2;
                        synapses[s].delay = 1.0f;
                        synapses[s].release_probability = 0.5f;
                        synapses[s].tau_clearance = 100.0f;
                        // Добавляем AMPA-рецептор по умолчанию
                        synapses[s].post_receptor_count = 1;
                        synapses[s].post_receptors[0].type = MOD_NT_RECEPTOR_AMPA;
                        synapses[s].post_receptors[0].density = 1.0f;
                        found = true;
                        applied++;
                        break;
                    }
                }
                if (!found && *num_synapses < max_synapses) {
                    uint32_t idx = (*num_synapses)++;
                    synapses[idx].id = (uint32_t)((*num_synapses_ptr_) * 1000 + idx);
                    synapses[idx].pre_segment = evt->segment_a;
                    synapses[idx].post_segment = evt->segment_b;
                    synapses[idx].type = SYNAPSE_TYPE_CHEMICAL;
                    synapses[idx].weight = evt->param1;
                    synapses[idx].pre_area = evt->param3;
                    synapses[idx].post_area = evt->param2;
                    synapses[idx].delay = 1.0f;
                    synapses[idx].release_probability = 0.5f;
                    synapses[idx].tau_clearance = 100.0f;
                    synapses[idx].post_receptor_count = 1;
                    synapses[idx].post_receptors[0].type = MOD_NT_RECEPTOR_AMPA;
                    synapses[idx].post_receptors[0].density = 1.0f;
                    applied++;
                }
                break;
            }
            case CONSOL_EVENT_MODIFIER_DENSITY: {
                // param1 = modifier_type, param2 = new_density
                uint8_t mtype = (uint8_t)evt->param1;
                float new_density = evt->param2;
                if (new_density < 0.001f) new_density = 0.001f;
                if (new_density > 5.0f) new_density = 5.0f;

                // Обновляем плотность на всех сегментах нейрона, имеющих этот модификатор
                for (uint32_t s = 0; s < num_segments; s++) {
                    if (segments[s].neuron_id != evt->neuron_id) continue;
                    for (uint8_t j = 0; j < segments[s].modifier_count; j++) {
                        if (segments[s].modifiers[j].type == mtype) {
                            segments[s].modifiers[j].density = new_density;
                            applied++;
                            break;
                        }
                    }
                }
                break;
            }
        }
    }

    event_count_.store(0);
    return applied;
}

void ConsolidationThread::threadLoop() {
    uint64_t last_check_tick = 0;

    while (running_.load()) {
        uint64_t current_tick = tick_counter_ ? *tick_counter_ : 0;
        uint32_t interval = global_interval_.load();
        if (interval == 0) {
            // Консолидация отключена — спим
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        if (current_tick - last_check_tick >= interval) {
            last_check_tick = current_tick;
            runConsolidationCycle(current_tick);

            // Обновить снимок ядер для Studio
            if (nucleus_snapshot_ && nuclei_) {
                std::lock_guard<std::mutex> lock(nucleus_mutex_);
                memcpy(nucleus_snapshot_, nuclei_, num_nuclei_ * sizeof(Nucleus));
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void ConsolidationThread::runConsolidationCycle(uint64_t current_tick) {
    if (!segments_ || !synapses_ || !neurons_ || !nuclei_ || !num_synapses_ptr_) return;
    uint32_t n_syn = *num_synapses_ptr_;

    // ── Фаза 1: обновление генной активности из сигналов ────────────
    for (uint32_t n = 0; n < num_nuclei_; n++) {
        Nucleus* nc = &nuclei_[n];
        if (!nc->enabled) continue;
        uint32_t interval = nc->consolidation_interval_ticks;
        if (interval == 0) interval = global_interval_.load();
        if (nc->last_consolidation_tick + interval > current_tick) continue;
        nc->last_consolidation_tick = (uint32_t)current_tick;

        float pCREB = nc->pcreb_level;
        float ERK   = nc->erk_level;
        float NFkB  = nc->nfkb_level;

        // Консолидация (CREB-зависимые гены)
        nc->gene_activity[GENE_CFOS]    += (pCREB * 0.5f - nc->gene_activity[GENE_CFOS]) * 0.01f;
        nc->gene_activity[GENE_BDNF]    += (pCREB * 0.6f + ERK * 0.3f - nc->gene_activity[GENE_BDNF]) * 0.01f;
        nc->gene_activity[GENE_ARC]     += (pCREB * 0.4f + ERK * 0.4f - nc->gene_activity[GENE_ARC]) * 0.01f;
        nc->gene_activity[GENE_COFILIN] += (ERK * 0.4f - nc->gene_activity[GENE_COFILIN]) * 0.008f;
        nc->gene_activity[GENE_MMP9]    += ((ERK + NFkB) * 0.3f - nc->gene_activity[GENE_MMP9]) * 0.005f;
        nc->gene_activity[GENE_TIMP]    += ((1.0f - nc->gene_activity[GENE_MMP9]) * 0.3f - nc->gene_activity[GENE_TIMP]) * 0.005f;

        // Реконсолидация (NF-kB-зависимые гены)
        nc->gene_activity[GENE_ZIF268]  += (NFkB * 0.5f - nc->gene_activity[GENE_ZIF268]) * 0.01f;

        // Структурные гены (зависят от консолидации и реконсолидации)
        nc->gene_activity[GENE_GLUR1]   += (nc->gene_activity[GENE_CFOS] * 0.3f - nc->gene_activity[GENE_GLUR1]) * 0.005f;
        nc->gene_activity[GENE_GLUR2]   += (nc->gene_activity[GENE_CFOS] * 0.25f - nc->gene_activity[GENE_GLUR2]) * 0.005f;
        nc->gene_activity[GENE_PSD95]   += (nc->gene_activity[GENE_ARC] * 0.3f - nc->gene_activity[GENE_PSD95]) * 0.005f;
        nc->gene_activity[GENE_NEUREXIN]+= (nc->gene_activity[GENE_BDNF] * 0.2f - nc->gene_activity[GENE_NEUREXIN]) * 0.004f;
        nc->gene_activity[GENE_NEUROLIGIN]+=(nc->gene_activity[GENE_BDNF] * 0.2f - nc->gene_activity[GENE_NEUROLIGIN]) * 0.004f;
        nc->gene_activity[GENE_INTEGRINB3]+= (nc->gene_activity[GENE_MMP9] * 0.15f - nc->gene_activity[GENE_INTEGRINB3]) * 0.003f;
        nc->gene_activity[GENE_DYNAMIN] += (pCREB * 0.2f - nc->gene_activity[GENE_DYNAMIN]) * 0.003f;
        nc->gene_activity[GENE_CLATHRIN]+= (pCREB * 0.2f - nc->gene_activity[GENE_CLATHRIN]) * 0.003f;
        nc->gene_activity[GENE_TRKB]    += (nc->gene_activity[GENE_BDNF] * 0.15f - nc->gene_activity[GENE_TRKB]) * 0.004f;
        nc->gene_activity[GENE_MTOR]    += (nc->amino_acid_level * 0.3f - nc->gene_activity[GENE_MTOR]) * 0.003f;
        nc->gene_activity[GENE_S6K1]    += (nc->mtorc1_level * 0.3f - nc->gene_activity[GENE_S6K1]) * 0.005f;
        nc->gene_activity[GENE_4EBP]    += (nc->mtorc1_level * 0.25f - nc->gene_activity[GENE_4EBP]) * 0.005f;

        // CREB auto-regulates
        nc->gene_activity[GENE_CREB]    += (pCREB * 0.1f - nc->gene_activity[GENE_CREB]) * 0.002f;

        // ── Транскрипционные факторы → ионные каналы (GRN) ──────────
        // TFs регулируются pCREB (activity-dependent) и имеют базовый уровень
        // от типа нейрона (задаётся при дифференцировке)
        nc->gene_activity[GENE_TF_LHX3]   += (pCREB * 0.15f - nc->gene_activity[GENE_TF_LHX3]) * 0.003f;
        nc->gene_activity[GENE_TF_ISLET1] += (pCREB * 0.12f - nc->gene_activity[GENE_TF_ISLET1]) * 0.003f;
        nc->gene_activity[GENE_TF_PAX2]   += (pCREB * 0.08f - nc->gene_activity[GENE_TF_PAX2]) * 0.003f;
        nc->gene_activity[GENE_TF_BRN3A]  += (pCREB * 0.10f - nc->gene_activity[GENE_TF_BRN3A]) * 0.003f;
        nc->gene_activity[GENE_TF_MNX1]   += (pCREB * 0.10f - nc->gene_activity[GENE_TF_MNX1]) * 0.003f;
        nc->gene_activity[GENE_TF_LMX1B]  += (pCREB * 0.08f - nc->gene_activity[GENE_TF_LMX1B]) * 0.003f;

        // TFs → ионные каналы (регуляторная сеть)
        float LHX3= nc->gene_activity[GENE_TF_LHX3], ISL1= nc->gene_activity[GENE_TF_ISLET1];
        float PAX2= nc->gene_activity[GENE_TF_PAX2],   BRN3A=nc->gene_activity[GENE_TF_BRN3A];
        float MNX1= nc->gene_activity[GENE_TF_MNX1],   LMX1B=nc->gene_activity[GENE_TF_LMX1B];

        // Nav1.1: LHX3 + BRN3A + ISL1
        nc->gene_activity[GENE_SCN1A] += (LHX3*0.4f + BRN3A*0.3f + ISL1*0.3f - nc->gene_activity[GENE_SCN1A])*0.005f;
        // Kv4.2: LHX3 + MNX1
        nc->gene_activity[GENE_KCND2] += (LHX3*0.5f + MNX1*0.3f - nc->gene_activity[GENE_KCND2])*0.005f;
        // Kv7.2/7.3: PAX2 + LMX1B (GABAergic + diencephalic)
        nc->gene_activity[GENE_KCNQ2] += (PAX2*0.3f + LMX1B*0.4f - nc->gene_activity[GENE_KCNQ2])*0.005f;
        nc->gene_activity[GENE_KCNQ3] += (PAX2*0.2f + LMX1B*0.3f - nc->gene_activity[GENE_KCNQ3])*0.005f;
        // Cav3.1 (T-type): ISL1 + LHX3 (PIR)
        nc->gene_activity[GENE_CACNA1G] += (ISL1*0.5f + LHX3*0.3f - nc->gene_activity[GENE_CACNA1G])*0.005f;
        // Изоформы #8 (46-56): TF-ассоциация по родственникам
        nc->gene_activity[GENE_CACNA1H] += (ISL1*0.5f + LHX3*0.3f - nc->gene_activity[GENE_CACNA1H])*0.005f;  // Cav3.2 как Cav3.1
        nc->gene_activity[GENE_KCND3]  += (LHX3*0.5f + MNX1*0.3f - nc->gene_activity[GENE_KCND3])*0.005f;      // Kv4.3 как Kv4.2
        nc->gene_activity[GENE_KCNC2]  += (LHX3*0.4f + ISL1*0.4f - nc->gene_activity[GENE_KCNC2])*0.005f;      // Kv3.2
        nc->gene_activity[GENE_KCNC3]  += (LHX3*0.4f + ISL1*0.4f - nc->gene_activity[GENE_KCNC3])*0.005f;      // Kv3.3
        nc->gene_activity[GENE_KCNB2]  += (LHX3*0.4f + MNX1*0.3f - nc->gene_activity[GENE_KCNB2])*0.005f;      // Kv2.2
        nc->gene_activity[GENE_KCNA4]  += (BRN3A*0.3f + PAX2*0.3f - nc->gene_activity[GENE_KCNA4])*0.005f;     // Kv1.4
        nc->gene_activity[GENE_KCNA6]  += (BRN3A*0.3f + PAX2*0.3f - nc->gene_activity[GENE_KCNA6])*0.005f;     // Kv1.6
        nc->gene_activity[GENE_KCNJ12] += (PAX2*0.4f - nc->gene_activity[GENE_KCNJ12])*0.005f;                  // Kir2.2 как Kir2.1
        nc->gene_activity[GENE_KCNN4]  += (ISL1*0.4f - nc->gene_activity[GENE_KCNN4])*0.005f;                   // KCa3.1
        nc->gene_activity[GENE_TRPC3]  += (ISL1*0.3f + BRN3A*0.3f - nc->gene_activity[GENE_TRPC3])*0.005f;      // TRPC3
        nc->gene_activity[GENE_TRPM7]  += (ISL1*0.3f - nc->gene_activity[GENE_TRPM7])*0.005f;                   // TRPM7
        // HCN1/2: ISL1
        nc->gene_activity[GENE_HCN1] += (ISL1*0.4f - nc->gene_activity[GENE_HCN1])*0.005f;
        nc->gene_activity[GENE_HCN2] += (ISL1*0.5f - nc->gene_activity[GENE_HCN2])*0.005f;
        // Kir2.1: PAX2 (GABAergic)
        nc->gene_activity[GENE_KCNJ2] += (PAX2*0.4f - nc->gene_activity[GENE_KCNJ2])*0.005f;
        // K2P (TREK-1): BRN3A + PAX2 (sensory + GABAergic)
        nc->gene_activity[GENE_KCNK2] += (BRN3A*0.3f + PAX2*0.3f - nc->gene_activity[GENE_KCNK2])*0.005f;
        // BK: MNX1 + LHX3 (motor + dIN)
        nc->gene_activity[GENE_KCNMA1] += (MNX1*0.4f + LHX3*0.3f - nc->gene_activity[GENE_KCNMA1])*0.005f;
        // KCNT1 (KNa): universal baseline + activity-dependent via pCREB
        nc->gene_activity[GENE_KCNT1] += (pCREB*0.5f + LHX3*0.2f + ISL1*0.3f - nc->gene_activity[GENE_KCNT1])*0.005f;
        // Vesicular genes: pCREB-driven + baseline
        nc->gene_activity[GENE_SYN1]   += (pCREB*0.6f - nc->gene_activity[GENE_SYN1])*0.003f;
        nc->gene_activity[GENE_VAMP2]  += (pCREB*0.4f + 0.3f - nc->gene_activity[GENE_VAMP2])*0.003f;
        nc->gene_activity[GENE_SNAP25] += (pCREB*0.4f + 0.3f - nc->gene_activity[GENE_SNAP25])*0.003f;
        nc->gene_activity[GENE_STX1A]  += (pCREB*0.4f + 0.3f - nc->gene_activity[GENE_STX1A])*0.003f;
        nc->gene_activity[GENE_SYT1]   += (pCREB*0.5f - nc->gene_activity[GENE_SYT1])*0.003f;
        nc->gene_activity[GENE_SYT7]   += (pCREB*0.3f - nc->gene_activity[GENE_SYT7])*0.003f;
        nc->gene_activity[GENE_UNC13A] += (pCREB*0.5f - nc->gene_activity[GENE_UNC13A])*0.003f;
        nc->gene_activity[GENE_RIMS1]  += (pCREB*0.4f - nc->gene_activity[GENE_RIMS1])*0.003f;
        nc->gene_activity[GENE_CPLX1]  += (pCREB*0.3f + 0.2f - nc->gene_activity[GENE_CPLX1])*0.003f;

        // Clamp
        for (int g = 0; g < GENE_COUNT; g++) {
            if (nc->gene_activity[g] < 0.0f) nc->gene_activity[g] = 0.0f;
            if (nc->gene_activity[g] > 1.0f) nc->gene_activity[g] = 1.0f;
        }
    }

    // ── Фаза 2: трансляция ген→белок ────────────────────────────────
    for (uint32_t n = 0; n < num_nuclei_; n++) {
        Nucleus* nc = &nuclei_[n];
        if (!nc->enabled) continue;
        float synth_rate = nc->protein_synthesis_rate;
        if (synth_rate < 0.001f) synth_rate = 0.001f;

        for (int g = 0; g < GENE_COUNT; g++) {
            nc->protein_level[g] += (nc->gene_activity[g] - nc->protein_level[g]) * 0.003f * synth_rate;
            if (nc->protein_level[g] < 0.0f) nc->protein_level[g] = 0.0f;
            if (nc->protein_level[g] > 1.0f) nc->protein_level[g] = 1.0f;
        }

        // Читаем translation_rate из соматического PROTEIN_SYNTH
        uint32_t soma_idx = neurons_[n].segment_start + neurons_[n].soma_index;
        if (soma_idx < num_segments_) {
            Segment* soma = &segments_[soma_idx];
            for (uint8_t j = 0; j < soma->modifier_count; j++) {
                if (soma->modifiers[j].type == MOD_PROTEIN_SYNTH) {
                    nc->protein_synthesis_rate = soma->modifiers[j].state[0];
                }
                // Передаём amino_acid_level в MTORC1
                if (soma->modifiers[j].type == MOD_MTORC1) {
                    soma->modifiers[j].state[3] = nc->amino_acid_level;
                }
            }
        }
    }

    // ── Protein→synapse mapping (BION v7): protein levels → RRP параметры ──
    // Био: трансляция белков (Munc13-1, SYT1, CPLX1) меняет везикулярную динамику.
    // Выполняется ПОСЛЕ Phase 2 (protein_level[] обновлён) и ДО Phase 3 (структурные решения).
    // Этот проход автоматически работает для CPU/GPU/daemon — все пути читают поля Synapse.
    if (synapses_ && num_synapses_ptr_ && segments_) {
        uint32_t n_syn = *num_synapses_ptr_;
        for (uint32_t s = 0; s < n_syn; s++) {
            Synapse* syn = &synapses_[s];
            if (syn->id == 0) continue; // tombstone
            if (!syn->post_receptor_count) continue; // gap junction
            uint32_t pre_seg = syn->pre_segment;
            if (pre_seg < num_segments_) {
                uint32_t pre_nid = segments_[pre_seg].neuron_id;
                if (pre_nid < num_nuclei_) {
                    rrp_apply_protein_mapping(syn, nuclei_[pre_nid].protein_level);
                }
            }
        }
    }

    // ── Фаза 3: структурные решения ─────────────────────────────────
    std::lock_guard<std::mutex> lock(event_mutex_);
    if (event_count_.load() >= MAX_CONSOL_EVENTS - 10) return; // почти заполнена

    for (uint32_t n = 0; n < num_nuclei_; n++) {
        Nucleus* nc = &nuclei_[n];
        if (!nc->enabled) continue;
        int events_for_this_neuron = 0;

        // ── 3a: Удаление синапсов (LTD-критерий) ──────────────────
        // Calcineurin > 0.3 AND weight < 0.05 AND MMP9 < 0.3
        for (uint32_t s = 0; s < n_syn && event_count_.load() < MAX_CONSOL_EVENTS; s++) {
            if (synapses_[s].id == 0) continue;
            Synapse* syn = &synapses_[s];
            if (syn->post_segment >= num_segments_) continue;
            Segment* post = &segments_[syn->post_segment];
            if (post->neuron_id != nc->neuron_id) continue;

            float calcineurin = 0.0f;
            for (uint8_t j = 0; j < post->modifier_count; j++)
                if (post->modifiers[j].type == MOD_CALCINEURIN)
                    calcineurin = post->modifiers[j].state[0];

            if (calcineurin > 0.3f && syn->weight < 0.05f && nc->protein_level[GENE_MMP9] < 0.3f) {
                uint32_t evt_idx = event_count_.fetch_add(1);
                if (evt_idx < MAX_CONSOL_EVENTS) {
                    pending_events_[evt_idx].event_type = CONSOL_EVENT_DELETE_SYNAPSE;
                    pending_events_[evt_idx].neuron_id = nc->neuron_id;
                    pending_events_[evt_idx].segment_a = syn->pre_segment;
                    pending_events_[evt_idx].segment_b = syn->post_segment;
                    pending_events_[evt_idx].apply_tick = current_tick + 100;
                    events_for_this_neuron++;
                }
                break; // не более 1 удаления за цикл
            }
        }

        // ── 3b: Новые синапсы (synaptogenesis) ──────────────────────
        if (nc->protein_level[GENE_ARC] > 0.6f &&
            nc->protein_level[GENE_PSD95] > 0.5f &&
            nc->protein_level[GENE_MMP9] > 0.3f &&
            nc->protein_level[GENE_BDNF] > 0.5f &&
            events_for_this_neuron == 0) {

            // Найти ближайший сегмент другого нейрона
            uint32_t soma_idx = neurons_[n].segment_start + neurons_[n].soma_index;
            float best_dist = 50.0f; // радиус поиска 50 мкм
            uint32_t best_seg = UINT32_MAX;
            uint32_t best_post_seg = soma_idx;

            for (uint32_t i = 0; i < num_segments_; i++) {
                if (segments_[i].neuron_id == nc->neuron_id) continue;
                float dx = segments_[i].position.x - segments_[soma_idx].position.x;
                float dy = segments_[i].position.y - segments_[soma_idx].position.y;
                float dz = segments_[i].position.z - segments_[soma_idx].position.z;
                float dist = sqrtf(dx*dx + dy*dy + dz*dz);
                if (dist < best_dist && dist > 0.1f) {
                    bool already_connected = false;
                    for (uint32_t s = 0; s < n_syn; s++) {
                        if (synapses_[s].id != 0 &&
                            synapses_[s].pre_segment == i &&
                            synapses_[s].post_segment == soma_idx) {
                            already_connected = true; break;
                        }
                    }
                    if (!already_connected) { best_dist = dist; best_seg = i; }
                }
            }

            if (best_seg != UINT32_MAX) {
                uint32_t evt_idx = event_count_.fetch_add(1);
                if (evt_idx < MAX_CONSOL_EVENTS) {
                    pending_events_[evt_idx].event_type = CONSOL_EVENT_NEW_SYNAPSE;
                    pending_events_[evt_idx].neuron_id = nc->neuron_id;
                    pending_events_[evt_idx].segment_a = best_seg;
                    pending_events_[evt_idx].segment_b = best_post_seg;
                    pending_events_[evt_idx].param1 = 0.1f;  // weight
                    pending_events_[evt_idx].param2 = 0.1f;  // post_area
                    pending_events_[evt_idx].param3 = 0.1f;  // pre_area
                    pending_events_[evt_idx].apply_tick = current_tick + 100;
                }
            }
        }

        // ── 3c: Изменение диаметра (Cofilin/F-actin) ────────────────
        float F_actin = 0.5f; // default
        uint32_t soma_idx = neurons_[n].segment_start + neurons_[n].soma_index;
        if (soma_idx < num_segments_) {
            for (uint8_t j = 0; j < segments_[soma_idx].modifier_count; j++)
                if (segments_[soma_idx].modifiers[j].type == MOD_COFILIN)
                    F_actin = segments_[soma_idx].modifiers[j].state[3];
        }

        float cofilin_protein = nc->protein_level[GENE_COFILIN];
        float delta_r = (F_actin - 0.5f) * 0.02f * segments_[soma_idx].radius * cofilin_protein;
        if (delta_r > 0.2f * segments_[soma_idx].radius)
            delta_r = 0.2f * segments_[soma_idx].radius;
        if (delta_r < -0.2f * segments_[soma_idx].radius)
            delta_r = -0.2f * segments_[soma_idx].radius;

        if (fabsf(delta_r) > 0.001f) {
            // Применяем к соме и дендритам нейрона
            for (uint32_t i = neurons_[n].segment_start;
                 i < neurons_[n].segment_start + neurons_[n].segment_count; i++) {
                uint32_t evt_idx = event_count_.fetch_add(1);
                if (evt_idx < MAX_CONSOL_EVENTS) {
                    pending_events_[evt_idx].event_type = CONSOL_EVENT_DIAMETER_CHANGE;
                    pending_events_[evt_idx].neuron_id = nc->neuron_id;
                    pending_events_[evt_idx].segment_a = i;
                    pending_events_[evt_idx].param1 = delta_r;
                    pending_events_[evt_idx].apply_tick = current_tick + 100;
                }
            }
        }

        // ── 3d: Изменение плотности AMPA-рецепторов ─────────────────
        float gluR_change = (nc->protein_level[GENE_GLUR1] + nc->protein_level[GENE_GLUR2]) * 0.5f;
        gluR_change = (gluR_change - 0.5f) * 0.01f; // ±0.5%

        if (fabsf(gluR_change) > 0.0005f) {
            for (uint32_t s = 0; s < n_syn && event_count_.load() < MAX_CONSOL_EVENTS; s++) {
                if (synapses_[s].id == 0) continue;
                if (synapses_[s].post_segment >= num_segments_) continue;
                if (segments_[synapses_[s].post_segment].neuron_id != nc->neuron_id) continue;

                uint32_t evt_idx = event_count_.fetch_add(1);
                if (evt_idx < MAX_CONSOL_EVENTS) {
                    pending_events_[evt_idx].event_type = CONSOL_EVENT_RECEPTOR_CHANGE;
                    pending_events_[evt_idx].neuron_id = nc->neuron_id;
                    pending_events_[evt_idx].segment_b = synapses_[s].post_segment;
                    pending_events_[evt_idx].param1 = gluR_change;
                    pending_events_[evt_idx].param2 = MOD_NT_RECEPTOR_AMPA;
                    pending_events_[evt_idx].apply_tick = current_tick + 100;
                }
                break; // один синапс за цикл
            }
        }

        // ── Генная экспрессия → плотность ионных каналов ────────────
        // Gene→modifier mapping: protein_level → modifier.density
        // Каждые ~10 циклов консолидации эмитим события изменения плотности
        static const int16_t gene_to_mod[57] = {
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 0-9: plasticity
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 10-19: plasticity
            -1, -1, -1, -1, -1, -1,                    // 20-25: TFs
            199, 85, 112, 112, 200, 129, 130, 106, 107, 104, // 26-35: ion channels (SCN1A→199 Nav1.1, CACNA1G→200 Cav3.1)
            -1, -1, -1, -1, -1, -1, -1, -1, -1,          // 36-44: vesicular genes (no direct mod mapping)
            121,                                              // 45: KCNT1→MOD_KNA_CHANNEL
            201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211 // 46-56: изоформы #8 (Cav3.2/Kv4.3/Kv3.2/Kv3.3/Kv2.2/Kv1.4/Kv1.6/Kir2.2/KCa3.1/TRPC3/TRPM7)
        };

        for (int g = 26; g < 57; g++) {
            int16_t mod_type = gene_to_mod[g];
            if (mod_type < 0) continue;

            float target_density = nc->protein_level[g];
            // KCNQ2+KCNQ3 → среднее для Kv7
            if (g == GENE_KCNQ2) target_density = (nc->protein_level[GENE_KCNQ2] + nc->protein_level[GENE_KCNQ3]) * 0.5f;
            if (g == GENE_KCNQ3) continue; // обработан вместе с KCNQ2
            // HCN1+HCN2 → среднее для HCN
            if (g == GENE_HCN1) target_density = (nc->protein_level[GENE_HCN1] + nc->protein_level[GENE_HCN2]) * 0.5f;
            if (g == GENE_HCN2) continue; // обработан вместе с HCN1

            if (event_count_.load() >= MAX_CONSOL_EVENTS - 10) break;

            uint32_t evt_idx = event_count_.fetch_add(1);
            if (evt_idx < MAX_CONSOL_EVENTS) {
                pending_events_[evt_idx].event_type = CONSOL_EVENT_MODIFIER_DENSITY;
                pending_events_[evt_idx].neuron_id = nc->neuron_id;
                pending_events_[evt_idx].param1 = (float)mod_type;  // тип модификатора
                pending_events_[evt_idx].param2 = target_density;    // новая плотность
                pending_events_[evt_idx].apply_tick = current_tick + 200;
            }
        }
    }
}
