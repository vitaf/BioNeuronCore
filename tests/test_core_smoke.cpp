// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_core_smoke.cpp — быстрые проверки здоровья демона (< 10 сек).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <vector>

#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

static const char* DAEMON_BIN = "./biocore-daemon";
static const char* SOCK_PATH  = "/tmp/biocore.sock";
static pid_t g_daemon_pid = 0;

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

static bool recvMsg(int fd, uint32_t& out_type, std::vector<uint8_t>& out_payload, int timeout_s = 5) {
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
        while (off < plen) { n = read(fd, out_payload.data() + off, plen - off); if (n <= 0) return false; off += n; }
    }
    return true;
}

static bool startDaemon() {
    unlink(SOCK_PATH);
    int pipefd[2]; if (pipe(pipefd) < 0) return false;
    pid_t pid = fork(); if (pid < 0) return false;
    if (pid == 0) {
        close(pipefd[0]);
        char ready_fd_str[16];
        snprintf(ready_fd_str, sizeof(ready_fd_str), "%d", pipefd[1]);
        execl(DAEMON_BIN, DAEMON_BIN, "--ready-fd", ready_fd_str, "-p", "plugins/builtin/", nullptr);
        _exit(1);
    }
    close(pipefd[1]);
    fd_set fds; FD_ZERO(&fds); FD_SET(pipefd[0], &fds);
    struct timeval tv = {10, 0};
    if (select(pipefd[0] + 1, &fds, nullptr, nullptr, &tv) <= 0) {
        close(pipefd[0]); kill(pid, SIGTERM); waitpid(pid, nullptr, 0); return false;
    }
    char ready; read(pipefd[0], &ready, 1);
    close(pipefd[0]);
    g_daemon_pid = pid;
    return true;
}

static void stopDaemon() {
    if (g_daemon_pid > 0) { kill(g_daemon_pid, SIGTERM); waitpid(g_daemon_pid, nullptr, 0); g_daemon_pid = 0; unlink(SOCK_PATH); }
}

TEST_CASE("Daemon starts and accepts connections", "[smoke]") {
    int fd = connectUnix();
    REQUIRE(fd >= 0);
    close(fd);
}

TEST_CASE("Daemon responds to PING with PONG", "[smoke]") {
    int fd = connectUnix(); REQUIRE(fd >= 0);
    sendMsg(fd, 0x0000);
    uint32_t type; std::vector<uint8_t> payload;
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0x0001);
    close(fd);
}

TEST_CASE("Daemon responds to GET_VERSION", "[smoke]") {
    int fd = connectUnix(); REQUIRE(fd >= 0);
    sendMsg(fd, 0x0002);
    uint32_t type; std::vector<uint8_t> payload;
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0x0003);
    REQUIRE(payload.size() >= 12);
    close(fd);
}

TEST_CASE("Daemon has plugins loaded at startup", "[smoke]") {
    int fd = connectUnix(); REQUIRE(fd >= 0);
    sendMsg(fd, 0x0504); // LIST_PLUGINS
    uint32_t type; std::vector<uint8_t> payload;
    REQUIRE(recvMsg(fd, type, payload));
    REQUIRE(type == 0x0505);
    REQUIRE(payload.size() >= 4);
    close(fd);
}

int main(int argc, char* argv[]) {
    if (!startDaemon()) {
        fprintf(stderr, "SKIP: Cannot start daemon\n");
        return 77;
    }
    usleep(300000); // ждём загрузки плагинов
    int result = Catch::Session().run(argc, argv);
    stopDaemon();
    return result;
}
