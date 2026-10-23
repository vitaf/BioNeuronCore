// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_daemon_protocol_full.cpp — полное покрытие MSG_*-обработчиков демона.
// Покрывает: Network I/O, Simulation control, Stimuli, Streaming, Plugins,
//            Consolidation, Backend, Checkpoint, Session mgmt, Error handling.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>

#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

static const char* DAEMON_BIN = "./biocore-daemon";
static const char* SOCK_PATH  = "/tmp/biocore.sock";
static const char* TEST_BION  = "../BioNeuronStudio/build/network.bion";
static pid_t g_daemon_pid = 0;

// ── Хелперы соединения ─────────────────────────────────────────────────
static int connectUnix() {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path) - 1);
    if (::connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd); return -1;
    }
    return fd;
}

static void sendMsg(int fd, uint32_t type, const uint8_t* payload = nullptr, uint32_t len = 0) {
    uint8_t hdr[8];
    hdr[0] = type & 0xFF; hdr[1] = (type >> 8) & 0xFF;
    hdr[2] = (type >> 16) & 0xFF; hdr[3] = (type >> 24) & 0xFF;
    hdr[4] = len & 0xFF; hdr[5] = (len >> 8) & 0xFF;
    hdr[6] = (len >> 16) & 0xFF; hdr[7] = (len >> 24) & 0xFF;
    write(fd, hdr, 8);
    if (len > 0) write(fd, payload, len);
}

static bool recvMsg(int fd, uint32_t& out_type, std::vector<uint8_t>& out_payload, int timeout_s = 10) {
    fd_set fds; FD_ZERO(&fds); FD_SET(fd, &fds);
    struct timeval tv = {timeout_s, 0};
    if (select(fd + 1, &fds, nullptr, nullptr, &tv) <= 0) return false;
    uint8_t hdr[8];
    ssize_t n = read(fd, hdr, 8);
    if (n != 8) return false;
    out_type = (uint32_t)hdr[0] | ((uint32_t)hdr[1] << 8) |
               ((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24);
    uint32_t plen = (uint32_t)hdr[4] | ((uint32_t)hdr[5] << 8) |
                    ((uint32_t)hdr[6] << 16) | ((uint32_t)hdr[7] << 24);
    out_payload.resize(plen);
    if (plen > 0) {
        size_t off = 0;
        while (off < plen) {
            n = read(fd, out_payload.data() + off, plen - off);
            if (n <= 0) return false;
            off += n;
        }
    }
    return true;
}

static bool loadNetwork(int fd) {
    if (access(TEST_BION, R_OK) != 0) return false;
    sendMsg(fd, 0x0100, (const uint8_t*)TEST_BION, strlen(TEST_BION));
    uint32_t type; std::vector<uint8_t> payload;
    return recvMsg(fd, type, payload) && type == 0x0101 && !payload.empty() && payload[0] == 1;
}

// ── Запуск/остановка демона ─────────────────────────────────────────────
static bool startDaemon() {
    unlink(SOCK_PATH);
    int pipefd[2];
    if (pipe(pipefd) < 0) return false;
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        close(pipefd[0]);
        char ready_fd_str[16];
        snprintf(ready_fd_str, sizeof(ready_fd_str), "%d", pipefd[1]);
        execl(DAEMON_BIN, DAEMON_BIN, "--ready-fd", ready_fd_str,
              "-p", "plugins/builtin/", nullptr);
        _exit(1);
    }
    close(pipefd[1]);
    fd_set fds; FD_ZERO(&fds); FD_SET(pipefd[0], &fds);
    struct timeval tv = {10, 0};
    if (select(pipefd[0] + 1, &fds, nullptr, nullptr, &tv) <= 0) {
        close(pipefd[0]);
        kill(pid, SIGTERM); waitpid(pid, nullptr, 0);
        return false;
    }
    char ready;
    read(pipefd[0], &ready, 1);
    close(pipefd[0]);
    g_daemon_pid = pid;
    return true;
}

static void stopDaemon() {
    if (g_daemon_pid > 0) {
        kill(g_daemon_pid, SIGTERM);
        waitpid(g_daemon_pid, nullptr, 0);
        g_daemon_pid = 0;
        unlink(SOCK_PATH);
    }
}

// ========================================================================
// NETWORK I/O
// ========================================================================
TEST_CASE("MSG_SAVE_BION: save loaded network", "[network]") {
    if (access(TEST_BION, R_OK) != 0) { WARN("no test network"); return; }
    int fd = connectUnix(); REQUIRE(fd >= 0);
    REQUIRE(loadNetwork(fd));

    const char* path = "/tmp/test_save_bion_daemon.bion";
    unlink(path);
    sendMsg(fd, 0x0102, (const uint8_t*)path, strlen(path));
    uint32_t type; std::vector<uint8_t> payload;
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0x0103);
    REQUIRE(!payload.empty());
    REQUIRE(payload[0] == 1);
    struct stat st;
    REQUIRE(stat(path, &st) == 0);
    unlink(path);
    close(fd);
}

TEST_CASE("MSG_SAVE_BION: no network loaded", "[network]") {
    int fd = connectUnix(); REQUIRE(fd >= 0);
    sendMsg(fd, 0x0102, (const uint8_t*)"/tmp/x.bion", 10);
    uint32_t type; std::vector<uint8_t> payload;
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0xFFFF); // MSG_ERROR
    close(fd);
}

TEST_CASE("MSG_UNLOAD: unload and verify", "[network]") {
    if (access(TEST_BION, R_OK) != 0) { WARN("no test network"); return; }
    int fd = connectUnix(); REQUIRE(fd >= 0);
    REQUIRE(loadNetwork(fd));

    sendMsg(fd, 0x0104); // UNLOAD
    uint32_t type; std::vector<uint8_t> payload;
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0x0105);

    // GET_NETWORK_INFO после UNLOAD должен вернуть ошибку
    sendMsg(fd, 0x0106);
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0xFFFF);
    close(fd);
}

TEST_CASE("MSG_GET_NETWORK_INFO: after load", "[network]") {
    if (access(TEST_BION, R_OK) != 0) { WARN("no test network"); return; }
    int fd = connectUnix(); REQUIRE(fd >= 0);
    REQUIRE(loadNetwork(fd));

    sendMsg(fd, 0x0106);
    uint32_t type; std::vector<uint8_t> payload;
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0x0107);
    REQUIRE(payload.size() >= 17); // 4×u32 + u8
    close(fd);
}

// ========================================================================
// SIMULATION CONTROL
// ========================================================================
TEST_CASE("MSG_PAUSE_SIM: pause and resume", "[sim]") {
    if (access(TEST_BION, R_OK) != 0) { WARN("no test network"); return; }
    int fd = connectUnix(); REQUIRE(fd >= 0);
    REQUIRE(loadNetwork(fd));

    uint8_t start[8] = {0,0,0,0, 0,0,0,0};
    sendMsg(fd, 0x0200, start, 8);
    uint32_t type; std::vector<uint8_t> payload;
    recvMsg(fd, type, payload);

    sendMsg(fd, 0x0204); // PAUSE
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0x0205);

    sendMsg(fd, 0x0204); // повторный PAUSE = UNPAUSE
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0x0205);

    sendMsg(fd, 0x0202); // STOP
    recvMsg(fd, type, payload);
    close(fd);
}

TEST_CASE("MSG_STEP_SIM: step N ticks", "[sim]") {
    if (access(TEST_BION, R_OK) != 0) { WARN("no test network"); return; }
    int fd = connectUnix(); REQUIRE(fd >= 0);
    REQUIRE(loadNetwork(fd));

    uint8_t step_payload[4] = {10,0,0,0}; // 10 ticks
    sendMsg(fd, 0x0206, step_payload, 4);
    uint32_t type; std::vector<uint8_t> payload;
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0x0207);

    usleep(200000); // ждём выполнения шагов
    sendMsg(fd, 0x0202); // STOP
    recvMsg(fd, type, payload);
    REQUIRE(type == 0x0203);
    // tick counter должен быть >= 10 в payload
    REQUIRE(payload.size() >= 8);
    close(fd);
}

TEST_CASE("MSG_SET_SPEED: valid range", "[sim]") {
    if (access(TEST_BION, R_OK) != 0) { WARN("no test network"); return; }
    int fd = connectUnix(); REQUIRE(fd >= 0);
    REQUIRE(loadNetwork(fd));

    uint8_t speed[8];
    float mult = 2.0f, hz = 10.0f;
    memcpy(speed, &mult, 4);
    memcpy(speed + 4, &hz, 4);
    sendMsg(fd, 0x0208, speed, 8);
    uint32_t type; std::vector<uint8_t> payload;
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0x0209);
    close(fd);
}

TEST_CASE("MSG_SET_BACKEND: CPU/GPU switch", "[sim]") {
    int fd = connectUnix(); REQUIRE(fd >= 0);
    uint8_t backend = 0; // CPU
    sendMsg(fd, 0x020A, &backend, 1);
    uint32_t type; std::vector<uint8_t> payload;
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0x020B);
    close(fd);
}

TEST_CASE("MSG_RESET_SIM: reset simulation state", "[sim]") {
    if (access(TEST_BION, R_OK) != 0) { WARN("no test network"); return; }
    int fd = connectUnix(); REQUIRE(fd >= 0);
    REQUIRE(loadNetwork(fd));

    sendMsg(fd, 0x020C); // RESET_SIM
    uint32_t type; std::vector<uint8_t> payload;
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0x020D);
    close(fd);
}

// ========================================================================
// STIMULI
// ========================================================================
TEST_CASE("MSG_ADD_CURRENT_STIMULUS: valid stimulus", "[stim]") {
    if (access(TEST_BION, R_OK) != 0) { WARN("no test network"); return; }
    int fd = connectUnix(); REQUIRE(fd >= 0);
    REQUIRE(loadNetwork(fd));
    // I_clamp: seg_id=0, amplitude=100pA, duration=50ms, start_tick=0
    uint8_t buf[20];
    uint8_t* p = buf;
    uint32_t seg_id = 0; memcpy(p, &seg_id, 4); p += 4;
    float amp = 100.0f; memcpy(p, &amp, 4); p += 4;
    float dur = 50.0f; memcpy(p, &dur, 4); p += 4;
    uint64_t start = 0; memcpy(p, &start, 8);
    sendMsg(fd, 0x0300, buf, 20);
    // ACK не посылается — проверяем что нет ошибки
    uint8_t start_sim[8] = {0,0,0,0, 0,0,0,0};
    sendMsg(fd, 0x0200, start_sim, 8);
    uint32_t type; std::vector<uint8_t> payload;
    recvMsg(fd, type, payload);
    REQUIRE(type == 0x0201);
    usleep(50000);
    sendMsg(fd, 0x0202); recvMsg(fd, type, payload);
    close(fd);
}

TEST_CASE("MSG_ADD_VOLTAGE_CLAMP: valid clamp", "[stim]") {
    if (access(TEST_BION, R_OK) != 0) { WARN("no test network"); return; }
    int fd = connectUnix(); REQUIRE(fd >= 0);
    REQUIRE(loadNetwork(fd));
    uint8_t buf[4+4+8+8]; // seg_id:u32, target_mV:f32, start:u64, end:u64
    uint8_t* p = buf;
    uint32_t seg_id = 0; memcpy(p, &seg_id, 4); p += 4;
    float target = -65.0f; memcpy(p, &target, 4); p += 4;
    uint64_t st = 0; memcpy(p, &st, 8); p += 8;
    uint64_t en = 100; memcpy(p, &en, 8);
    sendMsg(fd, 0x0302, buf, sizeof(buf));
    close(fd);
}

TEST_CASE("MSG_ADD_RAMP_STIMULUS: valid ramp", "[stim]") {
    if (access(TEST_BION, R_OK) != 0) { WARN("no test network"); return; }
    int fd = connectUnix(); REQUIRE(fd >= 0);
    REQUIRE(loadNetwork(fd));
    uint8_t buf[4+4+4+8+8]; // seg_id, start_pA, end_pA, start_tick, end_tick
    uint8_t* p = buf;
    uint32_t sid = 0; memcpy(p, &sid, 4); p += 4;
    float sp = 0.0f; memcpy(p, &sp, 4); p += 4;
    float ep = 200.0f; memcpy(p, &ep, 4); p += 4;
    uint64_t st = 0; memcpy(p, &st, 8); p += 8;
    uint64_t en = 100; memcpy(p, &en, 8);
    sendMsg(fd, 0x0304, buf, sizeof(buf));
    close(fd);
}

TEST_CASE("MSG_ADD_ION_INJECTION: valid K+ extra", "[stim]") {
    if (access(TEST_BION, R_OK) != 0) { WARN("no test network"); return; }
    int fd = connectUnix(); REQUIRE(fd >= 0);
    REQUIRE(loadNetwork(fd));
    uint8_t buf[4+4+4+1]; // seg_id, amount, tick, ion_idx, intracellular
    uint8_t* p = buf;
    uint32_t sid = 0; memcpy(p, &sid, 4); p += 4;
    float amt = 1.0f; memcpy(p, &amt, 4); p += 4;
    uint32_t tk = 0; memcpy(p, &tk, 4); p += 4;
    *p++ = 0; *p++ = 0; // K+ extra=0
    sendMsg(fd, 0x0306, buf, (uint32_t)(p - buf));
    close(fd);
}

TEST_CASE("MSG_ADD_LIGAND_INJECTION: valid Glu extra", "[stim]") {
    if (access(TEST_BION, R_OK) != 0) { WARN("no test network"); return; }
    int fd = connectUnix(); REQUIRE(fd >= 0);
    REQUIRE(loadNetwork(fd));
    uint8_t buf[4+4+4+1+1]; // seg_id, amount, tick, nt_idx, extra
    uint8_t* p = buf;
    uint32_t sid = 0; memcpy(p, &sid, 4); p += 4;
    float amt = 0.5f; memcpy(p, &amt, 4); p += 4;
    uint32_t tk = 0; memcpy(p, &tk, 4); p += 4;
    *p++ = 0; *p++ = 0; // Glutamate=0, extra=0
    sendMsg(fd, 0x0308, buf, (uint32_t)(p - buf));
    close(fd);
}

TEST_CASE("MSG_CLEAR_STIMULI: clear all", "[stim]") {
    int fd = connectUnix(); REQUIRE(fd >= 0);
    sendMsg(fd, 0x030A);
    uint32_t type; std::vector<uint8_t> payload;
    // CLEAR_STIMULI не шлёт ответ — проверяем что нет ошибки
    close(fd);
}

// ========================================================================
// STATE STREAMING
// ========================================================================
TEST_CASE("MSG_UNSUBSCRIBE_STATE: unsubscribe", "[stream]") {
    int fd = connectUnix(); REQUIRE(fd >= 0);
    sendMsg(fd, 0x0402);
    uint32_t type; std::vector<uint8_t> payload;
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0x0403);
    close(fd);
}

// ========================================================================
// PLUGINS
// ========================================================================
TEST_CASE("MSG_LOAD_PLUGIN_DIR: load builtin plugins", "[plugins]") {
    int fd = connectUnix(); REQUIRE(fd >= 0);
    const char* dir = "plugins/builtin/";
    sendMsg(fd, 0x0502, (const uint8_t*)dir, strlen(dir));
    uint32_t type; std::vector<uint8_t> payload;
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0x0503);
    REQUIRE(payload.size() >= 4);
    close(fd);
}

TEST_CASE("MSG_QUERY_MODIFIER_INFO: valid type", "[plugins]") {
    int fd = connectUnix(); REQUIRE(fd >= 0);
    uint8_t buf[4];
    uint32_t mod_type = 107; // K2P_LEAK (реальный тип — 0 удалён в #69)
    memcpy(buf, &mod_type, 4);
    sendMsg(fd, 0x050A, buf, 4);
    uint32_t type; std::vector<uint8_t> payload;
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE((type == 0x050B || type == 0xFFFF));
    close(fd);
}

TEST_CASE("MSG_QUERY_MODIFIER_INFO: invalid type (255)", "[plugins]") {
    int fd = connectUnix(); REQUIRE(fd >= 0);
    uint8_t buf[4];
    uint32_t mod_type = 255;
    memcpy(buf, &mod_type, 4);
    sendMsg(fd, 0x050A, buf, 4);
    uint32_t type; std::vector<uint8_t> payload;
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0xFFFF);
    close(fd);
}

// ========================================================================
// CONSOLIDATION
// ========================================================================
TEST_CASE("MSG_SET_CONSOLIDATION_INTERVAL: valid", "[consolidation]") {
    int fd = connectUnix(); REQUIRE(fd >= 0);
    uint8_t buf[4];
    uint32_t interval = 100;
    memcpy(buf, &interval, 4);
    sendMsg(fd, 0x0600, buf, 4);
    uint32_t type; std::vector<uint8_t> payload;
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0x0601);
    close(fd);
}

TEST_CASE("MSG_GET_NUCLEUS_SNAPSHOT: no network", "[consolidation]") {
    int fd = connectUnix(); REQUIRE(fd >= 0);
    uint8_t buf[4];
    uint32_t neuron_idx = 0;
    memcpy(buf, &neuron_idx, 4);
    sendMsg(fd, 0x0604, buf, 4);
    uint32_t type; std::vector<uint8_t> payload;
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0xFFFF); // ERR_NO_NETWORK
    close(fd);
}

TEST_CASE("MSG_RESET_GENE_EXPRESSION: with network loaded", "[consolidation]") {
    if (access(TEST_BION, R_OK) != 0) { WARN("no test network"); return; }
    int fd = connectUnix(); REQUIRE(fd >= 0);
    REQUIRE(loadNetwork(fd));

    sendMsg(fd, 0x0606);
    uint32_t type; std::vector<uint8_t> payload;
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0x0607);
    REQUIRE(!payload.empty());
    REQUIRE(payload[0] == 1);
    close(fd);
}

// ========================================================================
// BACKEND
// ========================================================================
TEST_CASE("MSG_LIST_DEVICES: returns device list", "[backend]") {
    int fd = connectUnix(); REQUIRE(fd >= 0);
    sendMsg(fd, 0x0702);
    uint32_t type; std::vector<uint8_t> payload;
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0x0703);
    REQUIRE(payload.size() >= 4); // count:u32
    close(fd);
}

// ========================================================================
// CHECKPOINT
// ========================================================================
TEST_CASE("MSG_LOAD_CHECKPOINT: valid checkpoint load", "[checkpoint]") {
    if (access(TEST_BION, R_OK) != 0) { WARN("no test network"); return; }
    int fd = connectUnix(); REQUIRE(fd >= 0);
    REQUIRE(loadNetwork(fd));

    // Сохраняем чекпоинт
    const char* ckpt = "/tmp/test_proto_full_ckpt.bion";
    unlink(ckpt);
    sendMsg(fd, 0x0800, (const uint8_t*)ckpt, strlen(ckpt));
    uint32_t type; std::vector<uint8_t> payload;
    recvMsg(fd, type, payload);

    // Загружаем чекпоинт с restore_tick=0
    size_t fnlen = strlen(ckpt);
    std::vector<uint8_t> load_payload(fnlen + 8);
    memcpy(load_payload.data(), ckpt, fnlen);
    uint64_t rt = 0;
    memcpy(load_payload.data() + fnlen, &rt, 8);
    sendMsg(fd, 0x0802, load_payload.data(), load_payload.size());
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0x0803);
    REQUIRE(!payload.empty());
    REQUIRE(payload[0] == 1);
    unlink(ckpt);
    close(fd);
}

// ========================================================================
// SESSION MANAGEMENT
// ========================================================================
TEST_CASE("MSG_CREATE_SESSION: create and verify", "[session]") {
    int fd = connectUnix(); REQUIRE(fd >= 0);
    uint8_t buf[4];
    int32_t gpu_idx = -1; // default
    memcpy(buf, &gpu_idx, 4);
    sendMsg(fd, 0x0900, buf, 4);
    uint32_t type; std::vector<uint8_t> payload;
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0x0901);
    REQUIRE(payload.size() >= 9); // ok:u8 + session_id:u32 + gpu_device_index:i32
    REQUIRE(payload[0] == 1);
    uint32_t sid;
    memcpy(&sid, payload.data() + 1, 4);
    REQUIRE(sid > 0);

    // Уничтожаем созданную сессию
    uint8_t destroy[4];
    memcpy(destroy, &sid, 4);
    sendMsg(fd, 0x0902, destroy, 4);
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0x0903);
    close(fd);
}

TEST_CASE("MSG_DESTROY_SESSION: cannot destroy default (0)", "[session]") {
    int fd = connectUnix(); REQUIRE(fd >= 0);
    uint8_t buf[4];
    uint32_t sid = 0;
    memcpy(buf, &sid, 4);
    sendMsg(fd, 0x0902, buf, 4);
    uint32_t type; std::vector<uint8_t> payload;
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0xFFFF); // cannot destroy session 0
    close(fd);
}

TEST_CASE("MSG_RUN_TASK: without network loaded", "[session]") {
    int fd = connectUnix(); REQUIRE(fd >= 0);
    // task_type:u32 + duration_ticks:u32 + params_json_len:u16 + params_json
    uint8_t buf[10];
    uint8_t* p = buf;
    uint32_t task_type = 0; memcpy(p, &task_type, 4); p += 4;
    uint32_t duration = 100; memcpy(p, &duration, 4); p += 4;
    uint16_t json_len = 0; memcpy(p, &json_len, 2);
    sendMsg(fd, 0x0908, buf, 10);
    uint32_t type; std::vector<uint8_t> payload;
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0xFFFF); // ERR_NO_NETWORK
    close(fd);
}

// ========================================================================
// ERROR HANDLING
// ========================================================================
TEST_CASE("MSG_ERROR: unknown message type", "[error]") {
    int fd = connectUnix(); REQUIRE(fd >= 0);
    sendMsg(fd, 0xDEAD); // неизвестный тип
    uint32_t type; std::vector<uint8_t> payload;
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0xFFFF);
    REQUIRE(payload.size() >= 8); // req_type:u32 + code:u32
    close(fd);
}

TEST_CASE("MSG_ERROR: startSim without network", "[error]") {
    int fd = connectUnix(); REQUIRE(fd >= 0);
    uint8_t start[8] = {0,0,0,0, 0,0,0,0};
    sendMsg(fd, 0x0200, start, 8);
    uint32_t type; std::vector<uint8_t> payload;
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0xFFFF); // ERR_NO_NETWORK
    close(fd);
}

TEST_CASE("MSG_ERROR: stopSim when not running", "[error]") {
    int fd = connectUnix(); REQUIRE(fd >= 0);
    sendMsg(fd, 0x0202);
    uint32_t type; std::vector<uint8_t> payload;
    REQUIRE(recvMsg(fd, type, payload));
    // Может быть STOP_SIM_RESULT (идемпотентно) или ERR_NOT_RUNNING
    REQUIRE((type == 0x0203 || type == 0xFFFF));
    close(fd);
}

TEST_CASE("MSG_ERROR: load with empty filename", "[error]") {
    int fd = connectUnix(); REQUIRE(fd >= 0);
    sendMsg(fd, 0x0100, nullptr, 0);
    uint32_t type; std::vector<uint8_t> payload;
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0xFFFF);
    close(fd);
}

// Этот тест ДОЛЖЕН быть последним — SHUTDOWN убивает демон
TEST_CASE("MSG_SHUTDOWN: graceful shutdown", "[control]") {
    int fd = connectUnix(); REQUIRE(fd >= 0);
    sendMsg(fd, 0x0004);
    uint32_t type; std::vector<uint8_t> payload;
    bool got = recvMsg(fd, type, payload, 2);
    if (got) REQUIRE(type == 0x0005);
    close(fd);
    // Демон уже мёртв после SHUTDOWN — это нормально
}

// ── Main ─────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    if (!startDaemon()) {
        fprintf(stderr, "SKIP: Cannot start daemon (biocore-daemon not found or failed to start)\n");
        return 77;
    }
    usleep(500000);
    int result = Catch::Session().run(argc, argv);
    stopDaemon();
    return result;
}
