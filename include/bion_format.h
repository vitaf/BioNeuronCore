// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#pragma once
#include <cstddef>
#include <cstdio>
#include <cstring>
#include "types.h"

// Magic-число .bion файла (BION в ASCII = 0x42494F4E)
#define BION_MAGIC 0x42494F4E

// Текущая версия формата
// v8: MAX_MODIFIERS_PER_SEGMENT 16→32, Segment 988→1692
// v9: IntraConnection 12→16 bytes (+_pad, fixes heap corruption on XMM load)
// v10: size-based self-describing header (+file_sz_* fields, +ER_Ca2 in Segment 1692→1696)
// v12: ионы float→double (intra_ions/extra_ions), Segment 1728→1800 (+align8 pad +tail8: sizeof кратен 8)
// v13: Modifier density/state float→double (44→88 байт), Segment 1800→3208
// v14: Synapse выравнивание post_receptors на 8 (reserved2/reserved4 +4 байта, 1560→1568) — CUDA err=716 (misaligned double)
#define BION_VERSION 14

// Поддерживаемые версии (для обратной совместимости)
#define BION_VERSION_MIN 3
#define BION_VERSION_MAX 14

// Размеры заголовка для staged reading
#define BION_HDR_SIZE_V3    68   // v3-v4 base (до num_nuclei)
#define BION_HDR_SIZE_V5    84   // v5-v9: +num_nuclei, nucleus_version, _reserved_hdr[2]
#define BION_HDR_SIZE_V10   sizeof(NetworkHeader)  // v10+: +file_sz_* fields

// Универсальное чтение с size-based адаптацией (v10+)
// file_sz=0 → fallback на version-based логику
static inline size_t bion_read_sized(FILE* f, void* dst, size_t elem_count,
                                      uint32_t file_sz, uint32_t code_sz) {
    if (file_sz == 0 || file_sz == code_sz) {
        return fread(dst, code_sz, elem_count, f);  // fast path / v<10 fallback
    }
    uint32_t copy_sz = (file_sz < code_sz) ? file_sz : code_sz;
    uint8_t* buf = new uint8_t[file_sz];
    size_t read = 0;
    for (size_t i = 0; i < elem_count; i++) {
        if (fread(buf, file_sz, 1, f) != 1) break;
        memset((uint8_t*)dst + i * code_sz, 0, code_sz);
        memcpy((uint8_t*)dst + i * code_sz, buf, copy_sz);
        read++;
    }
    delete[] buf;
    return read;
}

// Synapse история: 824 (v6, без RRP) → 836 (v7-v9: +RRP, NT_COUNT=14) →
// 840 (v10 до bouton_Ca: NT_COUNT=15) → 844 (+bouton_Ca) → 848 (текущий: +spine_Ca в конце).
// cleft_neurotransmitter[NT_COUNT] 14→15 сдвигает поля в СЕРЕДИНЕ → нужен remap.
#define BION_SYNAPSE_SIZE_V7   836  // v7-v9: NT_COUNT=14 + RRP, без bouton_Ca
#define BION_SYNAPSE_SIZE_V10  840  // v10 до bouton_Ca: NT_COUNT=15 + RRP
#define BION_SYNAPSE_SIZE_V13  1560 // v13: Modifier double (88) — post_receptors НЕ выровнен на 8

// ── Modifier v12 (44 байта, float density/state) — заморожен для миграции v3-v12 → v13 ──
// v13 переводит density/state на double (88 байт) → нужен per-modifier remap.
#pragma pack(push, 1)
typedef struct {
    uint8_t  type;
    uint8_t  _pad[3];
    float    density;
    float    state[MAX_STATE_VARS];
    uint8_t  active_vars;
    uint8_t  flags;
    uint8_t  _reserved[2];
} ModifierV12;
#pragma pack(pop)
static_assert(sizeof(ModifierV12) == 44, "ModifierV12 must be 44 bytes");

static inline void bion_migrate_modifier_v12(const ModifierV12* old, Modifier* s) {
    s->type = old->type;
    s->density = (double)old->density;
    for (int j = 0; j < MAX_STATE_VARS; j++) s->state[j] = (double)old->state[j];
    s->active_vars = old->active_vars;
    s->flags = old->flags;
}

#pragma pack(push, 1)
struct SynapseV7 {
    uint32_t id;
    uint32_t pre_segment;
    uint32_t post_segment;
    uint8_t  type;
    uint8_t  reserved[3];
    float    weight;
    float    delay;
    float    pre_area;
    float    post_area;
    ModifierV12 pre_autoreceptors[MAX_PRE_AUTORECEPTORS];
    uint8_t  pre_autoreceptor_count;
    uint8_t  reserved2[3];
    ModifierV12 post_receptors[MAX_POST_RECEPTORS];
    uint8_t  post_receptor_count;
    uint8_t  reserved3[3];
    float    release_probability;
    float    release_amount;
    float    cleft_neurotransmitter[14];  // NT_COUNT=14 (без капсаицина)
    float    facilitation;
    float    depression;
    float    tau_clearance;
    uint8_t  refractory;
    uint8_t  reserved4[3];
    float    RRP;
    float    RRP_max;
    float    RRP_recovery;
};
#pragma pack(pop)
static_assert(sizeof(SynapseV7) == BION_SYNAPSE_SIZE_V7, "SynapseV7 must be 836 bytes");

static inline void bion_migrate_synapse_v7(const SynapseV7* old, Synapse* s) {
    memset(s, 0, sizeof(Synapse));
    s->id = old->id;
    s->pre_segment = old->pre_segment;
    s->post_segment = old->post_segment;
    s->type = old->type;
    s->weight = old->weight;
    s->delay = old->delay;
    s->pre_area = old->pre_area;
    s->post_area = old->post_area;
    s->pre_autoreceptor_count = old->pre_autoreceptor_count;
    if (s->pre_autoreceptor_count > MAX_PRE_AUTORECEPTORS)
        s->pre_autoreceptor_count = MAX_PRE_AUTORECEPTORS;
    for (int j = 0; j < s->pre_autoreceptor_count; j++)
        bion_migrate_modifier_v12(&old->pre_autoreceptors[j], &s->pre_autoreceptors[j]);
    s->post_receptor_count = old->post_receptor_count;
    if (s->post_receptor_count > MAX_POST_RECEPTORS)
        s->post_receptor_count = MAX_POST_RECEPTORS;
    for (int j = 0; j < s->post_receptor_count; j++)
        bion_migrate_modifier_v12(&old->post_receptors[j], &s->post_receptors[j]);
    s->release_probability = old->release_probability;
    s->release_amount = old->release_amount;
    for (int j = 0; j < 14; j++)  // NT_COUNT 14→15: [14] остаётся 0
        s->cleft_neurotransmitter[j] = old->cleft_neurotransmitter[j];
    s->facilitation = old->facilitation;
    s->depression = old->depression;
    s->tau_clearance = old->tau_clearance;
    s->refractory = old->refractory;
    s->RRP = old->RRP;
    s->RRP_max = old->RRP_max;
    s->RRP_recovery = old->RRP_recovery;
    // bouton_Ca = 0 (поля не было)
}

// ── Synapse v13 (1560 байт, Modifier double 88) — заморожен для миграции v13 → v14 ──
// v14 выравнивает post_receptors на 8 (reserved2/reserved4 +4 байта) → post_receptors
// сдвигается (740→744) → нужен per-field remap (не byte-copy).
#pragma pack(push, 1)
struct SynapseV13 {
    uint32_t id;
    uint32_t pre_segment;
    uint32_t post_segment;
    uint8_t  type;
    uint8_t  reserved[3];
    float    weight;
    float    delay;
    float    pre_area;
    float    post_area;
    Modifier pre_autoreceptors[MAX_PRE_AUTORECEPTORS];
    uint8_t  pre_autoreceptor_count;
    uint8_t  reserved2[3];
    Modifier post_receptors[MAX_POST_RECEPTORS];
    uint8_t  post_receptor_count;
    uint8_t  reserved3[3];
    float    release_probability;
    float    release_amount;
    float    cleft_neurotransmitter[NT_COUNT];
    float    facilitation;
    float    depression;
    float    tau_clearance;
    uint8_t  refractory;
    uint8_t  reserved4[3];
    float    RRP;
    float    RRP_max;
    float    RRP_recovery;
    float    bouton_Ca;
    float    spine_Ca;
    float    spine_camkii;
    float    spine_calcineurin;
};
#pragma pack(pop)
static_assert(sizeof(SynapseV13) == BION_SYNAPSE_SIZE_V13, "SynapseV13 must be 1560 bytes");

static inline void bion_migrate_synapse_v13(const SynapseV13* old, Synapse* s) {
    memset(s, 0, sizeof(Synapse));
    s->id = old->id;
    s->pre_segment = old->pre_segment;
    s->post_segment = old->post_segment;
    s->type = old->type;
    s->weight = old->weight;
    s->delay = old->delay;
    s->pre_area = old->pre_area;
    s->post_area = old->post_area;
    s->pre_autoreceptor_count = old->pre_autoreceptor_count;
    if (s->pre_autoreceptor_count > MAX_PRE_AUTORECEPTORS)
        s->pre_autoreceptor_count = MAX_PRE_AUTORECEPTORS;
    for (int j = 0; j < s->pre_autoreceptor_count; j++)
        s->pre_autoreceptors[j] = old->pre_autoreceptors[j];
    s->post_receptor_count = old->post_receptor_count;
    if (s->post_receptor_count > MAX_POST_RECEPTORS)
        s->post_receptor_count = MAX_POST_RECEPTORS;
    for (int j = 0; j < s->post_receptor_count; j++)
        s->post_receptors[j] = old->post_receptors[j];
    s->release_probability = old->release_probability;
    s->release_amount = old->release_amount;
    for (int j = 0; j < NT_COUNT; j++)
        s->cleft_neurotransmitter[j] = old->cleft_neurotransmitter[j];
    s->facilitation = old->facilitation;
    s->depression = old->depression;
    s->tau_clearance = old->tau_clearance;
    s->refractory = old->refractory;
    s->RRP = old->RRP;
    s->RRP_max = old->RRP_max;
    s->RRP_recovery = old->RRP_recovery;
    s->bouton_Ca = old->bouton_Ca;
    s->spine_Ca = old->spine_Ca;
    s->spine_camkii = old->spine_camkii;
    s->spine_calcineurin = old->spine_calcineurin;
}

// Читает count синапсов для v7+ (v6 обрабатывается отдельно в лоадере).
// Возвращает число фактически прочитанных элементов.
static inline size_t bion_read_synapses(FILE* f, Synapse* dst, uint32_t count,
                                         uint32_t version, uint32_t file_sz_synapse) {
    if (version >= 10) {
        if (file_sz_synapse == 0 || file_sz_synapse == sizeof(Synapse))
            return fread(dst, sizeof(Synapse), count, f);
        if (file_sz_synapse == BION_SYNAPSE_SIZE_V7) {  // ранний v10: NT=14 + RRP
            size_t n = 0;
            for (uint32_t i = 0; i < count; i++) {
                SynapseV7 old;
                if (fread(&old, sizeof(SynapseV7), 1, f) != 1) break;
                bion_migrate_synapse_v7(&old, &dst[i]);
                n++;
            }
            return n;
        }
        if (file_sz_synapse == BION_SYNAPSE_SIZE_V13) {  // v13: post_receptors не выровнен → remap
            size_t n = 0;
            for (uint32_t i = 0; i < count; i++) {
                SynapseV13 old;
                if (fread(&old, sizeof(SynapseV13), 1, f) != 1) break;
                bion_migrate_synapse_v13(&old, &dst[i]);
                n++;
            }
            return n;
        }
        // 840 → 844: bouton_Ca в конце → copy-first + zero-tail
        return bion_read_sized(f, dst, count, file_sz_synapse, sizeof(Synapse));
    }
    // v7-v9: 836 байт (NT_COUNT=14 + RRP) → полный remap
    size_t n = 0;
    for (uint32_t i = 0; i < count; i++) {
        SynapseV7 old;
        if (fread(&old, sizeof(SynapseV7), 1, f) != 1) break;
        bion_migrate_synapse_v7(&old, &dst[i]);
        n++;
    }
    return n;
}

// ── Миграция Segment v7-v9 (и раннего v10): NT_COUNT 14→15 + ER_Ca2 ─────────
// v7-v9: 1692 байта (NT_COUNT=14, без ER_Ca2). Ранний v10 (da4ace8): 1696 (NT=14 + ER_Ca2).
// Текущий: 1704 (NT_COUNT=15 + ER_Ca2). NT_COUNT сдвигает modifiers+хвост в СЕРЕДИНЕ
// структуры, поэтому нужен полный remap (в отличие от Synapse, где bouton_Ca в конце).
#define BION_SEGMENT_SIZE_V7      1692  // v7-v9: NT_COUNT=14, без ER_Ca2
#define BION_SEGMENT_SIZE_V10_ER  1696  // ранний v10: NT_COUNT=14, с ER_Ca2

#pragma pack(push, 1)
struct SegmentV7 {
    uint32_t id; uint32_t neuron_id;
    Vec3 position; Vec3 direction;
    float length; float radius; float V_m;
    float intra_ions[5]; float extra_ions[5];  // v7-v9: ION_COUNT=5 (хардкод, не текущий)
    float intra_nt[14]; float neurotransmitter[14];
    ModifierV12 modifiers[MAX_MODIFIERS_PER_SEGMENT];
    uint8_t modifier_count; uint8_t segment_type; uint8_t reserved1[2];
    float ATP_level; float temperature; float I_buffer; float transcription_signal;
    uint32_t intra_connection_ids[MAX_INTRACONNECTIONS_PER_SEGMENT];
    uint8_t intra_connection_count; uint8_t reserved2[3];
};
#pragma pack(pop)
static_assert(sizeof(SegmentV7) == BION_SEGMENT_SIZE_V7, "SegmentV7 must be 1692 bytes");

// v10: 1704 байта (ION_COUNT=5, NT_COUNT=15, +ER_Ca2) — до добавления A⁻ (ION_MAX=8)
#define BION_SEGMENT_SIZE_V10   1704
#define BION_SEGMENT_SIZE_V13   3208  // v13: Modifier double (88) — I_buffer НЕ выровнен на 8

#pragma pack(push, 1)
struct SegmentV10 {
    uint32_t id; uint32_t neuron_id;
    Vec3 position; Vec3 direction;
    float length; float radius; float V_m;
    float intra_ions[5]; float extra_ions[5];  // v10: ION_COUNT=5 (без A⁻)
    float intra_nt[15]; float neurotransmitter[15];
    ModifierV12 modifiers[MAX_MODIFIERS_PER_SEGMENT];
    uint8_t modifier_count; uint8_t segment_type; uint8_t reserved1[2];
    float ATP_level; float temperature; float I_buffer; float transcription_signal;
    uint32_t intra_connection_ids[MAX_INTRACONNECTIONS_PER_SEGMENT];
    uint8_t intra_connection_count; uint8_t reserved2[3];
    float ER_Ca2;
};
#pragma pack(pop)
static_assert(sizeof(SegmentV10) == BION_SEGMENT_SIZE_V10, "SegmentV10 must be 1704 bytes");

// Миграция v10 (5 ионов) → v11 (ION_MAX=8): A⁻ = 0 (init_ions задаст 151 при сборке сети).
static inline void bion_migrate_segment_v10(const SegmentV10* old, Segment* s) {
    memset(s, 0, sizeof(Segment));
    s->id = old->id;
    s->neuron_id = old->neuron_id;
    s->position = old->position;
    s->direction = old->direction;
    s->length = old->length;
    s->radius = old->radius;
    s->V_m = old->V_m;
    for (int j = 0; j < 5; j++) {  // 5 ионов; [5]=A⁻ остаётся 0
        s->intra_ions[j] = old->intra_ions[j];
        s->extra_ions[j] = old->extra_ions[j];
    }
    for (int j = 0; j < 15; j++) {
        s->intra_nt[j] = old->intra_nt[j];
        s->neurotransmitter[j] = old->neurotransmitter[j];
    }
    s->modifier_count = old->modifier_count;
    if (s->modifier_count > MAX_MODIFIERS_PER_SEGMENT) s->modifier_count = MAX_MODIFIERS_PER_SEGMENT;
    for (int j = 0; j < s->modifier_count; j++) bion_migrate_modifier_v12(&old->modifiers[j], &s->modifiers[j]);
    s->segment_type = old->segment_type;
    s->ATP_level = old->ATP_level;
    s->temperature = old->temperature;
    s->I_buffer = old->I_buffer;
    s->transcription_signal = old->transcription_signal;
    for (int j = 0; j < MAX_INTRACONNECTIONS_PER_SEGMENT; j++)
        s->intra_connection_ids[j] = old->intra_connection_ids[j];
    s->intra_connection_count = old->intra_connection_count;
    s->ER_Ca2 = old->ER_Ca2;
}

static inline int bion_read_segment_v10(FILE* f, Segment* s) {
    SegmentV10 old;
    if (fread(&old, sizeof(SegmentV10), 1, f) != 1) return 0;
    bion_migrate_segment_v10(&old, s);
    return 1;
}

// ── Segment v13 (3208 байт, Modifier double 88) — заморожен для миграции v13 → v14 ──
// v14 выравнивает I_buffer на 8 (reserved1/reserved2 +4 байта) → I_buffer сдвигается
// (3124→3128) → нужен per-field remap (не byte-copy).
#pragma pack(push, 1)
struct SegmentV13 {
    uint32_t id; uint32_t neuron_id;
    Vec3 position; Vec3 direction;
    float length; float radius;
    double V_m;
    double intra_ions[ION_MAX];
    double extra_ions[ION_MAX];
    float intra_nt[NT_COUNT];
    float neurotransmitter[NT_COUNT];
    Modifier modifiers[MAX_MODIFIERS_PER_SEGMENT];
    uint8_t modifier_count; uint8_t segment_type; uint8_t reserved1[2];
    float ATP_level; float temperature;
    double I_buffer;
    float transcription_signal;
    uint32_t intra_connection_ids[MAX_INTRACONNECTIONS_PER_SEGMENT];
    uint8_t intra_connection_count; uint8_t reserved2[3];
    float ER_Ca2;
};
#pragma pack(pop)
static_assert(sizeof(SegmentV13) == BION_SEGMENT_SIZE_V13, "SegmentV13 must be 3208 bytes");

static inline void bion_migrate_segment_v13(const SegmentV13* old, Segment* s) {
    memset(s, 0, sizeof(Segment));
    s->id = old->id;
    s->neuron_id = old->neuron_id;
    s->position = old->position;
    s->direction = old->direction;
    s->length = old->length;
    s->radius = old->radius;
    s->V_m = old->V_m;
    for (int j = 0; j < ION_MAX; j++) {
        s->intra_ions[j] = old->intra_ions[j];
        s->extra_ions[j] = old->extra_ions[j];
    }
    for (int j = 0; j < NT_COUNT; j++) {
        s->intra_nt[j] = old->intra_nt[j];
        s->neurotransmitter[j] = old->neurotransmitter[j];
    }
    s->modifier_count = old->modifier_count;
    if (s->modifier_count > MAX_MODIFIERS_PER_SEGMENT) s->modifier_count = MAX_MODIFIERS_PER_SEGMENT;
    for (int j = 0; j < s->modifier_count; j++) s->modifiers[j] = old->modifiers[j];
    s->segment_type = old->segment_type;
    s->ATP_level = old->ATP_level;
    s->temperature = old->temperature;
    s->I_buffer = old->I_buffer;
    s->transcription_signal = old->transcription_signal;
    for (int j = 0; j < MAX_INTRACONNECTIONS_PER_SEGMENT; j++)
        s->intra_connection_ids[j] = old->intra_connection_ids[j];
    s->intra_connection_count = old->intra_connection_count;
    s->ER_Ca2 = old->ER_Ca2;
}

static inline int bion_read_segment_v13(FILE* f, Segment* s) {
    SegmentV13 old;
    if (fread(&old, sizeof(SegmentV13), 1, f) != 1) return 0;
    bion_migrate_segment_v13(&old, s);
    return 1;
}

// Полный remap старого сегмента (NT_COUNT=14) в текущий (NT_COUNT=15, ER_Ca2).
// er_ca2: значение ER_Ca2 на диске (0, если поле отсутствовало — v7-v9).
static inline void bion_migrate_segment_v7(const SegmentV7* old, Segment* s, float er_ca2) {
    memset(s, 0, sizeof(Segment));
    s->id = old->id;
    s->neuron_id = old->neuron_id;
    s->position = old->position;
    s->direction = old->direction;
    s->length = old->length;
    s->radius = old->radius;
    s->V_m = old->V_m;
    for (int j = 0; j < ION_COUNT; j++) {
        s->intra_ions[j] = old->intra_ions[j];
        s->extra_ions[j] = old->extra_ions[j];
    }
    for (int j = 0; j < 14; j++) {  // NT_COUNT 14→15: новый [14] остаётся 0
        s->intra_nt[j] = old->intra_nt[j];
        s->neurotransmitter[j] = old->neurotransmitter[j];
    }
    s->modifier_count = old->modifier_count;
    if (s->modifier_count > MAX_MODIFIERS_PER_SEGMENT)
        s->modifier_count = MAX_MODIFIERS_PER_SEGMENT;
    for (int j = 0; j < s->modifier_count; j++) bion_migrate_modifier_v12(&old->modifiers[j], &s->modifiers[j]);
    s->segment_type = old->segment_type;
    s->ATP_level = old->ATP_level;
    s->temperature = old->temperature;
    s->I_buffer = old->I_buffer;
    s->transcription_signal = old->transcription_signal;
    for (int j = 0; j < MAX_INTRACONNECTIONS_PER_SEGMENT; j++)
        s->intra_connection_ids[j] = old->intra_connection_ids[j];
    s->intra_connection_count = old->intra_connection_count;
    s->ER_Ca2 = er_ca2;
}

// Читает один старый сегмент (file_sz = 1692 или 1696) и мигрирует. Возвращает 1 при успехе.
static inline int bion_read_segment_v7(FILE* f, Segment* s, uint32_t file_sz) {
    SegmentV7 old;
    if (fread(&old, sizeof(SegmentV7), 1, f) != 1) return 0;
    float er_ca2 = 0.0f;
    if (file_sz >= sizeof(SegmentV7) + sizeof(float)) {
        if (fread(&er_ca2, sizeof(float), 1, f) != 1) return 0;
    }
    bion_migrate_segment_v7(&old, s, er_ca2);
    return 1;
}

// Читает count сегментов для v7+ (v3-v6 обрабатываются отдельно в лоадере).
// Возвращает число фактически прочитанных элементов.
static inline size_t bion_read_segments(FILE* f, Segment* dst, uint32_t count,
                                         uint32_t version, uint32_t file_sz_segment) {
    if (version >= 10) {
        if (file_sz_segment == 0 || file_sz_segment == sizeof(Segment))
            return fread(dst, sizeof(Segment), count, f);
        if (file_sz_segment == BION_SEGMENT_SIZE_V7 || file_sz_segment == BION_SEGMENT_SIZE_V10_ER) {
            size_t n = 0;
            for (uint32_t i = 0; i < count; i++)
                if (bion_read_segment_v7(f, &dst[i], file_sz_segment)) n++;
            return n;
        }
        if (file_sz_segment == BION_SEGMENT_SIZE_V10) {
            size_t n = 0;
            for (uint32_t i = 0; i < count; i++)
                if (bion_read_segment_v10(f, &dst[i])) n++;
            return n;
        }
        if (file_sz_segment == BION_SEGMENT_SIZE_V13) {  // v13: I_buffer не выровнен → remap
            size_t n = 0;
            for (uint32_t i = 0; i < count; i++)
                if (bion_read_segment_v13(f, &dst[i])) n++;
            return n;
        }
        return bion_read_sized(f, dst, count, file_sz_segment, sizeof(Segment));
    }
    // v7-v9: Segment 1692 байта (NT_COUNT=14, без ER_Ca2) → полный remap
    size_t n = 0;
    for (uint32_t i = 0; i < count; i++)
        if (bion_read_segment_v7(f, &dst[i], BION_SEGMENT_SIZE_V7)) n++;
    return n;
}

// ═══════════════════════════════════════════════════════════════════════════════
// NucleusV46 — замороженный формат ядра v14 (GENE_COUNT=46, 412 байт) для миграции
// старых .bion при переходе на GENE_COUNT=57 (500 байт). Детекция по file_sz_nucleus.
// ═══════════════════════════════════════════════════════════════════════════════
struct NucleusV46 {
    uint32_t neuron_id;
    uint8_t  enabled;
    uint8_t  consolidation_mode;
    uint8_t  _pad0[2];
    uint32_t consolidation_interval_ticks;
    uint32_t last_consolidation_tick;
    float    gene_activity[46];
    float    protein_level[46];
    float    protein_synthesis_rate;
    float    pcreb_level;
    float    erk_level;
    float    nfkb_level;
    float    mtorc1_level;
    float    amino_acid_level;
    uint8_t  _reserved[4];
};
#define BION_NUCLEUS_SIZE_V46 412
static_assert(sizeof(NucleusV46) == 412, "NucleusV46 must be 412 bytes (GENE_COUNT=46)");

static inline void bion_migrate_nucleus_v46(const NucleusV46* old, Nucleus* n) {
    n->neuron_id = old->neuron_id;
    n->enabled = old->enabled;
    n->consolidation_mode = old->consolidation_mode;
    n->consolidation_interval_ticks = old->consolidation_interval_ticks;
    n->last_consolidation_tick = old->last_consolidation_tick;
    for (int g = 0; g < 46; g++) {
        n->gene_activity[g] = old->gene_activity[g];
        n->protein_level[g]  = old->protein_level[g];
    }
    for (int g = 46; g < GENE_COUNT; g++) {  // новые гены 46..56 → 0
        n->gene_activity[g] = 0.0f;
        n->protein_level[g]  = 0.0f;
    }
    n->protein_synthesis_rate = old->protein_synthesis_rate;
    n->pcreb_level = old->pcreb_level;
    n->erk_level = old->erk_level;
    n->nfkb_level = old->nfkb_level;
    n->mtorc1_level = old->mtorc1_level;
    n->amino_acid_level = old->amino_acid_level;
}

// Вычисление полного размера .bion файла по количеству элементов (v5: +nuclei)
static inline size_t bion_calc_size(uint32_t n_neurons, uint32_t n_segments,
                                     uint32_t n_connections, uint32_t n_synapses,
                                     uint32_t n_nuclei) {
    return sizeof(NetworkHeader)
           + n_neurons * sizeof(Neuron)
           + n_segments * sizeof(Segment)
           + n_connections * sizeof(IntraConnection)
           + n_synapses * sizeof(Synapse)
           + n_nuclei * sizeof(Nucleus);
}
static inline size_t bion_calc_size_v4(uint32_t n_neurons, uint32_t n_segments,
                                        uint32_t n_connections, uint32_t n_synapses) {
    return bion_calc_size(n_neurons, n_segments, n_connections, n_synapses, n_neurons);
}

// Проверка заголовка: magic, версия, наличие сегментов.
// Возвращает 0 при успехе, отрицательный код при ошибке.
static inline int bion_validate_header(const NetworkHeader* hdr) {
    if (!hdr) return -1;
    if (hdr->magic != BION_MAGIC) return -1;
    if (hdr->version < BION_VERSION_MIN || hdr->version > BION_VERSION_MAX) return -2;
    if (hdr->num_segments == 0) return -3;
    return 0;
}

// Строковое представление версии (для диагностики)
static inline const char* bion_version_string(const NetworkHeader* hdr) {
    (void)hdr;
    static_assert(BION_VERSION == 14, "sync this string with BION_VERSION");
    return "14";
}
