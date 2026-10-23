// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#include "daemon_config.h"
#include "daemon_log.h"
#include "common/json_parser.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <sys/stat.h>

using namespace json_parser;

bool DaemonConfig::load(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 65536) { fclose(f); return false; }
    char* buf = new char[sz + 1];
    if (fread(buf, 1, sz, f) != (size_t)sz) { delete[] buf; fclose(f); return false; }
    buf[sz] = '\0';
    fclose(f);

    // Parse top-level sections by finding "SECTION_NAME": {
    auto getSection = [&](const char* section) -> const char* {
        std::string s = "\"";
        s += section;
        s += "\"";
        return strstr(buf, s.c_str());
    };

    // listen
    if (const char* sec = getSection("listen")) {
        unix_socket = get_string(sec, "unix_socket");
        if (unix_socket.empty()) unix_socket = "/tmp/biocore.sock";
        tcp_host = get_string(sec, "tcp_host");
        if (tcp_host.empty()) tcp_host = "127.0.0.1";
        int port_tmp;
        std::string err;
        if (validate_port(sec, "tcp_port", port_tmp, err) && port_tmp > 0)
            tcp_port = (uint16_t)port_tmp;
        else if (port_tmp <= 0)
            tcp_port = 6789;
    }

    // simulation
    if (const char* sec = getSection("simulation")) {
        default_dt = get_float(sec, "default_dt");
        if (default_dt <= 0.0f) default_dt = 0.1f;
        default_mode = (uint8_t)get_int(sec, "default_mode", 0);
        consolidation_interval = (uint32_t)get_int(sec, "consolidation_interval", 10000);
        plugins_path = get_string(sec, "plugins_path");
        if (plugins_path.empty()) plugins_path = "plugins/builtin/";
        gpu_enabled = get_bool(sec, "gpu_enabled", false);  // CPU-only default (GPU — опционально через конфиг)
        {
            int ms;
            std::string err;
            if (validate_int(sec, "max_sessions", ms, err, 1, 256))
                max_sessions = (uint32_t)ms;
            else
                max_sessions = 16;
        }
        // gpu_device_allowlist — JSON массив [0, 1, 2] или пустой (все)
        {
            std::string arr = get_raw(sec, "gpu_device_allowlist");
            if (!arr.empty() && arr[0] == '[') {
                for (const char* p = arr.c_str() + 1; *p; ) {
                    while (*p == ' ' || *p == ',') p++;
                    if (*p == ']') break;
                    if ((*p >= '0' && *p <= '9') || *p == '-') {
                        char* end = nullptr;
                        int val = (int)strtol(p, &end, 10);
                        if (end > p) {
                            gpu_device_allowlist.push_back(val);
                            p = end;
                        } else break;
                    } else break;
                }
            }
        }
    }

    // logging
    if (const char* sec = getSection("logging")) {
        log_file = get_string(sec, "log_file");
        if (log_file.empty()) log_file = "/var/log/biocore.log";
        log_level = get_string(sec, "log_level");
        if (log_level.empty()) log_level = "info";
        // log_max_size_mb: clamp to [1..1024] если значение задано, иначе default 10
        {
            std::string raw = get_string(sec, "log_max_size_mb");
            if (raw.empty() || raw == "null") {
                log_max_size = 10 * 1024 * 1024; // 10MB default
            } else {
                int val = std::atoi(raw.c_str());
                if (val < 1) val = 1;
                if (val > 1024) val = 1024;
                log_max_size = (int64_t)val * 1024 * 1024;
            }
        }
        // log_max_files: clamp to [2..20] если значение задано, иначе default 5
        {
            std::string raw = get_string(sec, "log_max_files");
            if (raw.empty() || raw == "null") {
                log_max_files = 5;
            } else {
                int val = std::atoi(raw.c_str());
                if (val < 2) val = 2;
                if (val > 20) val = 20;
                log_max_files = val;
            }
        }
    }

    delete[] buf;
    return true;
}

std::string DaemonConfig::findConfigPath() {
    // 1. ./biocore.conf
    struct stat st;
    if (stat("./biocore.conf", &st) == 0)
        return "./biocore.conf";

    // 2. ~/.biocore/biocore.conf
    const char* home = getenv("HOME");
    if (home) {
        std::string path = std::string(home) + "/.biocore/biocore.conf";
        if (stat(path.c_str(), &st) == 0)
            return path;
    }

    return ""; // none found, use defaults
}

void DaemonConfig::print() const {
    DLOG_INFO("=== BioNeuronCore Daemon Config ===");
    DLOG_INFO("listen.unix_socket = %s", unix_socket.c_str());
    DLOG_INFO("listen.tcp_host    = %s", tcp_host.c_str());
    DLOG_INFO("listen.tcp_port    = %u", (unsigned)tcp_port);
    DLOG_INFO("simulation.dt      = %.3f", default_dt);
    DLOG_INFO("simulation.mode    = %u", (unsigned)default_mode);
    DLOG_INFO("plugins_path       = %s", plugins_path.c_str());
    DLOG_INFO("gpu_enabled        = %s", gpu_enabled ? "true" : "false");
    DLOG_INFO("log_file           = %s", log_file.empty() ? "(stdout)" : log_file.c_str());
    DLOG_INFO("log_max_size       = %ld MB", (long)(log_max_size / (1024*1024)));
    DLOG_INFO("log_max_files      = %d", log_max_files);
    DLOG_INFO("===================================");
}
