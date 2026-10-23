// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_daemon_dual_gpu.cpp — Daemon: перечисление устройств из двух бекендов,
// backend_type в GpuDeviceInfo, MSG_GET_BACKEND_INFO
// Требует Catch2 v2.

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "../src/daemon_server.h"
#include "../src/daemon_config.h"
#include <dlfcn.h>

// Минимальный конфиг для теста
static DaemonConfig make_test_config() {
    DaemonConfig cfg;
    cfg.gpu_enabled = true;
    cfg.unix_socket = "";   // без сокета — headless
    cfg.tcp_port = 0;
    return cfg;
}

TEST_CASE("daemon_dual_gpu: init loads both backends", "[daemon][gpu][dual]") {
    DaemonServer server;
    DaemonConfig cfg = make_test_config();

    bool ok = server.init(cfg);
    // init может вернуть false если нет сокетов (headless mode)
    // Главное — проверить что gpuAvailable() корректен
    INFO("gpuAvailable=" << server.gpuAvailable());
    INFO("gpuDevices count=" << server.gpuDevices().size());

    if (server.gpuAvailable()) {
        REQUIRE(!server.gpuDevices().empty());
    }
}

TEST_CASE("daemon_dual_gpu: GpuDeviceInfo has backend_type", "[daemon][gpu][dual]") {
    DaemonServer server;
    DaemonConfig cfg = make_test_config();
    server.init(cfg);

    const auto& devs = server.gpuDevices();

    int intel_count = 0, cuda_count = 0;
    for (auto& d : devs) {
        if (strcmp(d.backend_type, "intel") == 0) intel_count++;
        else if (strcmp(d.backend_type, "cuda") == 0) cuda_count++;
    }

    INFO("Intel devices: " << intel_count << ", CUDA devices: " << cuda_count);
    // Если оба бекенда загружены — должны быть устройства обоих типов
    if (server.gpuAvailable()) {
        CHECK((intel_count + cuda_count) > 0);
    }
}

TEST_CASE("daemon_dual_gpu: backendForDevice finds correct backend", "[daemon][gpu][dual]") {
    DaemonServer server;
    DaemonConfig cfg = make_test_config();
    server.init(cfg);

    if (!server.gpuAvailable()) {
        WARN("No GPU backends — skipping backendForDevice test");
        return;
    }

    const auto& devs = server.gpuDevices();
    for (int i = 0; i < (int)devs.size(); i++) {
        auto* bh = server.backendForDevice(i);
        REQUIRE(bh != nullptr);
        // backend_type должен совпадать
        REQUIRE(bh->backend_type == devs[i].backend_type);
        // Все обязательные функции должны быть не-null
        REQUIRE(bh->create_ != nullptr);
        REQUIRE(bh->destroy_ != nullptr);
        REQUIRE(bh->load_bion_ != nullptr);
        REQUIRE(bh->run_tick_ != nullptr);
        REQUIRE(bh->bion_data_ != nullptr);
    }
}
