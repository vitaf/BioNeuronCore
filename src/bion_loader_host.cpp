// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#include "bion_loader.h"
#include "daemon_log.h"
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <new>
#include <vector>

// Коды ошибок (отрицательные)
#define BION_ERR_FILE_OPEN    -1
#define BION_ERR_BAD_MAGIC    -2
#define BION_ERR_BAD_VERSION  -3
#define BION_ERR_ZERO_SEGS    -4
#define BION_ERR_ALLOC        -5
#define BION_ERR_BAD_CONN     -6
#define BION_ERR_BAD_SYNAPSE  -7
#define BION_ERR_BAD_NEURON   -8
#define BION_ERR_BAD_MODCOUNT -9
#define BION_ERR_WRITE_FAIL   -10
#define BION_ERR_FILE_READ    -11

int bion_load_host(const char* filename, BionData* data) {
    memset(data, 0, sizeof(BionData));

    FILE* f = fopen(filename, "rb");
    if (!f) return BION_ERR_FILE_OPEN;

    // Читаем базовый заголовок (68 байт, совместимо с v3/v4)
    // v5: +16 байт (num_nuclei, nucleus_version, _reserved_hdr[2])
    // v10: +16 байт (file_sz_* fields)
    size_t base_hdr_size = BION_HDR_SIZE_V3;  // 68 байт
    if (fread(&data->header, base_hdr_size, 1, f) != 1) {
        fclose(f);
        return BION_ERR_FILE_READ;
    }
    if (data->header.version >= 5) {
        uint8_t* hdr_end = ((uint8_t*)&data->header) + base_hdr_size;
        size_t ext_sz = (data->header.version >= 10)
            ? (BION_HDR_SIZE_V10 - BION_HDR_SIZE_V3)   // 32 байта до v10
            : (BION_HDR_SIZE_V5 - BION_HDR_SIZE_V3);    // 16 байт до v5
        if (fread(hdr_end, ext_sz, 1, f) != 1) {
            fclose(f);
            return BION_ERR_FILE_READ;
        }
    }

    if (data->header.magic != BION_MAGIC) {
        DLOG_ERROR("BION:bad magic 0x%08X, expected 0x%08X",
                data->header.magic, BION_MAGIC);
        fclose(f);
        return BION_ERR_BAD_MAGIC;
    }

    if (data->header.version < BION_VERSION_MIN ||
        data->header.version > BION_VERSION_MAX) {
        DLOG_ERROR("BION:unsupported version %u, expected %u",
                data->header.version, BION_VERSION);
        fclose(f);
        return BION_ERR_BAD_VERSION;
    }

    if (data->header.num_segments == 0) {
        DLOG_ERROR("BION:network has zero segments");
        fclose(f);
        return BION_ERR_ZERO_SEGS;
    }

    data->num_neurons    = data->header.num_neurons;
    data->num_segments   = data->header.num_segments;
    data->num_connections = data->header.num_intra_connections;
    data->num_synapses   = data->header.num_synapses;

    // Host-аллокация через new[]
    data->neurons     = new (std::nothrow) Neuron[data->num_neurons];
    data->segments    = new (std::nothrow) Segment[data->num_segments];
    data->connections = new (std::nothrow) IntraConnection[data->num_connections];
    data->synapses    = new (std::nothrow) Synapse[data->num_synapses];

    if ((data->num_neurons > 0 && !data->neurons) ||
        !data->segments ||
        (data->num_connections > 0 && !data->connections) ||
        (data->num_synapses > 0 && !data->synapses)) {
        bion_unload_host(data);
        fclose(f);
        return BION_ERR_ALLOC;
    }

    if (data->num_neurons > 0) {
        if (fread(data->neurons, sizeof(Neuron), data->num_neurons, f) != data->num_neurons) {
            bion_unload_host(data); fclose(f);
            return BION_ERR_FILE_READ;
        }
    }

    if (data->header.version >= 7) {
        // v7-v9: Segment 1692 байта (NT_COUNT=14, без ER_Ca2) → remap; v10: file_sz_segment.
        if (bion_read_segments(f, data->segments, data->num_segments,
                               data->header.version, data->header.file_sz_segment)
                != data->num_segments) {
            bion_unload_host(data); fclose(f);
            return BION_ERR_FILE_READ;
        }
    } else if (data->header.version >= 4) {
        // v4-v6: старый формат с MAX_MODIFIERS=16 (sizeof=988)
        #pragma pack(push, 1)
        struct V4Segment {
            uint32_t id; uint32_t neuron_id;
            Vec3 position; Vec3 direction;
            float length; float radius; float V_m;
            float intra_ions[5]; float extra_ions[5];
            float intra_nt[14]; float neurotransmitter[14];
            Modifier modifiers[16];
            uint8_t modifier_count; uint8_t segment_type; uint8_t _reserved1[2];
            float ATP_level; float temperature; float I_buffer; float transcription_signal;
            uint32_t intra_connection_ids[16]; uint8_t intra_connection_count; uint8_t _reserved2[3];
        };
        #pragma pack(pop)
        V4Segment v4seg;
        for (uint32_t i = 0; i < data->num_segments; i++) {
            if (fread(&v4seg, sizeof(V4Segment), 1, f) != 1) {
                bion_unload_host(data); fclose(f);
                return BION_ERR_FILE_READ;
            }
            Segment* s = &data->segments[i];
            memset(s, 0, sizeof(Segment));
            // Поля до modifiers — идентичны
            memcpy(s, &v4seg, offsetof(V4Segment, modifiers));
            // Модификаторы: копируем только первые 16
            s->modifier_count = v4seg.modifier_count;
            if (s->modifier_count > 16) s->modifier_count = 16;
            for (int j = 0; j < s->modifier_count; j++)
                memcpy(&s->modifiers[j], &v4seg.modifiers[j], sizeof(Modifier));
            // Хвостовые поля — копируем в новые позиции
            s->segment_type = v4seg.segment_type;
            s->ATP_level = v4seg.ATP_level;
            s->temperature = v4seg.temperature;
            s->I_buffer = v4seg.I_buffer;
            s->transcription_signal = v4seg.transcription_signal;
            for (int j = 0; j < 16; j++) s->intra_connection_ids[j] = v4seg.intra_connection_ids[j];
            s->intra_connection_count = v4seg.intra_connection_count;
        }
    } else {
        // v3 → v4 миграция
        #pragma pack(push, 1)
        struct V3Modifier {
            uint8_t  type;
            float    density;
            float    state[8];
            uint8_t  active_vars;
            uint8_t  flags;
            uint8_t  _reserved[2];
        };
        struct V3Segment {
            uint32_t id;
            uint32_t neuron_id;
            Vec3     position;
            float    length;
            float    radius;
            float    V_m;
            float    intra_ions[4];
            float    extra_ions[4];
            float    neurotransmitter[12];
            V3Modifier modifiers[16];
            uint8_t  modifier_count;
            uint8_t  _reserved[3];
            float    ATP_level;
            float    temperature;
            float    I_buffer;
            uint32_t intra_connection_ids[16];
            uint8_t  intra_connection_count;
            uint8_t  _reserved2[3];
        };
        #pragma pack(pop)

        V3Segment v3seg;
        for (uint32_t i = 0; i < data->num_segments; i++) {
            if (fread(&v3seg, sizeof(V3Segment), 1, f) != 1) {
                bion_unload_host(data); fclose(f);
                return BION_ERR_FILE_READ;
            }
            Segment* s = &data->segments[i];
            memset(s, 0, sizeof(Segment));
            s->id = v3seg.id;
            s->neuron_id = v3seg.neuron_id;
            s->position = v3seg.position;
            s->direction = {0, 1, 0};
            s->length = v3seg.length;
            s->radius = v3seg.radius;
            s->V_m = v3seg.V_m;
            for (int j = 0; j < 4; j++)  s->intra_ions[j] = v3seg.intra_ions[j];
            for (int j = 0; j < 4; j++)  s->extra_ions[j] = v3seg.extra_ions[j];
            for (int j = 0; j < 12; j++) s->neurotransmitter[j] = v3seg.neurotransmitter[j];
            s->modifier_count = v3seg.modifier_count;
            s->segment_type = 0;
            s->ATP_level = v3seg.ATP_level;
            s->temperature = v3seg.temperature;
            s->I_buffer = v3seg.I_buffer;
            for (int j = 0; j < 16; j++) s->intra_connection_ids[j] = v3seg.intra_connection_ids[j];
            s->intra_connection_count = v3seg.intra_connection_count;
            for (int j = 0; j < s->modifier_count; j++) {
                s->modifiers[j].type = v3seg.modifiers[j].type;
                s->modifiers[j].density = v3seg.modifiers[j].density;
                for (int k = 0; k < 8; k++) s->modifiers[j].state[k] = v3seg.modifiers[j].state[k];
                s->modifiers[j].active_vars = v3seg.modifiers[j].active_vars;
                s->modifiers[j].flags = v3seg.modifiers[j].flags;
            }
        }
    }

    if (data->num_connections > 0) {
        if (data->header.version <= 8) {
            // v3-v8: IntraConnection 12 байт (без _pad). Читаем и расширяем до 16 байт.
            struct OldIntraConnection { uint32_t segment_a; uint32_t segment_b; float coupling_resistance; };
            auto* old_conns = new (std::nothrow) OldIntraConnection[data->num_connections];
            if (!old_conns) { bion_unload_host(data); fclose(f); return BION_ERR_ALLOC; }
            if (fread(old_conns, sizeof(OldIntraConnection), data->num_connections, f) != data->num_connections) {
                delete[] old_conns; bion_unload_host(data); fclose(f);
                return BION_ERR_FILE_READ;
            }
            for (uint32_t i = 0; i < data->num_connections; i++) {
                data->connections[i].segment_a = old_conns[i].segment_a;
                data->connections[i].segment_b = old_conns[i].segment_b;
                // v3-v8: coupling_resistance (float Ом) → флаг is_intracellular (сопротивление больше не храним)
                data->connections[i].is_intracellular = (old_conns[i].coupling_resistance != 0.0f) ? 1 : 0;
                data->connections[i]._pad = 0;
            }
            delete[] old_conns;
        } else {
            if (fread(data->connections, sizeof(IntraConnection), data->num_connections, f) != data->num_connections) {
                bion_unload_host(data); fclose(f);
                return BION_ERR_FILE_READ;
            }
        }
    }
    if (data->num_synapses > 0) {
        if (data->header.version <= 6) {
            // v6 Synapse: 824 байта (без RRP полей). Читаем побайтово, расширяем.
            struct SynapseV6 { uint8_t raw[824]; };
            std::vector<SynapseV6> old_syns(data->num_synapses);
            if (fread(old_syns.data(), 824, data->num_synapses, f) != data->num_synapses) {
                bion_unload_host(data); fclose(f);
                return BION_ERR_FILE_READ;
            }
            for (uint32_t i = 0; i < data->num_synapses; i++) {
                memcpy(&data->synapses[i], &old_syns[i], 824);
                data->synapses[i].RRP = 10.0f;
                data->synapses[i].RRP_max = 10.0f;
                data->synapses[i].RRP_recovery = 0.02f;
            }
        } else {
            // v7-v9: 836 байт (NT=14 + RRP) → remap; v10: file_sz_synapse (836/840/844)
            if (bion_read_synapses(f, data->synapses, data->num_synapses,
                                   data->header.version, data->header.file_sz_synapse)
                    != data->num_synapses) {
                bion_unload_host(data); fclose(f);
                return BION_ERR_FILE_READ;
            }
        }
    }

    // Ядра (v5+): загружаем или создаём по умолчанию
    struct NucleusV5 {  // v5 формат: 20 генов, 204 байта
        uint32_t neuron_id;
        uint8_t  enabled, consolidation_mode, _pad0[2];
        uint32_t consolidation_interval_ticks, last_consolidation_tick;
        float    gene_activity[20], protein_level[20];
        float    protein_synthesis_rate, pcreb_level, erk_level, nfkb_level;
        float    mtorc1_level, amino_acid_level;
        uint8_t  _reservedV5[4];
    };

    if (data->header.version >= 5 && data->header.num_nuclei > 0) {
        data->num_nuclei = data->header.num_nuclei;
        data->nuclei = new (std::nothrow) Nucleus[data->num_nuclei];
        if (data->nuclei) {
            if (data->header.version == 5) {
                // v5→v6 миграция: читаем 204-байтные v5 ядра, расширяем до 332 байт
                NucleusV5 nv5;
                for (uint32_t i = 0; i < data->num_nuclei; i++) {
                    if (fread(&nv5, sizeof(NucleusV5), 1, f) != 1) {
                        bion_unload_host(data); fclose(f);
                        return BION_ERR_FILE_READ;
                    }
                    memset(&data->nuclei[i], 0, sizeof(Nucleus));
                    data->nuclei[i].neuron_id = nv5.neuron_id;
                    data->nuclei[i].enabled = nv5.enabled;
                    data->nuclei[i].consolidation_mode = nv5.consolidation_mode;
                    data->nuclei[i].consolidation_interval_ticks = nv5.consolidation_interval_ticks;
                    data->nuclei[i].last_consolidation_tick = nv5.last_consolidation_tick;
                    for (int g = 0; g < 20; g++) {
                        data->nuclei[i].gene_activity[g] = nv5.gene_activity[g];
                        data->nuclei[i].protein_level[g]  = nv5.protein_level[g];
                    }
                    data->nuclei[i].protein_synthesis_rate = nv5.protein_synthesis_rate;
                    data->nuclei[i].pcreb_level = nv5.pcreb_level;
                    data->nuclei[i].erk_level = nv5.erk_level;
                    data->nuclei[i].nfkb_level = nv5.nfkb_level;
                    data->nuclei[i].mtorc1_level = nv5.mtorc1_level;
                    data->nuclei[i].amino_acid_level = nv5.amino_acid_level;
                }
            } else if (data->header.version == 6) {
                // v6→v7 миграция: 332-байтные ядра (36 генов) → 412 байт (46 генов)
                struct NucleusV6 { uint8_t raw[332]; };
                std::vector<NucleusV6> old_nuc(data->num_nuclei);
                if (fread(old_nuc.data(), 332, data->num_nuclei, f) != data->num_nuclei) {
                    bion_unload_host(data); fclose(f);
                    return BION_ERR_FILE_READ;
                }
                for (uint32_t i = 0; i < data->num_nuclei; i++) {
                    memcpy(&data->nuclei[i], &old_nuc[i], 332);
                    // zero новые гены (36..45)
                    for (int g = 36; g < 46; g++) {
                        data->nuclei[i].gene_activity[g] = 0.0f;
                        data->nuclei[i].protein_level[g]  = 0.0f;
                    }
                }
            } else {
                // v7-v14: file_sz_nucleus==412 (старый GENE_COUNT=46) → миграция на 57 генов
                if (data->header.file_sz_nucleus == BION_NUCLEUS_SIZE_V46) {
                    NucleusV46 nv46;
                    for (uint32_t i = 0; i < data->num_nuclei; i++) {
                        if (fread(&nv46, sizeof(NucleusV46), 1, f) != 1) {
                            bion_unload_host(data); fclose(f);
                            return BION_ERR_FILE_READ;
                        }
                        memset(&data->nuclei[i], 0, sizeof(Nucleus));
                        bion_migrate_nucleus_v46(&nv46, &data->nuclei[i]);
                    }
                } else {
                    if (fread(data->nuclei, sizeof(Nucleus), data->num_nuclei, f) != data->num_nuclei) {
                        bion_unload_host(data); fclose(f);
                        return BION_ERR_FILE_READ;
                    }
                }
            }
        }
    } else {
        // v3/v4 → v6: создаём ядра по умолчанию
        data->num_nuclei = data->num_neurons;
        data->nuclei = new (std::nothrow) Nucleus[data->num_nuclei];
        if (data->nuclei) {
            for (uint32_t i = 0; i < data->num_nuclei; i++) {
                memset(&data->nuclei[i], 0, sizeof(Nucleus));
                data->nuclei[i].neuron_id = data->neurons[i].id;
                data->nuclei[i].enabled = 1;
                data->nuclei[i].consolidation_mode = 3;
                data->nuclei[i].consolidation_interval_ticks = 0;
                data->nuclei[i].amino_acid_level = 0.5f;
            }
        }
    }

    fclose(f);

    // Валидация связей
    for (uint32_t i = 0; i < data->num_connections; i++) {
        if (data->connections[i].segment_a >= data->num_segments ||
            data->connections[i].segment_b >= data->num_segments) {
            DLOG_ERROR("BION:IntraConnection[%u] out-of-range segment", i);
            bion_unload_host(data);
            return BION_ERR_BAD_CONN;
        }
    }

    // Осевой ток — Нернст-Планк (электродиффузия ионов), сопротивление больше НЕ хранится
    // и НЕ пересчитывается. is_intracellular — флаг (1 = внутриклеточная связь), не резистор.
    // См. notes/modifier_refs/ELECTRODIFFUSION.md.

    for (uint32_t i = 0; i < data->num_synapses; i++) {
        if (data->synapses[i].pre_segment >= data->num_segments ||
            data->synapses[i].post_segment >= data->num_segments) {
            DLOG_ERROR("BION:Synapse[%u] out-of-range segment", i);
            bion_unload_host(data);
            return BION_ERR_BAD_SYNAPSE;
        }
        if (data->synapses[i].pre_autoreceptor_count > MAX_PRE_AUTORECEPTORS ||
            data->synapses[i].post_receptor_count > MAX_POST_RECEPTORS) {
            DLOG_ERROR("BION:Synapse[%u] too many receptors", i);
            bion_unload_host(data);
            return BION_ERR_BAD_MODCOUNT;
        }
        if (data->synapses[i].type >= SYNAPSE_TYPE_COUNT) {
            DLOG_ERROR("BION:Synapse[%u] invalid type %u", i, data->synapses[i].type);
            bion_unload_host(data);
            return BION_ERR_BAD_SYNAPSE;
        }
    }

    for (uint32_t i = 0; i < data->num_neurons; i++) {
        uint32_t end = data->neurons[i].segment_start + data->neurons[i].segment_count;
        if (end > data->num_segments) {
            DLOG_ERROR("BION:Neuron[%u] segment range exceeds num_segments", i);
            bion_unload_host(data);
            return BION_ERR_BAD_NEURON;
        }
    }

    for (uint32_t i = 0; i < data->num_segments; i++) {
        if (data->segments[i].modifier_count > MAX_MODIFIERS_PER_SEGMENT) {
            DLOG_ERROR("BION:Segment[%u] too many modifiers", i);
            bion_unload_host(data);
            return BION_ERR_BAD_MODCOUNT;
        }
    }

    // Заполнение intra_connection_ids[]
    for (uint32_t i = 0; i < data->num_segments; i++) {
        data->segments[i].intra_connection_count = 0;
    }
    for (uint32_t i = 0; i < data->num_connections; i++) {
        uint32_t a = data->connections[i].segment_a;
        uint32_t b = data->connections[i].segment_b;
        if (data->segments[a].intra_connection_count < MAX_INTRACONNECTIONS_PER_SEGMENT) {
            data->segments[a].intra_connection_ids[
                data->segments[a].intra_connection_count++] = i;
        }
        if (data->segments[b].intra_connection_count < MAX_INTRACONNECTIONS_PER_SEGMENT) {
            data->segments[b].intra_connection_ids[
                data->segments[b].intra_connection_count++] = i;
        }
    }

    return 0;
}

void bion_unload_host(BionData* data) {
    delete[] data->neurons;
    delete[] data->segments;
    delete[] data->connections;
    delete[] data->synapses;
    delete[] data->nuclei;
    data->neurons = nullptr;
    data->segments = nullptr;
    data->connections = nullptr;
    data->synapses = nullptr;
    data->nuclei = nullptr;
    data->num_neurons = 0;
    data->num_segments = 0;
    data->num_connections = 0;
    data->num_synapses = 0;
    data->num_nuclei = 0;
    memset(&data->header, 0, sizeof(NetworkHeader));
}

int bion_save(const char* filename, const BionData* data) {
    FILE* f = fopen(filename, "wb");
    if (!f) return BION_ERR_FILE_OPEN;

    NetworkHeader hdr = data->header;
    hdr.magic      = BION_MAGIC;
    hdr.version    = BION_VERSION;
    hdr.mode       = 0;
    hdr.diffusion_type = 0;
    hdr.num_neurons     = data->num_neurons;
    hdr.num_segments    = data->num_segments;
    hdr.num_intra_connections = data->num_connections;
    hdr.num_synapses    = data->num_synapses;
    hdr.grid_dim_x = 0;
    hdr.grid_dim_y = 0;
    hdr.grid_dim_z = 0;
    hdr.voxel_size = 1.0f;
    hdr.timestamp  = (uint64_t)time(nullptr);
    strncpy(hdr.build_id, "v5.0.0", sizeof(hdr.build_id) - 1);
    hdr.num_nuclei = data->num_nuclei;
    hdr.nucleus_version = 1;
    // v10: size-based self-describing fields
    hdr.file_sz_segment     = sizeof(Segment);
    hdr.file_sz_synapse     = sizeof(Synapse);
    hdr.file_sz_nucleus     = sizeof(Nucleus);
    hdr.file_sz_intraconnection = sizeof(IntraConnection);
    hdr.file_sz_neuron      = sizeof(Neuron);
    hdr.file_sz_modifier    = sizeof(Modifier);

    if (fwrite(&hdr, sizeof(NetworkHeader), 1, f) != 1) goto fail;
    if (data->num_neurons > 0) {
        if (fwrite(data->neurons, sizeof(Neuron),
                   data->num_neurons, f) != data->num_neurons) goto fail;
    }
    if (fwrite(data->segments, sizeof(Segment),
               data->num_segments, f) != data->num_segments) goto fail;
    if (data->num_connections > 0) {
        if (fwrite(data->connections, sizeof(IntraConnection),
                   data->num_connections, f) != data->num_connections) goto fail;
    }
    if (data->num_synapses > 0) {
        if (fwrite(data->synapses, sizeof(Synapse),
                   data->num_synapses, f) != data->num_synapses) goto fail;
    }
    if (data->num_nuclei > 0 && data->nuclei) {
        if (fwrite(data->nuclei, sizeof(Nucleus),
                   data->num_nuclei, f) != data->num_nuclei) goto fail;
    }

    fclose(f);
    return 0;

fail:
    fclose(f);
    return BION_ERR_WRITE_FAIL;
}
