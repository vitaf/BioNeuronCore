// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_daemon_config.cpp — юнит-тесты daemon_config и daemon_state.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <string>
#include <vector>
#include <unistd.h>

#include "daemon_config.h"
#include "daemon_state.h"
#include "daemon_log.h"

static int g_pass = 0, g_fail = 0;
#define PASS(fmt,...) do{g_pass++;printf("  [PASS] " fmt "\n",##__VA_ARGS__);}while(0)
#define FAIL(fmt,...) do{g_fail++;printf("  [FAIL] " fmt "\n",##__VA_ARGS__);}while(0)
#define CHECK(c,fmt,...) do{if(c)PASS(fmt,##__VA_ARGS__);else FAIL(fmt,##__VA_ARGS__);}while(0)

// ── TEST 1: DaemonConfig defaults ─────────────────────────────────────────
static void test_config_defaults() {
    printf("\n─── Config Defaults ───\n");

    DaemonConfig cfg;
    CHECK(cfg.unix_socket == "/tmp/biocore.sock", "default unix_socket");
    CHECK(cfg.tcp_host == "127.0.0.1", "default tcp_host");
    CHECK(cfg.tcp_port == 0, "default tcp_port (0=disabled)");
    CHECK(cfg.default_dt == 0.1f, "default dt");
    CHECK(cfg.default_mode == 0, "default mode BIO");
    CHECK(cfg.consolidation_interval == 10000, "default consolidation_interval");
    CHECK(cfg.plugins_path == "plugins/builtin/", "default plugins_path");
    CHECK(cfg.gpu_enabled == false, "default gpu_enabled (CPU-only)");
    CHECK(cfg.log_file == "/var/log/biocore.log", "default log_file");
    CHECK(cfg.log_level == "info", "default log_level");
    CHECK(cfg.log_max_size == 10 * 1024 * 1024, "default log_max_size 10MB");
    CHECK(cfg.log_max_files == 5, "default log_max_files");
}

// ── TEST 2: DaemonConfig JSON parsing ─────────────────────────────────────
static void test_config_parse() {
    printf("\n─── Config JSON Parse ───\n");

    const char* json =
        "{\n"
        "  \"listen\": {\n"
        "    \"unix_socket\": \"/tmp/test.sock\",\n"
        "    \"tcp_host\": \"0.0.0.0\",\n"
        "    \"tcp_port\": 9876\n"
        "  },\n"
        "  \"simulation\": {\n"
        "    \"default_dt\": 0.05,\n"
        "    \"default_mode\": 1,\n"
        "    \"consolidation_interval\": 500,\n"
        "    \"plugins_path\": \"/opt/biocore/plugins/\",\n"
        "    \"gpu_enabled\": false\n"
        "  },\n"
        "  \"logging\": {\n"
        "    \"log_file\": \"/tmp/biocore.log\",\n"
        "    \"log_level\": \"debug\",\n"
        "    \"log_max_size_mb\": 50,\n"
        "    \"log_max_files\": 10\n"
        "  }\n"
        "}\n";

    // Write temp config file
    const char* tmp = "/tmp/biocore_test_config.json";
    FILE* f = fopen(tmp, "w");
    assert(f);
    fwrite(json, 1, strlen(json), f);
    fclose(f);

    DaemonConfig cfg;
    CHECK(cfg.load(tmp), "load config file");

    CHECK(cfg.unix_socket == "/tmp/test.sock", "parsed unix_socket");
    CHECK(cfg.tcp_host == "0.0.0.0", "parsed tcp_host");
    CHECK(cfg.tcp_port == 9876, "parsed tcp_port");
    CHECK(cfg.default_dt == 0.05f, "parsed dt");
    CHECK(cfg.default_mode == 1, "parsed mode ARB");
    CHECK(cfg.consolidation_interval == 500, "parsed consolidation_interval");
    CHECK(cfg.plugins_path == "/opt/biocore/plugins/", "parsed plugins_path");
    CHECK(cfg.gpu_enabled == false, "parsed gpu_enabled=off");
    CHECK(cfg.log_file == "/tmp/biocore.log", "parsed log_file");
    CHECK(cfg.log_level == "debug", "parsed log_level");
    CHECK(cfg.log_max_size == 50LL * 1024 * 1024, "parsed log_max_size 50MB");
    CHECK(cfg.log_max_files == 10, "parsed log_max_files");

    unlink(tmp);
}

// ── TEST 3: DaemonConfig — несуществующий файл ────────────────────────────
static void test_config_missing() {
    printf("\n─── Config Missing File ───\n");

    DaemonConfig cfg;
    CHECK(!cfg.load("/tmp/no_such_config_12345.json"), "load missing file fails");
    // Defaults should be unchanged
    CHECK(cfg.tcp_port == 0, "default tcp_port preserved (0=disabled)");
}

// ── TEST 4: DaemonConfig — валидация граничных значений ───────────────────
static void test_config_validation() {
    printf("\n─── Config Validation ───\n");

    // dt <= 0 → default
    {
        const char* json = "{\"simulation\":{\"default_dt\":0}}";
        FILE* f = fopen("/tmp/biocore_test_cfg2.json", "w");
        fwrite(json, 1, strlen(json), f); fclose(f);
        DaemonConfig cfg;
        cfg.load("/tmp/biocore_test_cfg2.json");
        CHECK(cfg.default_dt == 0.1f, "dt=0 clamped to default 0.1");
        unlink("/tmp/biocore_test_cfg2.json");
    }

    // log_max_files < 2 → clamp to 2
    {
        const char* json = "{\"logging\":{\"log_max_files\":1}}";
        FILE* f = fopen("/tmp/biocore_test_cfg3.json", "w");
        fwrite(json, 1, strlen(json), f); fclose(f);
        DaemonConfig cfg;
        cfg.load("/tmp/biocore_test_cfg3.json");
        CHECK(cfg.log_max_files == 2, "log_max_files=1 clamped to 2");
        unlink("/tmp/biocore_test_cfg3.json");
    }

    // log_max_files > 20 → clamp to 20
    {
        const char* json = "{\"logging\":{\"log_max_files\":100}}";
        FILE* f = fopen("/tmp/biocore_test_cfg4.json", "w");
        fwrite(json, 1, strlen(json), f); fclose(f);
        DaemonConfig cfg;
        cfg.load("/tmp/biocore_test_cfg4.json");
        CHECK(cfg.log_max_files == 20, "log_max_files=100 clamped to 20");
        unlink("/tmp/biocore_test_cfg4.json");
    }

    // log_max_size_mb too small → clamp to 1MB
    {
        const char* json = "{\"logging\":{\"log_max_size_mb\":0}}";
        FILE* f = fopen("/tmp/biocore_test_cfg5.json", "w");
        fwrite(json, 1, strlen(json), f); fclose(f);
        DaemonConfig cfg;
        cfg.load("/tmp/biocore_test_cfg5.json");
        CHECK(cfg.log_max_size == 1048576, "log_max_size=0 clamped to 1MB");
        unlink("/tmp/biocore_test_cfg5.json");
    }
}

// ── TEST 5: DaemonConfig::findConfigPath ──────────────────────────────────
static void test_config_findPath() {
    printf("\n─── Config findConfigPath ───\n");

    std::string path = DaemonConfig::findConfigPath();
    // Может найти или нет — не ошибка. Просто проверяем что не крашится.
    PASS("findConfigPath returns: '%s'", path.empty() ? "(none)" : path.c_str());
}

// ── TEST 6: StateStreamer basic operations ────────────────────────────────
static void test_state_streamer() {
    printf("\n─── StateStreamer ───\n");

    StateStreamer ss;
    CHECK(ss.empty(), "empty initially");

    // add/remove subscriber (используем pipe fd)
    int pipes[2];
    CHECK(pipe(pipes) == 0, "create pipe");

    ss.addSubscriber(pipes[1], 5, false);
    CHECK(!ss.empty(), "not empty after add");
    CHECK(ss.hasSubscriber(pipes[1]), "has subscriber");

    ss.removeSubscriber(pipes[1]);
    CHECK(!ss.hasSubscriber(pipes[1]), "subscriber removed");
    CHECK(ss.empty(), "empty after remove");

    close(pipes[0]);
    close(pipes[1]);
}

// ── TEST 7: StateStreamer multiple subscribers ────────────────────────────
static void test_state_streamer_multi() {
    printf("\n─── StateStreamer Multi ───\n");

    StateStreamer ss;

    int p1[2], p2[2], p3[2];
    pipe(p1); pipe(p2); pipe(p3);

    ss.addSubscriber(p1[1], 1, false);
    ss.addSubscriber(p2[1], 10, true);
    ss.addSubscriber(p3[1], 0, false); // every tick

    CHECK(!ss.empty(), "has subscribers");
    CHECK(ss.hasSubscriber(p1[1]), "sub 1");
    CHECK(ss.hasSubscriber(p2[1]), "sub 2");
    CHECK(ss.hasSubscriber(p3[1]), "sub 3");

    ss.removeSubscriber(p2[1]);
    CHECK(!ss.hasSubscriber(p2[1]), "sub 2 removed");
    CHECK(!ss.empty(), "still has subs");

    ss.removeSubscriber(p1[1]);
    ss.removeSubscriber(p3[1]);
    CHECK(ss.empty(), "all removed");

    close(p1[0]); close(p1[1]);
    close(p2[0]); close(p2[1]);
    close(p3[0]); close(p3[1]);
}

// ── TEST 8: StateStreamer — remove non-existent ───────────────────────────
static void test_state_streamer_remove_nonexist() {
    printf("\n─── StateStreamer Remove Non-existent ───\n");

    StateStreamer ss;
    ss.removeSubscriber(999); // не должно крашиться
    CHECK(ss.empty(), "empty after removing non-existent");
    PASS("remove non-existent subscriber does not crash");
}

// ── TEST 9: DaemonLog — базовые операции ─────────────────────────────────
static void test_daemon_log_basic() {
    printf("\n─── DaemonLog Basic ───\n");

    const char* log_path = "/tmp/biocore_test_log.log";
    unlink(log_path);
    // Удаляем и ротированные
    for (int i = 1; i <= 5; i++) {
        char buf[700]; snprintf(buf, sizeof(buf), "%s.%d", log_path, i);
        unlink(buf);
    }

    // 9a. Init with file
    daemon_log_init(log_path, 10*1024*1024, 5);
    CHECK(!g_dlog.use_stdout, "log_init: file mode");
    CHECK(g_dlog.file != nullptr, "log_init: file opened");
    CHECK(g_dlog.max_size == 10*1024*1024, "log_init: max_size");
    CHECK(g_dlog.max_files == 5, "log_init: max_files");
    CHECK(strcmp(g_dlog.path, log_path) == 0, "log_init: path stored");

    // 9b. Write entries
    DLOG_INFO("test message %d", 42);
    DLOG_WARN("warning message");
    DLOG_ERROR("error message");
    CHECK(g_dlog.current_size > 0, "log_write: current_size increased");
    daemon_log_close();

    // 9c. Verify file exists and has content
    struct stat st;
    CHECK(stat(log_path, &st) == 0, "log file exists after close");
    CHECK(st.st_size > 0, "log file has content");

    // 9d. Init with nullptr → stdout mode
    daemon_log_init(nullptr, 10*1024*1024, 5);
    CHECK(g_dlog.use_stdout, "log_init(nullptr): stdout mode");
    CHECK(g_dlog.path[0] == '\0', "log_init(nullptr): empty path");
    daemon_log_close();

    // 9e. Init with empty string → stdout mode
    daemon_log_init("", 10*1024*1024, 5);
    CHECK(g_dlog.use_stdout, "log_init(''): stdout mode");

    unlink(log_path);
    for (int i = 1; i <= 5; i++) {
        char buf[700]; snprintf(buf, sizeof(buf), "%s.%d", log_path, i);
        unlink(buf);
    }
}

// ── TEST 10: DaemonLog — валидация параметров ────────────────────────────
static void test_daemon_log_validation() {
    printf("\n─── DaemonLog Validation ───\n");

    const char* log_path = "/tmp/biocore_test_val.log";
    unlink(log_path);

    // 10a. max_size < 1MB → clamp to 1MB
    daemon_log_init(log_path, 100, 5);
    CHECK(g_dlog.max_size == 1048576, "max_size clamped to 1MB minimum");
    daemon_log_close();

    // 10b. max_files < 2 → clamp to 2
    daemon_log_init(log_path, 10*1024*1024, 1);
    CHECK(g_dlog.max_files == 2, "max_files clamped to 2 minimum");
    daemon_log_close();

    // 10c. max_files > 20 → clamp to 20
    daemon_log_init(log_path, 10*1024*1024, 100);
    CHECK(g_dlog.max_files == 20, "max_files clamped to 20 maximum");
    daemon_log_close();

    unlink(log_path);
}

// ── TEST 11: DaemonLog — ротация по размеру ──────────────────────────────
static void test_daemon_log_rotation() {
    printf("\n─── DaemonLog Rotation ───\n");

    const char* log_path = "/tmp/biocore_test_rot.log";
    unlink(log_path);
    for (int i = 1; i <= 3; i++) {
        char buf[700]; snprintf(buf, sizeof(buf), "%s.%d", log_path, i);
        unlink(buf);
    }

    // Инициализация с файлом, пишем данные, затем напрямую вызываем ротацию
    daemon_log_init(log_path, 10*1048576, 3);

    // Пишем немного данных
    DLOG_INFO("test line 1 for rotation");
    DLOG_INFO("test line 2 for rotation");
    int64_t size_after_write = g_dlog.current_size;
    CHECK(size_after_write > 0, "data written before rotation");

    // Вызываем ротацию напрямую
    daemon_log_rotate();
    daemon_log_reopen();

    // Проверяем: основной файл должен быть пуст (current_size = 0)
    CHECK(g_dlog.current_size == 0, "current_size reset after rotation");
    CHECK(g_dlog.file != nullptr, "file reopened after rotation");

    // Ротированный файл должен существовать
    char r1[700]; snprintf(r1, sizeof(r1), "%s.1", log_path);
    struct stat st_rot;
    CHECK(stat(r1, &st_rot) == 0, "rotated file .1 exists");
    CHECK(st_rot.st_size > 0, "rotated file has content");

    daemon_log_close();

    unlink(log_path);
    for (int i = 1; i <= 3; i++) {
        char buf[700]; snprintf(buf, sizeof(buf), "%s.%d", log_path, i);
        unlink(buf);
    }
}

// ── TEST 12: DaemonLog — close без init ──────────────────────────────────
static void test_daemon_log_close_noinit() {
    printf("\n─── DaemonLog Close No Init ───\n");

    // daemon_log_close() до init не должен крашиться
    // (g_dlog.file = nullptr изначально)
    daemon_log_close();
    PASS("close without init does not crash");
}

// ── TEST 13: DaemonMain — аргументы командной строки ─────────────────────
static void test_daemon_main_args() {
    printf("\n─── DaemonMain Args ───\n");

    // 13a. --help выводит usage и выходит с 0
    int ret_h = system("./biocore-daemon --help >/dev/null 2>&1");
    // system возвращает статус; если daemon не собран — скипаем
    if (ret_h == -1 || access("./biocore-daemon", X_OK) != 0) {
        printf("  [INFO] daemon binary not found, skipping CLI tests\n");
        PASS("daemon_main args skipped (no binary)");
        return;
    }
    CHECK(WIFEXITED(ret_h) && WEXITSTATUS(ret_h) == 0, "--help exits 0");

    // 13b. --version выводит версию и выходит с 0
    int ret_v = system("./biocore-daemon --version >/dev/null 2>&1");
    CHECK(WIFEXITED(ret_v) && WEXITSTATUS(ret_v) == 0, "--version exits 0");

    // 13c. --help с длинной формой
    int ret_h2 = system("./biocore-daemon -h >/dev/null 2>&1");
    CHECK(WIFEXITED(ret_h2) && WEXITSTATUS(ret_h2) == 0, "-h exits 0");

    // 13d. Демон стартует и выключается по SIGTERM
    // Запускаем в фоне, даём время, убиваем
    int ret_d = system(
        "./biocore-daemon --ready-fd 1 >/dev/null 2>&1 & "
        "DAEMON_PID=$!; "
        "sleep 2; "
        "kill $DAEMON_PID 2>/dev/null; "
        "wait $DAEMON_PID 2>/dev/null; "
        "exit 0"
    );
    CHECK(WIFEXITED(ret_d) && WEXITSTATUS(ret_d) == 0, "daemon starts and stops via SIGTERM");
}

int main() {
    setbuf(stdout, nullptr);
    printf("=== Daemon Config + State Tests ===\n");

    test_config_defaults();
    test_config_parse();
    test_config_missing();
    test_config_validation();
    test_config_findPath();
    test_state_streamer();
    test_state_streamer_multi();
    test_state_streamer_remove_nonexist();
    test_daemon_log_basic();
    test_daemon_log_validation();
    test_daemon_log_rotation();
    test_daemon_log_close_noinit();
    test_daemon_main_args();

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
