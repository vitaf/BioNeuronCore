// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

// daemon_main.cpp — entry point for BioNeuronCore standalone daemon process.
#include "daemon_config.h"
#include "daemon_server.h"
#include "daemon_session.h"
#include "daemon_log.h"
#include "types.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include "kernel_dispatch.h"
#include "bion_loader.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <unistd.h>
#include <sys/stat.h>
#include <fstream>
#include <sstream>
#include <malloc.h>

// Forward declaration из simulation_host.cpp
extern "C" void core_run_tick_ctx(struct SessionContext* ctx,
                                   Segment*, uint32_t, IntraConnection*, uint32_t,
                                   Synapse*, uint32_t, uint8_t, uint64_t*, float,
                                   CurrentStimulus*, uint32_t*, VoltageClamp*, uint32_t*,
                                   RampStimulus*, uint32_t*, IonInjection*, uint32_t*,
                                   LigandInjection*, uint32_t*);

static DaemonServer* g_server = nullptr;
static bool g_batch_mode = false;
static std::string g_batch_checkpoint_path;  // путь для аварийного чекпоинта

static void signalHandler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        if (g_batch_mode && !g_batch_checkpoint_path.empty()) {
            // Batch mode: сохранить чекпоинт и выйти с кодом 75 (TEMPORARY_FAILURE)
            DLOG_WARN("[Batch] Signal %d — saving emergency checkpoint to %s",
                    sig, g_batch_checkpoint_path.c_str());
            // BionData уже в runBatchMode, не можем сохранить отсюда напрямую.
            // Устанавливаем флаг, runBatchMode проверит его при следующей итерации.
            g_batch_mode = false;  // флаг "остановиться"
        } else if (g_server) {
            DLOG_INFO("Signal %d received, shutting down...", sig);
            g_server->requestShutdown();
        }
    }
}

static void printUsage(const char* prog) {
    printf("Usage: %s [OPTIONS]\n", prog);
    printf("BioNeuronCore Daemon — standalone simulation server.\n\n");
    printf("Options:\n");
    printf("  -c, --config PATH       Path to biocore.conf (default: auto-detect)\n");
    printf("  -p, --plugins-path PATH Override plugins directory from config\n");
    printf("  --ready-fd N            Signal readiness on fd N (for tests)\n");
    printf("  -h, --help              Show this help\n");
    printf("  --version               Print version and exit\n");
    printf("\nBatch mode (no server, run simulation and exit):\n");
    printf("  --batch                 Enable batch (headless) execution mode\n");
    printf("  --load-bion PATH        BION file to load\n");
    printf("  --load-bexp PATH        .bexp experiment recipe to load\n");
    printf("  --ticks N               Number of ticks to simulate\n");
    printf("  --task-id N             Task identifier (for logging)\n");
    printf("  --bexp-output PATH      Output .bexp file path\n");
    printf("  --params-json PATH      JSON file with parameter overrides\n");
    printf("  --seed N                Random seed for reproducibility\n");
    printf("  --checkpoint-interval N Save checkpoint every N ticks (default 0 = off)\n");
    printf("  --resume-from PATH      Resume from a saved BION checkpoint\n");
    printf("  --gpu-device N          Select GPU device index (0=first Intel, 0=first CUDA)\n");
}

// Применение параметров из JSON к загруженной сети.
// Формат JSON: {"g_Na": 120.0, "g_K": 36.0, ...} или {"type_id": value, ...}
static void apply_param_overrides(Segment* segments, uint32_t num_segments,
                                   const std::string& params_json) {
    if (params_json.empty() || !segments) return;

    const char* p = params_json.c_str();
    while (*p) {
        // Пропуск пробелов и разделителей
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' ||
               *p == '{' || *p == '}' || *p == ',')) p++;
        if (!*p) break;

        // Чтение ключа (строка в кавычках)
        std::string key;
        if (*p == '"') {
            p++;
            while (*p && *p != '"') { key += *p; p++; }
            if (*p == '"') p++;
        } else {
            // Числовой ключ
            while (*p && *p >= '0' && *p <= '9') { key += *p; p++; }
        }
        if (key.empty()) break;

        // Пропуск ':'
        while (*p && (*p == ' ' || *p == '\t' || *p == ':')) p++;
        if (!*p) break;

        // Чтение значения (число float)
        char* end = nullptr;
        float val = strtof(p, &end);
        if (end == p) break;
        p = end;

        // Найти type ID: сначала пробуем числовой ключ
        uint8_t type_id = 0;
        bool found = false;
        if (key[0] >= '0' && key[0] <= '9') {
            type_id = (uint8_t)atoi(key.c_str());
            found = (type_id < MOD_TYPE_COUNT);
        } else {
            // Ищем по имени модификатора через PluginManager
            for (int tid = 0; tid < MOD_TYPE_COUNT; tid++) {
                const char* mname = plugin_modifier_name((uint8_t)tid);
                if (!mname || !mname[0]) continue;
                // Частичное совпадение (регистронезависимое)
                const char* n = mname;
                const char* k = key.c_str();
                while (*n && *k) {
                    char cn = (*n >= 'A' && *n <= 'Z') ? (*n - 'A' + 'a') : *n;
                    char ck = (*k >= 'A' && *k <= 'Z') ? (*k - 'A' + 'a') : *k;
                    if (cn != ck) break;
                    n++; k++;
                }
                if (*k == 0) {
                    type_id = (uint8_t)tid;
                    found = true;
                    break;
                }
            }
        }

        if (!found || type_id >= MOD_TYPE_COUNT) continue;

        // Применить параметр ко всем сегментам, имеющим модификатор этого типа
        for (uint32_t si = 0; si < num_segments; si++) {
            for (int mi = 0; mi < MAX_MODIFIERS_PER_SEGMENT; mi++) {
                if (segments[si].modifiers[mi].type == type_id &&
                    segments[si].modifiers[mi].type != 0) {
                    float old_val = segments[si].modifiers[mi].state[0];
                    if (old_val > 0.0f) {
                        segments[si].modifiers[mi].state[0] = val;
                    } else {
                        segments[si].modifiers[mi].state[0] = val;
                    }
                    break;
                }
            }
        }
    }
}

// ── Batch mode: загрузить BION, применить параметры, считать N тиков, записать .bexp ──
static int runBatchMode(const char* bion_path, uint32_t ticks, uint32_t task_id,
                         const char* bexp_output, const char* params_json_path,
                         uint32_t seed, uint32_t checkpoint_interval,
                         const char* resume_from, const DaemonConfig& cfg) {
    DLOG_INFO("[Batch] Task %u: loading %s, %u ticks, seed %u, ckpt=%u, resume=%s",
           task_id, bion_path, ticks, seed, checkpoint_interval,
           resume_from ? resume_from : "none");

    // Инициализация плагинов
    plugin_manager_init();
    std::string pp = cfg.plugins_path.empty() ? "plugins/builtin/" : cfg.plugins_path;
    int n_loaded = (int)plugin_load_directory(pp.c_str());
    DLOG_INFO("[Batch] Loaded %d plugins from %s", n_loaded, pp.c_str());
    init_biological_library();

    // Загрузка BION (или возобновление из чекпоинта)
    BionData bd;
    const char* load_path = (resume_from && resume_from[0]) ? resume_from : bion_path;
    int ret = bion_load_host(load_path, &bd);
    if (ret != 0) {
        DLOG_ERROR("[Batch] FATAL: failed to load BION file %s (error %d)",
                load_path, ret);
        return 1;
    }
    DLOG_INFO("[Batch] Loaded: %u segments, %u synapses, %u connections, %u neurons",
           bd.num_segments, bd.num_synapses, bd.num_connections, bd.num_neurons);

    // Применение параметров
    if (params_json_path && params_json_path[0]) {
        std::ifstream pf(params_json_path);
        if (pf) {
            std::stringstream buf;
            buf << pf.rdbuf();
            std::string params = buf.str();
            apply_param_overrides(bd.segments, bd.num_segments, params);
            DLOG_INFO("[Batch] Applied parameter overrides from %s", params_json_path);
        } else {
            DLOG_ERROR("[Batch] WARN: cannot read params JSON %s", params_json_path);
        }
    }

    // Установка seed для воспроизводимости: модифицируем release_probability
    // каждой синапса на детерминированную вариацию от seed
    if (seed != 0) {
        for (uint32_t si = 0; si < bd.num_synapses; si++) {
            // Детерминированный хеш от seed и id синапса для вариации release
            uint32_t hash = seed ^ (bd.synapses[si].id * 2654435761u);
            float variation = (float)(hash & 0xFFFF) / 65536.0f * 0.02f; // ±1%
            bd.synapses[si].release_probability *= (1.0f + variation);
        }
    }

    // Запуск симуляции: CPU-путь через SessionContext (core_run_tick_ctx)
    SessionContext ctx;
    ctx.session_id = 0;
    ctx.segments = bd.segments;
    ctx.synapses = bd.synapses;
    ctx.connections = bd.connections;
    ctx.neurons = bd.neurons;
    ctx.nuclei = bd.nuclei;
    ctx.num_segments = bd.num_segments;
    ctx.num_synapses = bd.num_synapses;
    ctx.num_connections = bd.num_connections;
    ctx.num_neurons = bd.num_neurons;
    ctx.num_nuclei = bd.num_nuclei;
    ctx.active_library = biological_library;
    ctx.active_modulators = biological_modulators;

    const float dt = 0.1f; // 0.1ms default
    time_t start_time = time(nullptr);

    uint64_t tick_counter = 0;
    // Включаем аварийный чекпоинт
    g_batch_mode = true;
    g_batch_checkpoint_path = std::string(bexp_output) + ".emergency.bion";

    DLOG_INFO("[Batch] Starting simulation: %u ticks at dt=%.2fms...", ticks, dt);
    for (uint32_t tick = 0; tick < ticks; tick++) {
        // Проверка graceful shutdown
        if (!g_batch_mode) {
            // Сохраняем аварийный чекпоинт
            BionData ckpt_bd;
            ckpt_bd.segments = bd.segments;
            ckpt_bd.synapses = bd.synapses;
            ckpt_bd.connections = bd.connections;
            ckpt_bd.neurons = bd.neurons;
            ckpt_bd.nuclei = bd.nuclei;
            ckpt_bd.num_segments = bd.num_segments;
            ckpt_bd.num_synapses = bd.num_synapses;
            ckpt_bd.num_connections = bd.num_connections;
            ckpt_bd.num_neurons = bd.num_neurons;
            ckpt_bd.num_nuclei = bd.num_nuclei;
            ckpt_bd.header = bd.header;
            bion_save(g_batch_checkpoint_path.c_str(), &ckpt_bd);
            DLOG_INFO("[Batch] Emergency checkpoint saved: %s", g_batch_checkpoint_path.c_str());
            bion_unload_host(&bd);
            exit(75);  // EX_TEMPFAIL — временная ошибка, можно перезапустить
        }
        core_run_tick_ctx(&ctx,
                          ctx.segments, ctx.num_segments,
                          ctx.connections, ctx.num_connections,
                          ctx.synapses, ctx.num_synapses,
                          0, &tick_counter, dt,
                          nullptr, nullptr, nullptr, nullptr,
                          nullptr, nullptr, nullptr, nullptr,
                          nullptr, nullptr);
        tick_counter++;

        // Авто-чекпоинт каждые N тиков
        if (checkpoint_interval > 0 && (tick + 1) % checkpoint_interval == 0) {
            char ckpt_path[512];
            snprintf(ckpt_path, sizeof(ckpt_path), "%s.ckpt_%u.bion",
                     bexp_output, tick + 1);
            BionData ckpt_bd;
            ckpt_bd.segments = bd.segments;
            ckpt_bd.synapses = bd.synapses;
            ckpt_bd.connections = bd.connections;
            ckpt_bd.neurons = bd.neurons;
            ckpt_bd.nuclei = bd.nuclei;
            ckpt_bd.num_segments = bd.num_segments;
            ckpt_bd.num_synapses = bd.num_synapses;
            ckpt_bd.num_connections = bd.num_connections;
            ckpt_bd.num_neurons = bd.num_neurons;
            ckpt_bd.num_nuclei = bd.num_nuclei;
            ckpt_bd.header = bd.header;
            if (bion_save(ckpt_path, &ckpt_bd) == 0) {
                DLOG_INFO("[Batch] Checkpoint saved: %s (tick %u)", ckpt_path, tick + 1);
            }
        }

        if ((tick + 1) % 10000 == 0) {
            DLOG_INFO("[Batch] Progress: %u/%u ticks (%.1f%%)",
                   tick + 1, ticks, 100.0f * (tick + 1) / ticks);
        }
    }

    time_t end_time = time(nullptr);
    DLOG_INFO("[Batch] Simulation complete: %u ticks in %ld seconds",
           ticks, (long)(end_time - start_time));

    // Запись .bexp (BION Experiment — BION + experiment metadata)
    std::ofstream of(bexp_output, std::ios::binary);
    if (!of) {
        DLOG_ERROR("[Batch] FATAL: cannot write bexp output %s", bexp_output);
        return 1;
    }

    // Формат .bexp: сигнатура "BEXP" + version(4) + bion_data + metadata
    const char sig[4] = {'B', 'E', 'X', 'P'};
    of.write(sig, 4);
    uint32_t bexp_version = 1;
    of.write((const char*)&bexp_version, 4);

    // Запись BION-данных (сегменты, синапсы, connections, нейроны, ядра)
    uint32_t net_ver = bd.header.version;
    of.write((const char*)&net_ver, 4);
    of.write((const char*)&bd.num_segments, 4);
    of.write((const char*)bd.segments, bd.num_segments * sizeof(Segment));
    of.write((const char*)&bd.num_synapses, 4);
    of.write((const char*)bd.synapses, bd.num_synapses * sizeof(Synapse));
    of.write((const char*)&bd.num_connections, 4);
    of.write((const char*)bd.connections, bd.num_connections * sizeof(IntraConnection));
    of.write((const char*)&bd.num_neurons, 4);
    of.write((const char*)bd.neurons, bd.num_neurons * sizeof(Neuron));
    of.write((const char*)&bd.num_nuclei, 4);
    of.write((const char*)bd.nuclei, bd.num_nuclei * sizeof(Nucleus));

    // Метаданные: ticks, task_id, seed, timestamp
    uint64_t tick_count = ticks;
    uint32_t tsk_id = task_id;
    uint32_t sd = seed;
    uint64_t ts = (uint64_t)time(nullptr);
    of.write((const char*)&tick_count, 8);
    of.write((const char*)&tsk_id, 4);
    of.write((const char*)&sd, 4);
    of.write((const char*)&ts, 8);

    // Метрики: средний V_m по сегментам
    float sum_vm = 0.0f;
    for (uint32_t si = 0; si < bd.num_segments; si++) {
        sum_vm += bd.segments[si].V_m;
    }
    float mean_vm = bd.num_segments > 0 ? sum_vm / bd.num_segments : 0.0f;
    of.write((const char*)&mean_vm, 4);

    // Подсчёт спайков (V_m > -20mV)
    uint32_t spike_segs = 0;
    for (uint32_t si = 0; si < bd.num_segments; si++) {
        if (bd.segments[si].V_m > -20.0f) spike_segs++;
    }
    of.write((const char*)&spike_segs, 4);

    of.close();

    DLOG_INFO("[Batch] BEXP written: %s (%.1f KB, mean V_m=%.2f mV, %u spiking segments)",
           bexp_output, (double)of.tellp() / 1024.0, mean_vm, spike_segs);

    g_batch_mode = false;
    g_batch_checkpoint_path.clear();

    // Очистка через штатный bion_unload_host
    bion_unload_host(&bd);

    return 0;
}

int main(int argc, char** argv) {
    // ── Настройка glibc-аллокатора ДО любых аллокаций ──────────────────
    // Демон Core работает самостоятельно (без Studio), поэтому mallopt
    // должен быть здесь. Intel iGPU использует общую системную память —
    // фрагментация malloc напрямую вызывает рост RSS.
    mallopt(M_ARENA_MAX, 2);               // макс. 2 арены (не 8×cores)
    mallopt(M_TRIM_THRESHOLD, 128*1024);   // возвращать память агрессивнее

    std::string config_path;
    std::string plugins_path_override;
    int ready_fd = -1;

    // ── Batch mode args ──
    bool batch_mode = false;
    std::string batch_bion_path;
    std::string batch_bexp_path;
    uint32_t batch_ticks = 0;
    uint32_t batch_task_id = 0;
    std::string batch_bexp_output;
    std::string batch_params_json;
    uint32_t batch_seed = 0;
    uint32_t batch_checkpoint_interval = 0;
    std::string batch_resume_from;
    int gpu_device_index = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--batch") == 0) {
            batch_mode = true;
        } else if (strcmp(argv[i], "--load-bion") == 0) {
            if (i + 1 < argc) batch_bion_path = argv[++i];
        } else if (strcmp(argv[i], "--load-bexp") == 0) {
            if (i + 1 < argc) batch_bexp_path = argv[++i];
        } else if (strcmp(argv[i], "--ticks") == 0) {
            if (i + 1 < argc) batch_ticks = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--task-id") == 0) {
            if (i + 1 < argc) batch_task_id = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--bexp-output") == 0) {
            if (i + 1 < argc) batch_bexp_output = argv[++i];
        } else if (strcmp(argv[i], "--params-json") == 0) {
            if (i + 1 < argc) batch_params_json = argv[++i];
        } else if (strcmp(argv[i], "--seed") == 0) {
            if (i + 1 < argc) batch_seed = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--checkpoint-interval") == 0) {
            if (i + 1 < argc) batch_checkpoint_interval = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--resume-from") == 0) {
            if (i + 1 < argc) batch_resume_from = argv[++i];
        } else if (strcmp(argv[i], "--gpu-device") == 0) {
            if (i + 1 < argc) gpu_device_index = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
            if (i + 1 < argc) config_path = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--plugins-path") == 0) {
            if (i + 1 < argc) plugins_path_override = argv[++i];
        } else if (strcmp(argv[i], "--ready-fd") == 0) {
            if (i + 1 < argc) ready_fd = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--version") == 0) {
            printf("BioNeuronCore Daemon v0.1.0 (biocore-daemon)\n");
            return 0;
        }
    }

    // Load config
    DaemonConfig cfg;
    if (config_path.empty()) config_path = DaemonConfig::findConfigPath();
    if (!config_path.empty()) {
        if (cfg.load(config_path.c_str())) {
            daemon_log_init(cfg.log_file.c_str(), cfg.log_max_size, cfg.log_max_files);
            DLOG_INFO("Config loaded from %s", config_path.c_str());
        } else {
            daemon_log_init(cfg.log_file.c_str(), cfg.log_max_size, cfg.log_max_files);
            DLOG_WARN("Failed to parse config %s, using defaults", config_path.c_str());
        }
    } else {
        daemon_log_init(cfg.log_file.c_str(), cfg.log_max_size, cfg.log_max_files);
        DLOG_INFO("No config file found, using defaults");
    }

    if (!plugins_path_override.empty()) {
        cfg.plugins_path = plugins_path_override;
    }

    // ── Batch mode: загрузить → считать → записать → выйти ──
    if (batch_mode) {
        // Если загружается .bexp — извлекаем bion_file_path из JSON-заголовка
        if (batch_bion_path.empty() && !batch_bexp_path.empty()) {
            FILE* bf = fopen(batch_bexp_path.c_str(), "rb");
            if (bf) {
                char sig[5] = {0};
                if (fread(sig, 1, 4, bf) != 4) { fclose(bf); bf = nullptr; }
                if (bf && memcmp(sig, "BEXP", 4) == 0) {
                    uint32_t ver = 0;
                    if (fread(&ver, 4, 1, bf) != 1) { fclose(bf); bf = nullptr; }
                    uint32_t jlen = 0;
                    if (bf && fread(&jlen, 4, 1, bf) != 1) { fclose(bf); bf = nullptr; }
                    std::string json(jlen, '\0');
                    if (bf && fread(&json[0], 1, jlen, bf) != jlen) { fclose(bf); bf = nullptr; }
                    if (bf) {
                        auto pos = json.find("\"bion_file_path\"");
                        if (pos != std::string::npos) {
                            auto vs = json.find("\"", pos + 17);
                            if (vs != std::string::npos) {
                                vs++;
                                auto ve = json.find("\"", vs);
                                if (ve != std::string::npos)
                                    batch_bion_path = json.substr(vs, ve - vs);
                            }
                        }
                    }
                }
                if (bf) fclose(bf);
            }
        }

        if (batch_bion_path.empty()) {
            DLOG_ERROR("Batch mode requires --load-bion PATH or --load-bexp PATH");
            daemon_log_close();
            return 1;
        }
        if (batch_ticks == 0) {
            DLOG_ERROR("Batch mode requires --ticks N (>0)");
            daemon_log_close();
            return 1;
        }
        if (batch_bexp_output.empty()) {
            DLOG_ERROR("Batch mode requires --bexp-output PATH");
            daemon_log_close();
            return 1;
        }
        if (gpu_device_index >= 0) {
            printf("[Batch] GPU device: %d (env BIOCORE_GPU_DEVICE=%s)\n",
                   gpu_device_index, getenv("BIOCORE_GPU_DEVICE") ? getenv("BIOCORE_GPU_DEVICE") : "not set");
        }
        int ret = runBatchMode(batch_bion_path.c_str(), batch_ticks, batch_task_id,
                                batch_bexp_output.c_str(),
                                batch_params_json.empty() ? nullptr : batch_params_json.c_str(),
                                batch_seed, batch_checkpoint_interval,
                                batch_resume_from.empty() ? nullptr : batch_resume_from.c_str(),
                                cfg);
        daemon_log_close();
        return ret;
    }

    cfg.print();

    // Setup signal handlers
    signal(SIGINT,  signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGPIPE, SIG_IGN);

    // Create and init server
    DaemonServer server;
    g_server = &server;
    if (!server.init(cfg)) {
        DLOG_ERROR("FATAL: server init failed");
        daemon_log_close();
        return 1;
    }

    if (ready_fd >= 0) {
        char ready = 1;
        (void)write(ready_fd, &ready, 1);
        close(ready_fd);
    }

    server.run();
    g_server = nullptr;
    daemon_log_close();
    return 0;
}
