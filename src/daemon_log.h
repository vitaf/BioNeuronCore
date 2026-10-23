// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#pragma once
// daemon_log.h — логирование демона с ротацией по размеру (как в Studio debug_log.h).
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>

#define DAEMON_LOG_DEFAULT_MAX_SIZE   (10 * 1024 * 1024)
#define DAEMON_LOG_DEFAULT_MAX_FILES  5

struct DaemonLogCtx {
    FILE*   file = nullptr;
    char    path[640] = {};
    int64_t max_size = DAEMON_LOG_DEFAULT_MAX_SIZE;
    int     max_files = DAEMON_LOG_DEFAULT_MAX_FILES;
    int64_t current_size = 0;
    bool    use_stdout = true;  // fallback to stdout if no file
};

static DaemonLogCtx g_dlog;

// Убедиться что директория для логов существует
inline void daemon_log_ensure_dir(const char* log_path) {
    char dir[640];
    strncpy(dir, log_path, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
    // Найти последний '/'
    char* slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        // Рекурсивно создать директории
        char tmp[640];
        snprintf(tmp, sizeof(tmp), "%s", dir);
        for (char* p = tmp + 1; *p; p++) {
            if (*p == '/') {
                *p = '\0';
                mkdir(tmp, 0755);
                *p = '/';
            }
        }
        mkdir(tmp, 0755);
    }
}

// Ротация: log → log.1 → log.2 → ... → log.N
inline void daemon_log_rotate() {
    // Удаляем самый старый
    char old[700];
    snprintf(old, sizeof(old), "%s.%d", g_dlog.path, g_dlog.max_files - 1);
    unlink(old);

    for (int i = g_dlog.max_files - 2; i >= 0; i--) {
        char src[700], dst[700];
        if (i == 0) snprintf(src, sizeof(src), "%s", g_dlog.path);
        else        snprintf(src, sizeof(src), "%s.%d", g_dlog.path, i);
        snprintf(dst, sizeof(dst), "%s.%d", g_dlog.path, i + 1);
        rename(src, dst);
    }
}

inline void daemon_log_reopen() {
    if (g_dlog.file) { fclose(g_dlog.file); g_dlog.file = nullptr; }
    g_dlog.file = fopen(g_dlog.path, "a");
    g_dlog.current_size = 0;
    if (!g_dlog.file) g_dlog.use_stdout = true;
}

// Инициализация лог-файла
inline void daemon_log_init(const char* path, int64_t max_size, int max_files) {
    g_dlog.max_size  = (max_size < 1048576) ? 1048576 : max_size;  // минимум 1MB
    g_dlog.max_files = (max_files < 2) ? 2 : (max_files > 20 ? 20 : max_files);

    if (path && path[0]) {
        strncpy(g_dlog.path, path, sizeof(g_dlog.path) - 1);
        g_dlog.path[sizeof(g_dlog.path) - 1] = '\0';
        daemon_log_ensure_dir(path);
        g_dlog.use_stdout = false;
    } else {
        g_dlog.path[0] = '\0';
        g_dlog.use_stdout = true;
    }

    if (!g_dlog.use_stdout) {
        daemon_log_reopen();
        if (!g_dlog.file) g_dlog.use_stdout = true;
    }
}

// Основная функция логирования
inline void daemon_log_write(const char* level, const char* fmt, ...) {
    char msg[4096];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    if (n < 0) return;

    // Формат: unixtime [важность] [сообщение]
    time_t now = time(nullptr);
    char line[4608];
    int total = snprintf(line, sizeof(line), "[%lld] [%s] %s\n", (long long)now, level, msg);

    if (!g_dlog.use_stdout && g_dlog.file) {
        // Проверка на превышение размера → ротация
        if (g_dlog.current_size + total > g_dlog.max_size) {
            fflush(g_dlog.file);
            daemon_log_rotate();
            daemon_log_reopen();
        }
        if (g_dlog.file) {
            fputs(line, g_dlog.file);
            fflush(g_dlog.file);
            g_dlog.current_size += total;
        }
    }

    // Всегда дублируем в stdout/stderr (systemd journal)
    if (level[0] == 'E' || level[0] == 'W')  // ERROR, WARN
        fputs(line, stderr);
    else
        fputs(line, stdout);
}

// Макросы для удобства
#define DLOG_INFO(fmt, ...)  daemon_log_write("INFO",  fmt, ##__VA_ARGS__)
#define DLOG_WARN(fmt, ...)  daemon_log_write("WARN",  fmt, ##__VA_ARGS__)
#define DLOG_ERROR(fmt, ...) daemon_log_write("ERROR", fmt, ##__VA_ARGS__)

// Закрытие лога
inline void daemon_log_close() {
    if (g_dlog.file) { fflush(g_dlog.file); fclose(g_dlog.file); g_dlog.file = nullptr; }
}
