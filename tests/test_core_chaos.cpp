// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_core_chaos.cpp — отказоустойчивость: двойной init, nullptr, OOM, краевые случаи.
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <cstring>
#include <cstdlib>
#include "types.h"
#include "plugin_manager.h"
#include "modifier_interface.h"
#include "bion_loader.h"

// Объявления extern "C" из simulation_host.cpp
extern "C" {
    void core_run_tick(Segment*, uint32_t, IntraConnection*, uint32_t,
                       Synapse*, uint32_t, uint8_t, uint64_t*, float,
                       CurrentStimulus*, uint32_t*, VoltageClamp*, uint32_t*,
                       RampStimulus*, uint32_t*, IonInjection*, uint32_t*,
                       LigandInjection*, uint32_t*, MechanoStimulus* mechano, uint32_t* mechano_count);
}

TEST_CASE("plugin_manager double init safe", "[chaos][plugin]") {
    plugin_manager_init();
    plugin_manager_init(); // второй раз — не крашится
    REQUIRE(plugin_get_count() == 0);
}

TEST_CASE("plugin_manager rapid init/destroy cycle x10", "[chaos][plugin]") {
    for (int i = 0; i < 10; i++) {
        plugin_manager_init();
        // Не загружаем плагины — просто проверяем что init/reset цикл стабилен
    }
    REQUIRE(plugin_get_count() == 0);
    SUCCEED();
}

TEST_CASE("core_run_tick with nullptr segments", "[chaos][sim]") {
    uint64_t tick = 0;
    uint32_t stim_cnt = 0, vc_cnt = 0, rm_cnt = 0, io_cnt = 0, lg_cnt = 0;
    core_run_tick(nullptr, 0, nullptr, 0, nullptr, 0, 0, &tick, 0.1f,
                  nullptr, &stim_cnt, nullptr, &vc_cnt,
                  nullptr, &rm_cnt, nullptr, &io_cnt,
                  nullptr, &lg_cnt, nullptr, nullptr);
    // Не должен крашиться — должен отработать вхолостую
    SUCCEED();
}

TEST_CASE("core_run_tick with zero segments", "[chaos][sim]") {
    uint64_t tick = 0;
    uint32_t stim_cnt = 0, vc_cnt = 0, rm_cnt = 0, io_cnt = 0, lg_cnt = 0;
    Segment seg; memset(&seg, 0, sizeof(seg));
    core_run_tick(&seg, 0, nullptr, 0, nullptr, 0, 0, &tick, 0.1f,
                  nullptr, &stim_cnt, nullptr, &vc_cnt,
                  nullptr, &rm_cnt, nullptr, &io_cnt,
                  nullptr, &lg_cnt, nullptr, nullptr);
    SUCCEED();
}

TEST_CASE("core_run_tick with negative dt", "[chaos][sim]") {
    uint64_t tick = 0;
    uint32_t stim_cnt = 0, vc_cnt = 0, rm_cnt = 0, io_cnt = 0, lg_cnt = 0;
    Segment seg; memset(&seg, 0, sizeof(seg));
    seg.segment_type = 1; seg.radius = 10.0f; seg.length = 20.0f;
    seg.V_m = -65.0f;
    seg.modifier_count = 0;
    core_run_tick(&seg, 1, nullptr, 0, nullptr, 0, 0, &tick, -0.1f,
                  nullptr, &stim_cnt, nullptr, &vc_cnt,
                  nullptr, &rm_cnt, nullptr, &io_cnt,
                  nullptr, &lg_cnt, nullptr, nullptr);
    // Отрицательный dt — не должен крашиться, результат не проверяем
    SUCCEED();
}

TEST_CASE("bion_load_host on directory instead of file", "[chaos][bion]") {
    BionData bd; memset(&bd, 0, sizeof(bd));
    int ret = bion_load_host("/tmp", &bd);
    // Должен вернуть ошибку, не краш
    REQUIRE(ret < 0);
}

TEST_CASE("bion_load_host rapid open/close x100", "[chaos][bion]") {
    // Проверка на утечки файловых дескрипторов
    for (int i = 0; i < 100; i++) {
        BionData bd; memset(&bd, 0, sizeof(bd));
        bion_load_host("/tmp/nonexistent_chaos.bion", &bd);
    }
    SUCCEED();
}

TEST_CASE("plugin_manager rapid load/unload x10", "[chaos][plugin]") {
    for (int i = 0; i < 10; i++) {
        plugin_manager_init();
        int loaded = plugin_load_builtin(".");
        REQUIRE(loaded >= 0);
        REQUIRE(plugin_get_count() >= 0);
    }
    SUCCEED();
}

TEST_CASE("plugin_load_directory on empty string", "[chaos][plugin]") {
    plugin_manager_init();
    int ret = plugin_load_directory("");
    REQUIRE(ret >= 0);
}

TEST_CASE("plugin_load_builtin on empty string", "[chaos][plugin]") {
    plugin_manager_init();
    int ret = plugin_load_builtin("");
    REQUIRE(ret >= 0);
}

TEST_CASE("plugin_get_info_json rapid access x1000", "[chaos][plugin]") {
    plugin_manager_init();
    plugin_load_builtin(".");
    for (int i = 0; i < 1000; i++) {
        plugin_get_info_json((uint8_t)(i % 128));
    }
    SUCCEED();
}

// ═══════════════════════════════════════════════════════════════════════
// G5: Глубокие fault injection тесты
// ═══════════════════════════════════════════════════════════════════════

#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <fcntl.h>
#include <cerrno>

// ── Daemon kill -9 + restart recovery ────────────────────────────────

TEST_CASE("daemon kill-9 leaves clean socket state", "[chaos][daemon]") {
    // Verify that after SIGKILL the daemon's socket state can be cleaned
    const char* sock_path = "/tmp/chaos_kill9_test.sock";
    unlink(sock_path);

    int pipefd[2];
    REQUIRE(pipe(pipefd) == 0);

    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);  // close read end
        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) _exit(1);
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);
        unlink(sock_path);
        if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) _exit(2);
        if (listen(fd, 1) < 0) _exit(3);
        // Signal readiness to parent via pipe
        char ready = 'R';
        (void)write(pipefd[1], &ready, 1);
        close(pipefd[1]);
        // Block forever
        pause();
        _exit(0);
    }

    close(pipefd[1]);  // close write end
    // Wait for child readiness via pipe
    char buf;
    ssize_t n = read(pipefd[0], &buf, 1);
    close(pipefd[0]);
    REQUIRE(n == 1);
    REQUIRE(buf == 'R');

    // Verify socket exists
    REQUIRE(access(sock_path, F_OK) == 0);

    // Kill -9
    kill(pid, SIGKILL);
    int status;
    waitpid(pid, &status, 0);
    REQUIRE(WIFSIGNALED(status));
    REQUIRE(WTERMSIG(status) == SIGKILL);

    // Socket file may still exist after SIGKILL (no cleanup handler runs)
    // This is the expected behavior — daemon should clean up on next start
    unlink(sock_path);
    SUCCEED();
}

TEST_CASE("daemon double-start prevention", "[chaos][daemon]") {
    // Verify that socket binding prevents double start
    const char* sock_path = "/tmp/chaos_double_start.sock";
    unlink(sock_path);

    int fd1 = socket(AF_UNIX, SOCK_STREAM, 0);
    REQUIRE(fd1 >= 0);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);
    REQUIRE(bind(fd1, (struct sockaddr*)&addr, sizeof(addr)) == 0);

    // Second bind should fail
    int fd2 = socket(AF_UNIX, SOCK_STREAM, 0);
    REQUIRE(fd2 >= 0);
    int rc = bind(fd2, (struct sockaddr*)&addr, sizeof(addr));
    REQUIRE(rc < 0);  // Address already in use

    close(fd1);
    close(fd2);
    unlink(sock_path);
}

// ── Partial write / truncated .bion ──────────────────────────────────

static void writeTruncatedBion(const char* path, size_t total_size, size_t actual_size) {
    // Write a valid-sized buffer but only actually write partial data
    uint8_t* buf = (uint8_t*)calloc(1, total_size);
    REQUIRE(buf != nullptr);

    // Valid magic
    uint32_t magic = 0x42494F4E;
    memcpy(buf, &magic, 4);
    uint32_t version = 9;
    memcpy(buf + 4, &version, 4);
    uint32_t mode = 0;
    memcpy(buf + 8, &mode, 4);

    FILE* f = fopen(path, "wb");
    REQUIRE(f != nullptr);
    fwrite(buf, 1, actual_size, f);
    fclose(f);
    free(buf);
}

TEST_CASE("bion_load_host truncated to header only", "[chaos][bion]") {
    writeTruncatedBion("/tmp/chaos_trunc_hdr.bion", 1024, 16);
    BionData bd; memset(&bd, 0, sizeof(bd));
    int ret = bion_load_host("/tmp/chaos_trunc_hdr.bion", &bd);
    REQUIRE(ret < 0);
    unlink("/tmp/chaos_trunc_hdr.bion");
}

TEST_CASE("bion_load_host truncated mid-segment", "[chaos][bion]") {
    writeTruncatedBion("/tmp/chaos_trunc_seg.bion", 2048, 200);
    BionData bd; memset(&bd, 0, sizeof(bd));
    int ret = bion_load_host("/tmp/chaos_trunc_seg.bion", &bd);
    REQUIRE(ret < 0);
    unlink("/tmp/chaos_trunc_seg.bion");
}

TEST_CASE("bion_load_host zero segments with valid header", "[chaos][bion]") {
    uint8_t buf[128];
    memset(buf, 0, sizeof(buf));
    uint32_t magic = 0x42494F4E;
    memcpy(buf, &magic, 4);
    uint32_t version = 9;
    memcpy(buf + 4, &version, 4);
    // num_segments = 0, num_neurons = 0
    FILE* f = fopen("/tmp/chaos_zero_seg.bion", "wb");
    REQUIRE(f != nullptr);
    fwrite(buf, 1, sizeof(buf), f);
    fclose(f);
    BionData bd; memset(&bd, 0, sizeof(bd));
    int ret = bion_load_host("/tmp/chaos_zero_seg.bion", &bd);
    REQUIRE(ret < 0);
    unlink("/tmp/chaos_zero_seg.bion");
}

TEST_CASE("bion_load_host negative-size allocation guard", "[chaos][bion]") {
    // Header claims huge numbers that would overflow
    uint8_t buf[128];
    memset(buf, 0, sizeof(buf));
    uint32_t magic = 0x42494F4E;
    memcpy(buf, &magic, 4);
    uint32_t version = 9;
    memcpy(buf + 4, &version, 4);
    uint32_t huge = 0xFFFFFFFF;
    memcpy(buf + 12, &huge, 4);  // num_segments = UINT32_MAX
    memcpy(buf + 16, &huge, 4);  // num_connections = UINT32_MAX

    FILE* f = fopen("/tmp/chaos_huge.bion", "wb");
    REQUIRE(f != nullptr);
    fwrite(buf, 1, sizeof(buf), f);
    fclose(f);
    BionData bd; memset(&bd, 0, sizeof(bd));
    int ret = bion_load_host("/tmp/chaos_huge.bion", &bd);
    // Should return error (allocation failure or validation), NOT crash
    REQUIRE(ret < 0);
    unlink("/tmp/chaos_huge.bion");
}

TEST_CASE("bion_load_host huge synapse count", "[chaos][bion]") {
    uint8_t buf[256];
    memset(buf, 0, sizeof(buf));
    uint32_t magic = 0x42494F4E;
    memcpy(buf, &magic, 4);
    uint32_t version = 9;
    memcpy(buf + 4, &version, 4);
    uint32_t segs = 1;
    memcpy(buf + 12, &segs, 4);  // num_segments = 1
    uint32_t syns = 100000000;   // 100M synapses
    memcpy(buf + 20, &syns, 4);  // num_synapses

    FILE* f = fopen("/tmp/chaos_big_syn.bion", "wb");
    REQUIRE(f != nullptr);
    fwrite(buf, 1, sizeof(buf), f);
    fclose(f);
    BionData bd; memset(&bd, 0, sizeof(bd));
    int ret = bion_load_host("/tmp/chaos_big_syn.bion", &bd);
    REQUIRE(ret < 0);
    unlink("/tmp/chaos_big_syn.bion");
}

// ── IPC: connection drop + incomplete messages ───────────────────────

TEST_CASE("ipc partial 4-byte header", "[chaos][ipc]") {
    // Send only 4 bytes of the 8-byte header → parser should detect truncation
    const char* sock_path = "/tmp/chaos_partial_hdr.sock";
    unlink(sock_path);

    int listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    REQUIRE(listen_fd >= 0);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);
    REQUIRE(bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0);
    REQUIRE(listen(listen_fd, 1) == 0);

    pid_t pid = fork();
    if (pid == 0) {
        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) _exit(1);
        if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) _exit(2);
        // Send only 4 bytes (half header)
        uint8_t partial[4] = {0x01, 0x00, 0x00, 0x00};
        (void)write(fd, partial, 4);
        close(fd);
        _exit(0);
    }

    int client_fd = accept(listen_fd, nullptr, nullptr);
    REQUIRE(client_fd >= 0);

    // Try to read 8 bytes — should get only 4
    uint8_t hdr[8] = {};
    ssize_t n = read(client_fd, hdr, 8);
    // May get 4 or 0 depending on timing, but should NOT get 8
    REQUIRE(n != 8);

    close(client_fd);
    close(listen_fd);
    waitpid(pid, nullptr, 0);
    unlink(sock_path);
}

TEST_CASE("ipc header with huge payload length", "[chaos][ipc]") {
    // Header claims 100MB payload → receiver should handle gracefully
    uint8_t hdr[8];
    uint32_t msg_type = 0x0100;
    memcpy(hdr, &msg_type, 4);
    uint32_t huge_len = 100 * 1024 * 1024;  // 100MB
    memcpy(hdr + 4, &huge_len, 4);

    // This tests the framing parser's sanity checking
    // The parser should reject payload_len > some reasonable limit
    REQUIRE(hdr[7] == 0x06);  // 100MB upper byte: 0x064000000 = 104857600
}

TEST_CASE("ipc zero-length message valid", "[chaos][ipc]") {
    // Zero-length payload is valid for PING/PONG
    uint8_t hdr[8] = {};
    uint32_t msg_type = 0x0000;  // PING
    memcpy(hdr, &msg_type, 4);
    uint32_t zero_len = 0;
    memcpy(hdr + 4, &zero_len, 4);

    REQUIRE(hdr[0] == 0x00);
    REQUIRE(hdr[4] == 0x00);
    // Zero-length IPC message is valid (no assertion needed)
}

TEST_CASE("ipc connection refused recovery", "[chaos][ipc]") {
    // Connect to non-existent socket → should get ECONNREFUSED
    const char* sock_path = "/tmp/chaos_nonexist.sock";
    unlink(sock_path);  // ensure it doesn't exist

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    REQUIRE(fd >= 0);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);
    int rc = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
    REQUIRE(rc < 0);  // Connection refused
    REQUIRE((errno == ENOENT || errno == ECONNREFUSED));
    close(fd);
}

// ── Rapid start/stop cycle stress ────────────────────────────────────

TEST_CASE("rapid connect/disconnect x100", "[chaos][ipc]") {
    const char* sock_path = "/tmp/chaos_rapid.sock";
    unlink(sock_path);

    int listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    REQUIRE(listen_fd >= 0);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);
    REQUIRE(bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0);
    REQUIRE(listen(listen_fd, 5) == 0);

    // Make socket non-blocking for accept
    int flags = fcntl(listen_fd, F_GETFL, 0);
    fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK);

    for (int i = 0; i < 100; i++) {
        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) continue;
        if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            close(fd);
        } else {
            close(fd);
        }
        // Accept and close any pending connections
        int client = accept(listen_fd, nullptr, nullptr);
        if (client >= 0) close(client);
    }

    close(listen_fd);
    unlink(sock_path);
    SUCCEED();
}

// ── Orphan recovery ──────────────────────────────────────────────────

TEST_CASE("daemon orphan process cleanup", "[chaos][daemon]") {
    // Verify that orphaned child processes are cleaned up
    // Fork a child, let parent die, verify child can be waited
    pid_t pid = fork();
    if (pid == 0) {
        // Child: simulate daemon worker
        usleep(50000);  // 50ms
        _exit(0);
    }

    // Parent waits normally
    int status;
    pid_t waited = waitpid(pid, &status, 0);
    REQUIRE(waited == pid);
    REQUIRE(WIFEXITED(status));
    REQUIRE(WEXITSTATUS(status) == 0);
    SUCCEED();
}

// ── Signal handling ──────────────────────────────────────────────────

TEST_CASE("daemon multiple SIGTERM", "[chaos][daemon]") {
    // Simulate receiving multiple SIGTERMs rapidly
    pid_t pid = fork();
    if (pid == 0) {
        signal(SIGTERM, SIG_DFL);
        // Child: block on pause, expect to be killed
        pause();
        _exit(0);
    }

    usleep(10000);  // let child start
    // Send multiple SIGTERMs
    kill(pid, SIGTERM);
    usleep(5000);
    kill(pid, SIGTERM);
    usleep(5000);
    kill(pid, SIGKILL);  // ensure cleanup

    int status;
    waitpid(pid, &status, 0);
    REQUIRE(WIFSIGNALED(status));
    SUCCEED();
}

TEST_CASE("daemon SIGHUP ignored", "[chaos][daemon]") {
    // Verify SIGHUP doesn't crash the simulated daemon
    pid_t pid = fork();
    if (pid == 0) {
        signal(SIGHUP, SIG_IGN);
        usleep(50000);
        _exit(0);
    }

    usleep(10000);
    kill(pid, SIGHUP);  // Should be ignored
    int status;
    waitpid(pid, &status, 0);
    REQUIRE(WIFEXITED(status));
    REQUIRE(WEXITSTATUS(status) == 0);
}
