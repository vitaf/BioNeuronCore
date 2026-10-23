// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#pragma once
// daemon_config.h — JSON config parser for biocore.conf.
#include <string>
#include <cstdint>
#include <vector>

struct DaemonConfig {
    // listen
    std::string unix_socket = "/tmp/biocore.sock";
    std::string tcp_host    = "127.0.0.1";
    uint16_t    tcp_port    = 0;  // 0=disabled (use Unix socket). Override in config for production

    // simulation
    float    default_dt           = 0.1f;
    uint8_t  default_mode         = 0;   // 0=BIOLOGICAL, 1=ARBITRARY
    uint32_t consolidation_interval = 10000;
    std::string plugins_path      = "plugins/builtin/";
    bool     gpu_enabled          = false;  // CPU-only by default, GPU via config

    // multi-GPU / multi-session
    uint32_t max_sessions          = 16;     // макс. одновременных сессий
    std::vector<int> gpu_device_allowlist;   // пустой = все доступные GPU

    // logging
    std::string log_file        = "/var/log/biocore.log";
    std::string log_level       = "info";
    int64_t     log_max_size    = 10 * 1024 * 1024;  // 10 MB
    int         log_max_files   = 5;

    // Load from JSON file. Returns true on success.
    bool load(const char* path);

    // Resolve default config path: try ./biocore.conf, then ~/.biocore/biocore.conf
    static std::string findConfigPath();

    // Print to stdout
    void print() const;
};
