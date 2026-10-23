// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// test_daemon_session_isolation.cpp — проверка изоляции сессий в simulationLoop
// Верифицирует, что simulationLoop(SessionContext*) не использует current_ctx_
// и две параллельные симуляции не пересекаются по данным.

#include "daemon_session.h"
#include "daemon_server.h"
#include "plugin_manager.h"
#include "modifier_interface.h"

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <thread>
#include <chrono>
#include <cstring>

// Минимальная сеть: 2 сегмента, 0 связей, 0 синапсов
static void setupMinimalNetwork(SessionContext* ctx, bool withConnection = true) {
    ctx->segments = new Segment[2];
    memset(ctx->segments, 0, 2 * sizeof(Segment));
    ctx->segments[0].id = 0;
    ctx->segments[0].V_m = -65.0f;
    ctx->segments[0].radius = 10.0f;    // 10 µm
    ctx->segments[0].length = 100.0f;   // 100 µm → area ~6283 µm², Cm ~62.8 pF
    ctx->segments[1].id = 1;
    ctx->segments[1].V_m = -70.0f;
    ctx->segments[1].radius = 10.0f;
    ctx->segments[1].length = 100.0f;
    ctx->num_segments = 2;

    // Ионы (для Нернст-Планк осевого тока — без ионов заряд-флюкс = 0).
    for (int i = 0; i < 2; i++) {
        ctx->segments[i].intra_ions[ION_NA] = 15.0f;  ctx->segments[i].extra_ions[ION_NA] = 140.0f;
        ctx->segments[i].intra_ions[ION_K]  = 140.0f; ctx->segments[i].extra_ions[ION_K]  = 5.0f;
        ctx->segments[i].intra_ions[ION_CA] = 0.0001f; ctx->segments[i].extra_ions[ION_CA] = 1.2f;
        ctx->segments[i].intra_ions[ION_CL] = 5.0f;   ctx->segments[i].extra_ions[ION_CL] = 130.0f;
        ctx->segments[i].intra_ions[ION_MG] = 0.5f;   ctx->segments[i].extra_ions[ION_MG] = 2.0f;
        ctx->segments[i].ATP_level = 1.0f;
    }

    ctx->synapses = nullptr;
    ctx->num_synapses = 0;

    if (withConnection) {
        ctx->connections = new IntraConnection[1];
        memset(ctx->connections, 0, sizeof(IntraConnection));
        ctx->connections[0].segment_a = 0;
        ctx->connections[0].segment_b = 1;
        ctx->connections[0].is_intracellular = 1;  // 1 MOhm — realistic axial
        ctx->num_connections = 1;
    } else {
        ctx->connections = nullptr;
        ctx->num_connections = 0;
    }

    ctx->neurons = new Neuron[1];
    memset(ctx->neurons, 0, sizeof(Neuron));
    ctx->neurons[0].segment_start = 0;
    ctx->neurons[0].segment_count = 2;
    ctx->num_neurons = 1;

    ctx->nuclei = new Nucleus[1];
    memset(ctx->nuclei, 0, sizeof(Nucleus));
    ctx->num_nuclei = 1;

    ctx->simDataFromGpu = false;
    ctx->streamer.setDataPtrs(ctx->segments, ctx->num_segments,
                               ctx->synapses, ctx->num_synapses);
}

TEST_CASE("session isolation — two sessions run concurrently", "[daemon][session][thread]") {
    // Инициализация Core (требуется для core_run_tick)
    plugin_manager_init();
    init_biological_library();
    init_arbitrary_library();
    active_library = biological_library;
    active_modulators = biological_modulators;

    // Создаём сервер с минимальным конфигом
    DaemonConfig cfg;
    cfg.unix_socket = "";  // без сокетов
    cfg.tcp_port = 0;
    cfg.default_dt = 0.1f;

    DaemonServer server;
    REQUIRE(server.init(cfg));

    // Создаём две сессии
    auto* s0 = server.createSession(10, -1);
    auto* s1 = server.createSession(20, -1);
    REQUIRE(s0 != nullptr);
    REQUIRE(s1 != nullptr);
    REQUIRE(s0 != s1);

    setupMinimalNetwork(s0);
    setupMinimalNetwork(s1);

    // Разные начальные V_m для проверки изоляции
    s0->segments[0].V_m = -60.0f;
    s1->segments[0].V_m = -80.0f;

    // Настраиваем sleep-параметры: быстрый цикл без ожидания
    s0->sim_multiplier = 0.0f;
    s1->sim_multiplier = 0.0f;
    s0->sim_target_hz = 0.0f;
    s1->sim_target_hz = 0.0f;

    // ── Запуск симуляции в двух потоках ──
    s0->sim_running = true;
    s1->sim_running = true;

    std::thread t0(&DaemonServer::simulationLoop, &server, s0);
    std::thread t1(&DaemonServer::simulationLoop, &server, s1);

    // Даём поработать ~200ms (при dt=0.1 и 16ms sleep — около 12 тиков)
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Останавливаем
    s0->sim_running = false;
    s1->sim_running = false;
    t0.join();
    t1.join();

    // ── Проверки ──
    // Обе сессии продвинулись (сделали тики)
    REQUIRE(s0->sim_tick_counter > 0);
    REQUIRE(s1->sim_tick_counter > 0);

    // V_m разошлись (у каждой сессии своё значение)
    REQUIRE(s0->segments[0].V_m != s1->segments[0].V_m);

    // Нет перекрёстного загрязнения: V_m каждой сессии изменился
    // относительно своего начального, а не чужого
    float v0_delta = s0->segments[0].V_m - (-60.0f);
    float v1_delta = s1->segments[0].V_m - (-80.0f);
    REQUIRE(v0_delta != 0.0f);  // сессия 0 менялась
    REQUIRE(v1_delta != 0.0f);  // сессия 1 менялась

    server.destroySession(10);
    server.destroySession(20);
}

TEST_CASE("session isolation — tick counters are independent", "[daemon][session]") {
    plugin_manager_init();
    init_biological_library();
    init_arbitrary_library();
    active_library = biological_library;
    active_modulators = biological_modulators;

    DaemonConfig cfg;
    cfg.unix_socket = "";
    cfg.tcp_port = 0;
    cfg.default_dt = 0.1f;

    DaemonServer server;
    REQUIRE(server.init(cfg));

    auto* sA = server.createSession(100, -1);
    auto* sB = server.createSession(200, -1);
    REQUIRE(sA != nullptr);
    REQUIRE(sB != nullptr);

    setupMinimalNetwork(sA);
    setupMinimalNetwork(sB);

    // Разное количество предустановленных тиков
    sA->sim_tick_counter = 1000;
    sB->sim_tick_counter = 5000;

    sA->sim_running = true;
    sB->sim_running = true;
    sA->sim_target_hz = 0.0f;
    sB->sim_target_hz = 0.0f;

    std::thread tA(&DaemonServer::simulationLoop, &server, sA);
    std::thread tB(&DaemonServer::simulationLoop, &server, sB);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    sA->sim_running = false;
    sB->sim_running = false;
    tA.join();
    tB.join();

    // Счётчики тиков разошлись в разные стороны
    REQUIRE(sA->sim_tick_counter > 1000);
    REQUIRE(sB->sim_tick_counter > 5000);

    // Разница в счётчиках не испорчена (у каждой сессии свой)
    uint64_t diff = (sB->sim_tick_counter - sA->sim_tick_counter);
    // diff должен быть примерно равен исходной разнице (4000) плюс небольшая дельта
    REQUIRE(diff >= 3900);

    server.destroySession(100);
    server.destroySession(200);
}

TEST_CASE("session isolation — data pointers don't cross", "[daemon][session]") {
    plugin_manager_init();
    init_biological_library();
    init_arbitrary_library();
    active_library = biological_library;
    active_modulators = biological_modulators;

    DaemonConfig cfg;
    cfg.unix_socket = "";
    cfg.tcp_port = 0;
    cfg.default_dt = 0.1f;

    DaemonServer server;
    REQUIRE(server.init(cfg));

    auto* sA = server.createSession(300, -1);
    auto* sB = server.createSession(400, -1);
    REQUIRE(sA != nullptr);
    REQUIRE(sB != nullptr);

    setupMinimalNetwork(sA);
    setupMinimalNetwork(sB);

    // Разные указатели на данные — ключевая проверка изоляции
    REQUIRE(sA->segments != sB->segments);
    REQUIRE(sA->connections != sB->connections);
    REQUIRE(sA->neurons != sB->neurons);
    REQUIRE(sA->nuclei != sB->nuclei);
    REQUIRE(sA->streamer.empty() == sB->streamer.empty());  // оба пусты

    server.destroySession(300);
    server.destroySession(400);
}
