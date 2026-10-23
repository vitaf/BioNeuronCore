// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#pragma once
// daemon_protocol.h — wire protocol constants and message framing for Core Daemon <-> Studio IPC.
#include <cstdint>
#include <cstddef>
#include <cstring>

// ── Wire framing: [type:u32][payload_len:u32][payload:byte[payload_len]] ──
// Request types are even; response = request + 1; push (unsolicited) >= 0x8000.
// MSG_FLAG_SESSION: старший бит type = payload начинается с session_id:u32

namespace biocore_proto {

constexpr uint32_t MSG_FLAG_SESSION = 0x80000000u;

inline bool msg_has_session(uint32_t type) { return (type & MSG_FLAG_SESSION) != 0; }
inline uint32_t msg_base_type(uint32_t type) { return type & ~MSG_FLAG_SESSION; }

// ==================== CATEGORY: CONTROL (0x0000-0x00FF) ====================
constexpr uint32_t MSG_PING                 = 0x0000;
constexpr uint32_t MSG_PONG                 = 0x0001;
constexpr uint32_t MSG_GET_VERSION          = 0x0002;
constexpr uint32_t MSG_VERSION_INFO         = 0x0003;
constexpr uint32_t MSG_SHUTDOWN             = 0x0004;
constexpr uint32_t MSG_SHUTDOWN_ACK         = 0x0005;

// ==================== CATEGORY: NETWORK I/O (0x0100-0x01FF) ====================
constexpr uint32_t MSG_LOAD_BION            = 0x0100;
constexpr uint32_t MSG_LOAD_BION_RESULT     = 0x0101;
constexpr uint32_t MSG_SAVE_BION            = 0x0102;
constexpr uint32_t MSG_SAVE_BION_RESULT     = 0x0103;
constexpr uint32_t MSG_UNLOAD               = 0x0104;
constexpr uint32_t MSG_UNLOAD_RESULT        = 0x0105;
constexpr uint32_t MSG_GET_NETWORK_INFO     = 0x0106;
constexpr uint32_t MSG_NETWORK_INFO         = 0x0107;
constexpr uint32_t MSG_LOAD_BEXP            = 0x0108;  // загрузить .bexp как рецепт эксперимента
constexpr uint32_t MSG_LOAD_BEXP_RESULT     = 0x0109;  // [ok:u8][bion_path_len:u16][path][probes_json_len:u16][json]

// ==================== CATEGORY: SIMULATION (0x0200-0x02FF) ====================
constexpr uint32_t MSG_START_SIM            = 0x0200;
constexpr uint32_t MSG_START_SIM_RESULT     = 0x0201;
constexpr uint32_t MSG_STOP_SIM             = 0x0202;
constexpr uint32_t MSG_STOP_SIM_RESULT      = 0x0203;
constexpr uint32_t MSG_PAUSE_SIM            = 0x0204;
constexpr uint32_t MSG_PAUSE_SIM_RESULT     = 0x0205;
constexpr uint32_t MSG_STEP_SIM             = 0x0206;
constexpr uint32_t MSG_STEP_SIM_RESULT      = 0x0207;
constexpr uint32_t MSG_SET_SPEED            = 0x0208;
constexpr uint32_t MSG_SET_SPEED_RESULT     = 0x0209;
constexpr uint32_t MSG_SET_BACKEND          = 0x020A;
constexpr uint32_t MSG_SET_BACKEND_RESULT   = 0x020B;
constexpr uint32_t MSG_RESET_SIM            = 0x020C;
constexpr uint32_t MSG_RESET_SIM_RESULT     = 0x020D;

// ==================== CATEGORY: STIMULI (0x0300-0x03FF) ====================
constexpr uint32_t MSG_ADD_CURRENT_STIMULUS = 0x0300;
constexpr uint32_t MSG_ADD_VOLTAGE_CLAMP    = 0x0302;
constexpr uint32_t MSG_ADD_RAMP_STIMULUS    = 0x0304;
constexpr uint32_t MSG_ADD_ION_INJECTION    = 0x0306;
constexpr uint32_t MSG_ADD_LIGAND_INJECTION = 0x0308;
constexpr uint32_t MSG_CLEAR_STIMULI        = 0x030A;

// ==================== CATEGORY: STATE STREAMING (0x0400-0x04FF) ====================
constexpr uint32_t MSG_SUBSCRIBE_STATE      = 0x0400;
constexpr uint32_t MSG_SUBSCRIBE_RESULT     = 0x0401;
constexpr uint32_t MSG_UNSUBSCRIBE_STATE    = 0x0402;
constexpr uint32_t MSG_UNSUBSCRIBE_RESULT   = 0x0403;
constexpr uint32_t MSG_STATE_FRAME          = 0x0480;  // push
constexpr uint32_t MSG_STATE_FRAME_DELTA    = 0x0481;  // push (delta)

// ==================== CATEGORY: PLUGINS (0x0500-0x05FF) ====================
constexpr uint32_t MSG_LOAD_PLUGIN          = 0x0500;
constexpr uint32_t MSG_LOAD_PLUGIN_RESULT   = 0x0501;
constexpr uint32_t MSG_LOAD_PLUGIN_DIR      = 0x0502;
constexpr uint32_t MSG_LOAD_PLUGIN_DIR_RESULT = 0x0503;
constexpr uint32_t MSG_LIST_PLUGINS         = 0x0504;
constexpr uint32_t MSG_LIST_PLUGINS_RESULT  = 0x0505;
constexpr uint32_t MSG_QUERY_MODIFIER_INFO  = 0x050A;
constexpr uint32_t MSG_MODIFIER_INFO_RESULT = 0x050B;

// ==================== CATEGORY: CONSOLIDATION (0x0600-0x06FF) ====================
constexpr uint32_t MSG_SET_CONSOLIDATION_INTERVAL   = 0x0600;
constexpr uint32_t MSG_CONSOLIDATION_INTERVAL_RESULT = 0x0601;
constexpr uint32_t MSG_GET_CONSOLIDATION_STATUS     = 0x0602;
constexpr uint32_t MSG_CONSOLIDATION_STATUS         = 0x0603;
constexpr uint32_t MSG_GET_NUCLEUS_SNAPSHOT         = 0x0604;
constexpr uint32_t MSG_NUCLEUS_SNAPSHOT             = 0x0605;
constexpr uint32_t MSG_RESET_GENE_EXPRESSION        = 0x0606;
constexpr uint32_t MSG_RESET_GENE_EXPRESSION_RESULT = 0x0607;

// ==================== CATEGORY: BACKEND (0x0700-0x07FF) ====================
constexpr uint32_t MSG_GET_BACKEND_INFO     = 0x0700;
constexpr uint32_t MSG_BACKEND_INFO         = 0x0701;
constexpr uint32_t MSG_LIST_DEVICES         = 0x0702;
constexpr uint32_t MSG_LIST_DEVICES_RESULT  = 0x0703;

// ==================== CATEGORY: CHECKPOINT (0x0800-0x08FF) ====================
constexpr uint32_t MSG_SAVE_CHECKPOINT      = 0x0800;
constexpr uint32_t MSG_SAVE_CHECKPOINT_RESULT = 0x0801;
constexpr uint32_t MSG_LOAD_CHECKPOINT      = 0x0802;
constexpr uint32_t MSG_LOAD_CHECKPOINT_RESULT = 0x0803;
constexpr uint32_t MSG_CHECKPOINT_AND_CONTINUE = 0x0804;  // сохранить и продолжить
constexpr uint32_t MSG_CHECKPOINT_AND_CONTINUE_RESULT = 0x0805;

// ==================== CATEGORY: SESSION (0x0900-0x09FF) ====================
constexpr uint32_t MSG_CREATE_SESSION         = 0x0900;
constexpr uint32_t MSG_CREATE_SESSION_RESULT  = 0x0901;  // [ok:u8][session_id:u32][gpu_device_index:i32]
constexpr uint32_t MSG_DESTROY_SESSION        = 0x0902;
constexpr uint32_t MSG_DESTROY_SESSION_RESULT = 0x0903;  // [ok:u8]
constexpr uint32_t MSG_LIST_SESSIONS          = 0x0904;
constexpr uint32_t MSG_LIST_SESSIONS_RESULT   = 0x0905;  // [count:u32][session_info...]
constexpr uint32_t MSG_GET_SESSION_INFO       = 0x0906;
constexpr uint32_t MSG_SESSION_INFO           = 0x0907;
constexpr uint32_t MSG_RUN_TASK               = 0x0908;  // запустить фоновую задачу
constexpr uint32_t MSG_RUN_TASK_RESULT        = 0x0909;  // [ok:u8][bexp_path_len:u16][path:byte[]]
constexpr uint32_t MSG_RUN_TASK_PARAMS        = 0x090A;  // batch-задача с параметрами
constexpr uint32_t MSG_RUN_TASK_PARAMS_RESULT = 0x090B;  // [ok:u8][bexp_path_len:u16][path:byte[]]
constexpr uint32_t MSG_GET_TASK_PROGRESS      = 0x090C;  // запросить прогресс задачи
constexpr uint32_t MSG_TASK_PROGRESS          = 0x090D;  // [tick:u64][total_ticks:u64][percent:u8]

// ==================== ERROR ====================
constexpr uint32_t MSG_ERROR                = 0xFFFF;

// ── Error codes ─────────────────────────────────────────────────────────
constexpr uint32_t ERR_OK                   = 0;
constexpr uint32_t ERR_UNKNOWN_MSG          = 1;
constexpr uint32_t ERR_FILE_NOT_FOUND       = 2;
constexpr uint32_t ERR_LOAD_FAILED          = 3;
constexpr uint32_t ERR_SAVE_FAILED          = 4;
constexpr uint32_t ERR_NO_NETWORK           = 5;
constexpr uint32_t ERR_ALREADY_RUNNING      = 6;
constexpr uint32_t ERR_NOT_RUNNING          = 7;
constexpr uint32_t ERR_INVALID_PARAM        = 8;
constexpr uint32_t ERR_INTERNAL             = 9;
constexpr uint32_t ERR_BION_FORMAT           = 10;
constexpr uint32_t ERR_MAX_SESSIONS          = 11;
constexpr uint32_t ERR_INVALID_SESSION       = 12;
constexpr uint32_t ERR_GPU_DEVICE_UNAVAILABLE = 13;
// ── Message header (8 bytes on wire) ────────────────────────────────────
struct MsgHeader {
    uint32_t type;
    uint32_t payload_len;
};

inline void write_u32(uint8_t*& p, uint32_t v) { memcpy(p, &v, 4); p += 4; }
inline void write_u64(uint8_t*& p, uint64_t v) { memcpy(p, &v, 8); p += 8; }
inline void write_f32(uint8_t*& p, float v)    { memcpy(p, &v, 4); p += 4; }
inline void write_u8 (uint8_t*& p, uint8_t v)  { *p++ = v; }
inline void write_i32(uint8_t*& p, int32_t v)  { memcpy(p, &v, 4); p += 4; }

inline uint32_t read_u32(const uint8_t*& p) { uint32_t v; memcpy(&v, p, 4); p += 4; return v; }
inline uint64_t read_u64(const uint8_t*& p) { uint64_t v; memcpy(&v, p, 8); p += 8; return v; }
inline float    read_f32(const uint8_t*& p) { float v;    memcpy(&v, p, 4); p += 4; return v; }
inline uint8_t  read_u8 (const uint8_t*& p) { return *p++; }
inline int32_t  read_i32(const uint8_t*& p) { int32_t v;  memcpy(&v, p, 4); p += 4; return v; }

inline void write_bytes(uint8_t*& p, const void* src, size_t len) {
    if (len > 0) { memcpy(p, src, len); p += len; }
}
inline void read_bytes(const uint8_t*& p, void* dst, size_t len) {
    if (len > 0) { memcpy(dst, p, len); p += len; }
}

} // namespace biocore_proto
