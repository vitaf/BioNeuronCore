// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#include "daemon_server.h"
#include "daemon_log.h"
#include "bion_loader.h"
#include "modifier_interface.h"
#include "plugin_manager.h"
#include "consolidation.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <malloc.h>
#include <thread>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <dlfcn.h>
#include <sys/uio.h>

static constexpr int MAX_EVENTS   = 64;
static constexpr int EPOLL_TIMEOUT_MS = 100;

// ── Core internal API (from headers, not the Studio C bridge) ───────────
// core_run_tick is the only extern "C" — from simulation_host.cpp
extern "C" void core_run_tick(Segment*, uint32_t, IntraConnection*, uint32_t,
                               Synapse*, uint32_t, uint8_t, uint64_t*, float,
                               CurrentStimulus*, uint32_t*, VoltageClamp*, uint32_t*,
                               RampStimulus*, uint32_t*, IonInjection*, uint32_t*,
                               LigandInjection*, uint32_t*, MechanoStimulus*, uint32_t*);
extern "C" void core_run_tick_ctx(struct SessionContext*,
                               Segment*, uint32_t, IntraConnection*, uint32_t,
                               Synapse*, uint32_t, uint8_t, uint64_t*, float,
                               CurrentStimulus*, uint32_t*, VoltageClamp*, uint32_t*,
                               RampStimulus*, uint32_t*, IonInjection*, uint32_t*,
                               LigandInjection*, uint32_t*);
extern "C" void core_init_consolidation_ctx(struct SessionContext*, Neuron*, uint32_t, Nucleus*, uint32_t);
extern "C" void core_start_consolidation_ctx(struct SessionContext*, Segment*, uint32_t, Synapse*, uint32_t*, uint32_t, uint64_t*);
extern "C" void core_stop_consolidation_ctx(struct SessionContext*);
extern "C" void core_compute_metrics(Segment*, uint32_t, Neuron*, uint32_t, uint64_t, char*, size_t);
extern "C" const char* core_version_string();
extern "C" uint32_t core_version_major();
extern "C" uint32_t core_version_minor();
extern "C" uint32_t core_version_patch();

// Consolidation (extern "C" from simulation_host.cpp)
extern "C" void core_init_consolidation(Neuron*, uint32_t, Nucleus*, uint32_t);
extern "C" void core_start_consolidation(Segment*, uint32_t, Synapse*, uint32_t*, uint32_t, uint64_t*);
extern "C" void core_stop_consolidation();
extern "C" void core_set_consolidation_interval(uint32_t);
extern "C" uint32_t core_get_consolidation_interval();
extern "C" int core_get_pending_consolidation_events();
extern "C" int core_get_nucleus_snapshot(uint32_t, Nucleus*);

// Plugin manager — use C++ headers directly
#include "plugin_manager.h"
#include "modifier_interface.h"

// ==================== HELPERS ====================

static bool setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}

// Гарантированное чтение/запись N байт (цикл до полной доставки)
// Гарантированное чтение/запись N байт (цикл до полной доставки)
[[maybe_unused]] static bool readExact(int fd, void* buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = read(fd, (char*)buf + off, len - off);
        if (n <= 0) return false;
        off += (size_t)n;
    }
    return true;
}

[[maybe_unused]] static bool writeExact(int fd, const void* buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = write(fd, (const char*)buf + off, len - off);
        if (n <= 0) return false;
        off += (size_t)n;
    }
    return true;
}

// ==================== CONSTRUCTION ====================

DaemonServer::DaemonServer() {
}

DaemonServer::~DaemonServer() {
    shutdown_ = true;
    // Остановить все сессии
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& [id, ctx] : sessions_) {
            if (ctx->sim_thread.joinable()) {
                ctx->shutdown = true;
                ctx->sim_thread.join();
            }
            ctx->freeNetwork();
        }
        sessions_.clear();
    }
    default_session_ = nullptr;
    for (auto& bh : gpu_backends_) { if (bh.lib_handle) dlclose(bh.lib_handle); }
    gpu_backends_.clear();
    if (unix_fd_ >= 0) { close(unix_fd_); unlink(config_.unix_socket.c_str()); }
    if (tcp_fd_  >= 0) close(tcp_fd_);
    if (epoll_fd_ >= 0) close(epoll_fd_);
}

// ==================== INIT ====================

bool DaemonServer::init(const DaemonConfig& cfg) {
    config_ = cfg;

    // Init Core libraries
    plugin_manager_init();
    init_biological_library();
    init_arbitrary_library();
    active_library = biological_library;
    active_modulators = biological_modulators;

    // Load plugins
    if (!cfg.plugins_path.empty()) {
        plugin_load_directory(cfg.plugins_path.c_str());
        plugin_load_info_directory(cfg.plugins_path.c_str());
        DLOG_INFO("Loaded %d plugins from %s", plugin_get_count(), cfg.plugins_path.c_str());
    }

    // Create epoll
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) { perror("epoll_create1"); return false; }

    // Create Unix socket listener
    if (!cfg.unix_socket.empty()) {
        unlink(cfg.unix_socket.c_str());
        unix_fd_ = createUnixListener(cfg.unix_socket.c_str());
        if (unix_fd_ >= 0) {
            setNonBlocking(unix_fd_);
            struct epoll_event ev;
            ev.events = EPOLLIN;
            ev.data.fd = unix_fd_;
            epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, unix_fd_, &ev);
            DLOG_INFO(" Unix socket listening on %s", cfg.unix_socket.c_str());
        }
    }

    // Create TCP listener (if port != 0)
    if (cfg.tcp_port != 0) {
        tcp_fd_ = createTcpListener(cfg.tcp_host.c_str(), cfg.tcp_port);
        if (tcp_fd_ >= 0) {
            setNonBlocking(tcp_fd_);
            struct epoll_event ev;
            ev.events = EPOLLIN;
            ev.data.fd = tcp_fd_;
            epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, tcp_fd_, &ev);
            DLOG_INFO(" TCP listening on %s:%u", cfg.tcp_host.c_str(), (unsigned)cfg.tcp_port);
        }
    }

    if (unix_fd_ < 0 && tcp_fd_ < 0) {
        DLOG_INFO("No network listeners configured (headless/test mode)");
    }

    // GPU backend (если включен в конфиге)
    if (cfg.gpu_enabled) {
        const char* search_paths[] = {".", "/usr/lib/biocore", ""};

        // Загружаем оба .so независимо
        auto load_backend = [&](const char* so_name, const char* type_label) -> bool {
            for (int i = 0; i < 3; i++) {
                char full[640];
                if (search_paths[i][0])
                    snprintf(full, sizeof(full), "%s/%s", search_paths[i], so_name);
                else
                    snprintf(full, sizeof(full), "%s", so_name);
                void* h = dlopen(full, RTLD_NOW);
                if (!h) continue;
                GpuBackendHandle bh;
                bh.lib_handle = h;
                bh.backend_type = type_label;
                bh.create_    = (decltype(bh.create_))   dlsym(h, "gpu_backend_create");
                bh.destroy_   = (decltype(bh.destroy_))  dlsym(h, "gpu_backend_destroy");
                bh.load_bion_ = (decltype(bh.load_bion_))dlsym(h, "gpu_backend_load_bion");
                bh.run_tick_  = (decltype(bh.run_tick_)) dlsym(h, "gpu_backend_run_tick");
                bh.bion_data_ = (decltype(bh.bion_data_))dlsym(h, "gpu_backend_bion_data");
                bh.create_on_device_ = (decltype(bh.create_on_device_)) dlsym(h, "gpu_backend_create_on_device");
                bh.device_count_     = (decltype(bh.device_count_))     dlsym(h, "gpu_backend_device_count");
                bh.get_device_info_  = (decltype(bh.get_device_info_))  dlsym(h, "gpu_backend_get_device_info");
                if (bh.create_ && bh.destroy_ && bh.load_bion_ && bh.run_tick_ && bh.bion_data_) {
                    gpu_backends_.push_back(std::move(bh));
                    DLOG_INFO("GPU backend loaded: %s (%s)", full, type_label);
                    // Перечислить устройства этого бекенда
                    auto& ref = gpu_backends_.back();
                    if (ref.device_count_ && ref.get_device_info_) {
                        int ndev = ref.device_count_();
                        for (int d = 0; d < ndev; d++) {
                            GpuDeviceInfo info;
                            if (ref.get_device_info_(d, &info) == 0) {
                                info.device_index = (int)gpu_devices_.size();  // глобальный индекс
                                gpu_devices_.push_back(info);
                                DLOG_INFO("  GPU[%d]: %s (%s, vendor=0x%x, mem=%llu MB, cu=%u)",
                                    info.device_index, info.name, type_label, info.vendor_id,
                                    (unsigned long long)info.global_mem_bytes / (1024*1024),
                                    info.max_compute_units);
                            }
                        }
                    } else if (ref.create_) {
                        GpuDeviceInfo info;
                        memset(&info, 0, sizeof(info));
                        snprintf(info.name, 127, "%s GPU", type_label);
                        info.device_index = (int)gpu_devices_.size();
                        strncpy(info.backend_type, type_label, 15);
                        gpu_devices_.push_back(info);
                        DLOG_INFO("  GPU: %s .so, single device assumed", type_label);
                    }
                    return true;
                }
                dlclose(h);
            }
            return false;
        };

        load_backend("libbiocore_gpu_backend_intel.so", "intel");
        load_backend("libbiocore_gpu_backend_cuda.so", "cuda");

        if (gpu_backends_.empty()) DLOG_INFO("GPU backend not found, CPU only");
    }

    // Создать сессию по умолчанию (session 0)
    initDefaultSession();

    return true;
}

const DaemonServer::GpuBackendHandle* DaemonServer::backendForDevice(int global_device_index) const {
    if (global_device_index < 0 || global_device_index >= (int)gpu_devices_.size()) return nullptr;
    const char* wanted = gpu_devices_[global_device_index].backend_type;
    for (auto& bh : gpu_backends_) {
        if (bh.backend_type == wanted) return &bh;
    }
    return nullptr;
}

void DaemonServer::initDefaultSession() {
    auto ctx = std::make_unique<SessionContext>();
    ctx->session_id = 0;
    ctx->server = this;
    memset(ctx->stimuli, 0, sizeof(ctx->stimuli));
    memset(ctx->vclamps, 0, sizeof(ctx->vclamps));
    memset(ctx->ramps,   0, sizeof(ctx->ramps));
    memset(ctx->ion_inj, 0, sizeof(ctx->ion_inj));
    memset(ctx->lig_inj, 0, sizeof(ctx->lig_inj));
    default_session_ = ctx.get();
    current_ctx_ = ctx.get();  // текущий контекст по умолчанию
    sessions_[0] = std::move(ctx);
    DLOG_INFO("Default session 0 created");
}

int DaemonServer::createUnixListener(const char* path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("unix socket"); return -1; }
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("unix bind"); close(fd); return -1;
    }
    if (listen(fd, 16) < 0) { perror("unix listen"); close(fd); return -1; }
    chmod(path, 0666);
    return fd;
}

int DaemonServer::createTcpListener(const char* host, uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("tcp socket"); return -1; }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (strcmp(host, "0.0.0.0") == 0 || host[0] == '\0') {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        struct hostent* he = gethostbyname(host);
        if (!he) { DLOG_ERROR("[daemon] gethostbyname failed for %s", host); close(fd); return -1; }
        memcpy(&addr.sin_addr, he->h_addr, he->h_length);
    }

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("tcp bind"); close(fd); return -1;
    }
    if (listen(fd, 16) < 0) { perror("tcp listen"); close(fd); return -1; }
    return fd;
}

// ==================== CLIENT MANAGEMENT ====================

void DaemonServer::acceptClient(int listen_fd) {
    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    int fd = accept(listen_fd, (struct sockaddr*)&addr, &addrlen);
    if (fd < 0) return;

    setNonBlocking(fd);
    int opt = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET; // edge-triggered
    ev.data.fd = fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);

    ClientConnection cc;
    cc.fd = fd;

    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_.push_back(std::move(cc));
    }

    char remote_str[128] = "(unknown)";
    if (addr.ss_family == AF_INET) {
        struct sockaddr_in* sin = (struct sockaddr_in*)&addr;
        inet_ntop(AF_INET, &sin->sin_addr, remote_str, sizeof(remote_str));
    } else if (addr.ss_family == AF_UNIX) {
        strcpy(remote_str, "unix");
    }
    DLOG_INFO(" Client connected fd=%d from %s", fd, remote_str);
}

void DaemonServer::removeClient(int fd) {
    // Удалить fd из streamer-ов ВСЕХ сессий (не только текущей)
    for (auto& [id, ctx] : sessions_) {
        if (ctx) ctx->streamer.removeSubscriber(fd);
    }
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_.erase(
            std::remove_if(clients_.begin(), clients_.end(),
                           [fd](const ClientConnection& c) { return c.fd == fd; }),
            clients_.end());
    }
    // Очистить привязку клиента к сессии
    {
        std::lock_guard<std::mutex> lock(client_session_mutex_);
        client_session_map_.erase(fd);
    }
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
    DLOG_INFO(" Client disconnected fd=%d", fd);
}

// ==================== PROTOCOL DISPATCH ====================

void DaemonServer::processClientData(int fd) {
    ClientConnection* cc = nullptr;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto& c : clients_) {
            if (c.fd == fd) { cc = &c; break; }
        }
    }
    if (!cc) return;

    // Read as much as available into recv_buf
    uint8_t tmp[65536];
    while (true) {
        ssize_t n = read(fd, tmp, sizeof(tmp));
        if (n > 0) {
            size_t old_size = cc->recv_buf.size();
            cc->recv_buf.resize(old_size + (size_t)n);
            memcpy(cc->recv_buf.data() + old_size, tmp, (size_t)n);
        } else if (n == 0) {
            removeClient(fd);
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            removeClient(fd);
            return;
        }
    }

    // Process complete messages in recv_buf
    while (true) {
        size_t avail = cc->recv_buf.size();

        if (cc->recv_wanted == 0) {
            // Need at least 8 bytes for header
            if (avail < 8) return;
            const uint8_t* p = cc->recv_buf.data();
            cc->recv_msg_type   = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
            cc->recv_payload_len = (uint32_t)p[4] | ((uint32_t)p[5] << 8) | ((uint32_t)p[6] << 16) | ((uint32_t)p[7] << 24);
            cc->recv_wanted = 8 + cc->recv_payload_len;
        }

        if (avail >= cc->recv_wanted) {
            // Extract payload
            std::vector<uint8_t> payload(cc->recv_payload_len);
            if (cc->recv_payload_len > 0)
                memcpy(payload.data(), cc->recv_buf.data() + 8, cc->recv_payload_len);

            uint32_t type = cc->recv_msg_type;
            uint32_t plen = cc->recv_payload_len;

            // Consume this message from buffer
            size_t consumed = cc->recv_wanted;
            if (consumed < cc->recv_buf.size()) {
                memmove(cc->recv_buf.data(), cc->recv_buf.data() + consumed, cc->recv_buf.size() - consumed);
            }
            cc->recv_buf.resize(cc->recv_buf.size() - consumed);
            cc->recv_wanted = 0;

            // Dispatch
            dispatchMessage(fd, type, payload.data(), plen);
        } else {
            return; // partial message, wait for more data
        }
    }
}

void DaemonServer::sendResponse(int fd, uint32_t type, const uint8_t* payload, uint32_t len) {
    // Собираем header + payload в один буфер для гарантированной доставки
    std::vector<uint8_t> buf(8 + len);
    buf[0] = type & 0xFF;
    buf[1] = (type >> 8) & 0xFF;
    buf[2] = (type >> 16) & 0xFF;
    buf[3] = (type >> 24) & 0xFF;
    buf[4] = len & 0xFF;
    buf[5] = (len >> 8) & 0xFF;
    buf[6] = (len >> 16) & 0xFF;
    buf[7] = (len >> 24) & 0xFF;
    if (len > 0 && payload) memcpy(buf.data() + 8, payload, len);

    if (!writeExact(fd, buf.data(), buf.size())) {
        DLOG_ERROR("sendResponse: writeExact failed on fd %d (len=%u)", fd, (unsigned)buf.size());
    }
}

void DaemonServer::dispatchMessage(int fd, uint32_t type, const uint8_t* payload, uint32_t len) {
    auto sendOk = [&](uint32_t resp_type) {
        sendResponse(fd, resp_type, nullptr, 0);
    };
    auto sendError = [&](uint32_t req_type, uint32_t code, const char* msg) {
        using namespace biocore_proto;
        size_t msglen = strlen(msg);
        std::vector<uint8_t> buf(4 + 4 + msglen);
        uint8_t* p = buf.data();
        write_u32(p, req_type);
        write_u32(p, code);
        memcpy(p, msg, msglen);
        sendResponse(fd, MSG_ERROR, buf.data(), (uint32_t)buf.size());
    };

    const uint8_t* p = payload;
    using namespace biocore_proto;

    // ── Извлечение session_id ──
    uint32_t session_id = 0;
    uint32_t base_type = type;
    (void)base_type;
    if (msg_has_session(type)) {
        base_type = msg_base_type(type);
        if (len >= 4) {
            memcpy(&session_id, p, 4);
            p += 4;
            len -= 4;
        }
    }
    // Получить контекст сессии
    SessionContext* ctx = getSession(session_id);
    if (!ctx) ctx = default_session_;  // fallback на session 0

    // Временно переключить указатели на контекст сессии
    // (для обратной совместимости с существующими handler'ами)
    SessionContext* prev_ctx = current_ctx_;
    current_ctx_ = ctx;

    switch (type) {
    // ── Control ─────────────────────────────────────────────────────
    case MSG_PING:
        sendOk(MSG_PONG);
        break;

    case MSG_GET_VERSION: {
        std::vector<uint8_t> buf(3 * 4 + 16);  // major:u32 + minor:u32 + patch:u32 + build_id:16
        uint8_t* wp = buf.data();
        write_u32(wp, (uint32_t)core_version_major());
        write_u32(wp, (uint32_t)core_version_minor());
        write_u32(wp, (uint32_t)core_version_patch());
        // build_id: DJB2 hash of version string + __DATE__ __TIME__
        {
            const char* s = core_version_string();
            const char* dt = __DATE__ __TIME__;
            uint32_t hash = 5381;
            for (const char* p = s; *p; p++) hash = hash * 33 + (uint8_t)*p;
            for (const char* p = dt; *p; p++) hash = hash * 33 + (uint8_t)*p;
            write_u32(wp, hash);
            memset(wp, 0, 12);  // остальные 12 байт зарезервированы
        }
        sendResponse(fd, MSG_VERSION_INFO, buf.data(), (uint32_t)buf.size());
        break;
    }

    case MSG_SHUTDOWN:
        sendOk(MSG_SHUTDOWN_ACK);
        requestShutdown();
        break;

    // ── Network I/O ──────────────────────────────────────────────────
    case MSG_LOAD_BION: {
        if (len == 0) { sendError(type, ERR_INVALID_PARAM, "empty filename"); break; }
        std::string filename((const char*)payload, len);
        bool ok = loadNetwork(filename.c_str());
        std::vector<uint8_t> buf(5);
        uint8_t* wp = buf.data();
        write_u8(wp, ok ? 1 : 0);
        write_u32(wp, ok ? numSegs() : 0);
        sendResponse(fd, MSG_LOAD_BION_RESULT, buf.data(), ok ? 5 : 1);
        break;
    }

    case MSG_SAVE_BION: {
        if (len == 0 || !isNetworkLoaded()) { sendError(type, ERR_NO_NETWORK, "no network loaded"); break; }
        std::string filename((const char*)payload, len);
        bool ok = saveNetwork(filename.c_str());
        std::vector<uint8_t> buf(1);
        buf[0] = ok ? 1 : 0;
        sendResponse(fd, MSG_SAVE_BION_RESULT, buf.data(), 1);
        break;
    }

    case MSG_UNLOAD:
        unloadNetwork();
        sendOk(MSG_UNLOAD_RESULT);
        break;

    case MSG_LOAD_BEXP: {
        // payload: [bexp_path_len:u16][bexp_path:byte[]][ticks:u32][output_path_len:u16][output_path:byte[]]
        if (len < 6 || !ctx) { sendError(type, ERR_INVALID_PARAM, "invalid payload"); break; }
        const uint8_t* q = p;
        uint16_t bexp_len = (uint16_t)q[0] | ((uint16_t)q[1] << 8); q += 2;
        if (bexp_len == 0 || bexp_len > 1024 || (size_t)(q - p + bexp_len + 4 + 2) > len) {
            sendError(type, ERR_INVALID_PARAM, "bexp_path_len invalid"); break;
        }
        std::string bexp_path((const char*)q, bexp_len); q += bexp_len;

        uint32_t ticks = read_u32(q);
        if (ticks == 0 || ticks > 100'000'000) {
            sendError(type, ERR_INVALID_PARAM, "ticks out of range"); break;
        }

        uint16_t out_len = (uint16_t)q[0] | ((uint16_t)q[1] << 8); q += 2;
        if (out_len == 0 || out_len > 1024 || (size_t)(q - p + out_len) > len) {
            sendError(type, ERR_INVALID_PARAM, "output_path_len invalid"); break;
        }
        std::string out_path((const char*)q, out_len);

        // Читаем .bexp JSON-заголовок
        FILE* bf = fopen(bexp_path.c_str(), "rb");
        if (!bf) { sendError(type, ERR_FILE_NOT_FOUND, "bexp file not found"); break; }
        char sig[5] = {0};
        if (fread(sig, 1, 4, bf) != 4) { fclose(bf); break; }
        if (memcmp(sig, "BEXP", 4) != 0) { fclose(bf); sendError(type, ERR_BION_FORMAT, "invalid BEXP signature"); break; }

        uint32_t bexp_ver = 0;
        if (fread(&bexp_ver, 4, 1, bf) != 1) { fclose(bf); break; }

        // Читаем длину JSON-заголовка (uint32_t после версии)
        uint32_t json_len = 0;
        if (fread(&json_len, 4, 1, bf) != 1) { fclose(bf); break; }

        std::string json_buf(json_len, '\0');
        if (fread(&json_buf[0], 1, json_len, bf) != json_len) { fclose(bf); break; }
        fclose(bf);

        // Извлекаем bion_file_path из JSON
        std::string bion_path;
        auto bion_key = json_buf.find("\"bion_file_path\"");
        if (bion_key != std::string::npos) {
            auto val_start = json_buf.find("\"", bion_key + 17);
            if (val_start != std::string::npos) {
                val_start++;
                auto val_end = json_buf.find("\"", val_start);
                if (val_end != std::string::npos)
                    bion_path = json_buf.substr(val_start, val_end - val_start);
            }
        }
        if (bion_path.empty()) {
            sendError(type, ERR_INVALID_PARAM, "bexp missing bion_file_path"); break;
        }

        // Загружаем .bion
        if (!loadNetworkForSession(ctx->session_id, bion_path.c_str())) {
            sendError(type, ERR_LOAD_FAILED, "failed to load bion from bexp"); break;
        }

        // Извлекаем probes и events (стимулы) из JSON
        // Применяем стимулы если они описаны в .bexp
        auto events_key = json_buf.find("\"events\"");
        if (events_key != std::string::npos) {
            // Упрощённый парсинг: ищем current_stimulus в events
            auto cs = json_buf.find("\"type\":\"current_stimulus\"", events_key);
            if (cs != std::string::npos && cs < events_key + 2000) {
                // Извлекаем segment_id, amplitude, duration, delay
                float amp = 0, dur = 0;
                uint32_t seg = 0;
                auto amp_pos = json_buf.find("\"amplitude\"", cs);
                if (amp_pos != std::string::npos) amp = strtof(json_buf.c_str() + amp_pos + 12, nullptr);
                auto dur_pos = json_buf.find("\"duration\"", cs);
                if (dur_pos != std::string::npos) dur = strtof(json_buf.c_str() + dur_pos + 11, nullptr);
                auto seg_pos = json_buf.find("\"segment_id\"", cs);
                if (seg_pos != std::string::npos) seg = (uint32_t)atoi(json_buf.c_str() + seg_pos + 13);

                if (amp != 0 && dur > 0 && ctx->stimulus_count < SessionContext::MAX_STIM) {
                    ctx->stimuli[ctx->stimulus_count].segment_id = seg;
                    ctx->stimuli[ctx->stimulus_count].amplitude_pA = amp;
                    ctx->stimuli[ctx->stimulus_count].duration_ms = dur;
                    ctx->stimulus_count++;
                }
            }
        }

        // Извлекаем detection/expected_range для валидации
        std::string probes_json = "{}";
        auto det_key = json_buf.find("\"detection\"");
        if (det_key != std::string::npos) {
            auto det_end = json_buf.find("}", det_key);
            if (det_end != std::string::npos) {
                // Упрощённо: сохраняем detection как есть
                probes_json = json_buf.substr(det_key, det_end - det_key + 1);
            }
        }

        // Сохраняем контекст задачи
        ctx->task_start_tick = ctx->sim_tick_counter;
        ctx->task_total_ticks = ticks;

        // Симуляция N тиков
        float dt = 0.1f;
        for (uint32_t t = 0; t < ticks; t++) {
            core_run_tick_ctx(ctx,
                _segments(), num_segments(),
                _connections(), num_connections(),
                _synapses(), num_synapses(),
                0, &ctx->sim_tick_counter, dt,
                ctx->stimuli, &ctx->stimulus_count,
                nullptr, nullptr, nullptr, nullptr,
                nullptr, nullptr, nullptr, nullptr);
        }

        // Сохраняем результат
        bool saved = saveNetwork(out_path.c_str());

        // Ответ: [ok:u8][bion_path_len:u16][bion_path][probes_json_len:u16][probes_json]
        uint8_t ok = saved ? 1 : 0;
        uint16_t blen = (uint16_t)bion_path.size();
        uint16_t plen = (uint16_t)probes_json.size();
        std::vector<uint8_t> buf(1 + 2 + blen + 2 + plen);
        buf[0] = ok;
        buf[1] = (uint8_t)(blen & 0xFF); buf[2] = (uint8_t)(blen >> 8);
        memcpy(buf.data() + 3, bion_path.c_str(), blen);
        buf[3 + blen] = (uint8_t)(plen & 0xFF); buf[4 + blen] = (uint8_t)(plen >> 8);
        if (plen > 0) memcpy(buf.data() + 5 + blen, probes_json.c_str(), plen);
        sendResponse(fd, MSG_LOAD_BEXP_RESULT, buf.data(), (uint32_t)buf.size());
        break;
    }

    case MSG_GET_NETWORK_INFO: {
        if (!isNetworkLoaded()) { sendError(type, ERR_NO_NETWORK, "no network loaded"); break; }
        std::vector<uint8_t> buf(4 + 4 + 4 + 4 + 4);
        uint8_t* wp = buf.data();
        write_u32(wp, numSegs());
        write_u32(wp, numSyns());
        write_u32(wp, numConns());
        write_u32(wp, numNeurons());
        write_u32(wp, simMode());
        sendResponse(fd, MSG_NETWORK_INFO, buf.data(), (uint32_t)buf.size());
        break;
    }

    // ── Simulation ──────────────────────────────────────────────────
    case MSG_START_SIM: {
        if (!isNetworkLoaded()) { sendError(type, ERR_NO_NETWORK, "no network loaded"); break; }
        float mult = 1.0f, hz = 0.0f;
        if (len >= 8) { mult = read_f32(p); hz = read_f32(p); }
        if (mult < 0.0f) mult = 0.0f;
        if (hz < 0.0f) hz = 0.0f;
        bool ok = startSim(mult, hz);
        std::vector<uint8_t> buf(1);
        buf[0] = ok ? 1 : 0;
        sendResponse(fd, MSG_START_SIM_RESULT, buf.data(), 1);
        break;
    }

    case MSG_STOP_SIM: {
        if (!isNetworkLoaded()) { sendError(type, ERR_NO_NETWORK, "no network loaded"); break; }
        stopSim();
        std::vector<uint8_t> buf(8);
        uint8_t* wp = buf.data();
        write_u64(wp, sim_tick_counter());
        sendResponse(fd, MSG_STOP_SIM_RESULT, buf.data(), 8);
        break;
    }

    case MSG_PAUSE_SIM:
        if (!isNetworkLoaded()) { sendError(type, ERR_NO_NETWORK, "no network loaded"); break; }
        pauseSim();
        sendOk(MSG_PAUSE_SIM_RESULT);
        break;

    case MSG_STEP_SIM: {
        if (!isNetworkLoaded()) { sendError(type, ERR_NO_NETWORK, "no network loaded"); break; }
        uint32_t n = 1;
        if (len >= 4) n = read_u32(p);
        if (n == 0 || n > 1000000) { sendError(type, ERR_INVALID_PARAM, "num_ticks out of range [1,1000000]"); break; }
        stepSim(n);
        std::vector<uint8_t> buf(8);
        uint8_t* wp = buf.data();
        write_u64(wp, sim_tick_counter());
        sendResponse(fd, MSG_STEP_SIM_RESULT, buf.data(), 8);
        break;
    }

    case MSG_SET_SPEED: {
        float mult = 1.0f, hz = 0.0f;
        if (len >= 8) { mult = read_f32(p); hz = read_f32(p); }
        if (mult < 0.0f || mult > 100.0f) { sendError(type, ERR_INVALID_PARAM, "multiplier out of range [0,100]"); break; }
        if (hz < 0.0f || hz > 10000.0f) { sendError(type, ERR_INVALID_PARAM, "target_hz out of range [0,10000]"); break; }
        sim_multiplier() = mult;
        sim_target_hz()  = hz;
        sendOk(MSG_SET_SPEED_RESULT);
        break;
    }

    case MSG_SET_BACKEND: {
        uint8_t be = 0;
        if (len >= 1) be = read_u8(p);
        if (be > 1) { sendError(type, ERR_INVALID_PARAM, "backend must be 0 (CPU) or 1 (GPU)"); break; }
        sim_backend().store(be);
        std::vector<uint8_t> buf(1);
        buf[0] = 1;
        sendResponse(fd, MSG_SET_BACKEND_RESULT, buf.data(), 1);
        break;
    }

    case MSG_RESET_SIM: {
        if (!isNetworkLoaded()) { sendError(type, ERR_NO_NETWORK, "no network loaded"); break; }
        stopSim();
        sim_tick_counter() = 0;
        sendOk(MSG_RESET_SIM_RESULT);
        break;
    }

    // ── Stimuli ─────────────────────────────────────────────────────
    case MSG_ADD_CURRENT_STIMULUS: {
        if (!isNetworkLoaded() || len < 16) break;
        uint32_t sid = read_u32(p);
        float amp = read_f32(p);
        float dur = read_f32(p);
        float delay = read_f32(p);
        if (sid >= num_segments()) break;
        if (dur < 0.0f) dur = 0.0f;
        if (delay < 0.0f) delay = 0.0f;
        addCurrentStimulus(sid, amp, dur, delay);
        break;
    }
    case MSG_ADD_VOLTAGE_CLAMP: {
        if (!isNetworkLoaded() || len < 24) break;
        uint32_t sid = read_u32(p);
        float v = read_f32(p);
        uint64_t st = read_u64(p);
        uint64_t en = read_u64(p);
        if (sid >= num_segments() || st > en) break;
        addVoltageClamp(sid, v, st, en);
        break;
    }
    case MSG_ADD_RAMP_STIMULUS: {
        if (!isNetworkLoaded() || len < 28) break;
        uint32_t sid = read_u32(p);
        float sa = read_f32(p);
        float ea = read_f32(p);
        uint64_t st = read_u64(p);
        uint64_t en = read_u64(p);
        if (sid >= num_segments() || st > en) break;
        addRampStimulus(sid, sa, ea, st, en);
        break;
    }
    case MSG_ADD_ION_INJECTION: {
        if (!isNetworkLoaded() || len < 13) break;
        uint32_t sid = read_u32(p);
        int32_t ion = read_i32(p);
        float amt = read_f32(p);
        uint8_t xc = read_u8(p);
        if (sid >= num_segments() || ion < 0 || ion >= ION_COUNT) break;
        addIonInjection(sid, ion, amt, xc != 0);
        break;
    }
    case MSG_ADD_LIGAND_INJECTION: {
        if (!isNetworkLoaded() || len < 13) break;
        uint32_t sid = read_u32(p);
        int32_t nt = read_i32(p);
        float amt = read_f32(p);
        uint8_t lc = read_u8(p);
        if (sid >= num_segments() || nt < 0 || nt >= NT_COUNT) break;
        addLigandInjection(sid, nt, amt, lc != 0);
        break;
    }
    case MSG_CLEAR_STIMULI:
        clearStimuli();
        break;

    // ── State streaming ──────────────────────────────────────────────
    case MSG_SUBSCRIBE_STATE: {
        if (!isNetworkLoaded()) { sendError(type, ERR_NO_NETWORK, "no network loaded"); break; }
        uint32_t interval = 0;
        bool delta = false;
        if (len >= 4) interval = read_u32(p);
        if (len >= 5) delta = (read_u8(p) != 0);
        // interval=0 → каждый тик, максимум 100000 тиков между фреймами
        if (interval > 100000) interval = 100000;
        streamer().addSubscriber(fd, interval, delta);
        streamer().setDataPtrs(_segments(), num_segments(), _synapses(), num_synapses());
        std::vector<uint8_t> buf(1);
        buf[0] = 1;
        sendResponse(fd, MSG_SUBSCRIBE_RESULT, buf.data(), 1);
        break;
    }
    case MSG_UNSUBSCRIBE_STATE:
        streamer().removeSubscriber(fd);
        sendOk(MSG_UNSUBSCRIBE_RESULT);
        break;

    // ── Plugins ──────────────────────────────────────────────────────
    case MSG_LOAD_PLUGIN_DIR: {
        if (len == 0) { sendError(type, ERR_INVALID_PARAM, "empty path"); break; }
        std::string path((const char*)payload, len);
        int n = plugin_load_directory(path.c_str());
        plugin_load_info_directory(path.c_str());
        std::vector<uint8_t> buf(4);
        uint8_t* wp = buf.data();
        write_i32(wp, n);
        sendResponse(fd, MSG_LOAD_PLUGIN_DIR_RESULT, buf.data(), 4);
        break;
    }
    case MSG_LIST_PLUGINS: {
        int n = plugin_get_count();
        std::vector<uint8_t> buf;
        auto addStr = [&](const char* s) {
            if (!s) s = "";
            uint16_t slen = (uint16_t)strlen(s);
            size_t off = buf.size();
            buf.resize(off + 2 + slen);
            buf[off] = slen & 0xFF;
            buf[off+1] = (slen >> 8) & 0xFF;
            memcpy(buf.data() + off + 2, s, slen);
        };
        size_t count_off = buf.size();
        buf.resize(count_off + 4);
        uint8_t* wp = buf.data();
        write_u32(wp, (uint32_t)n); (void)wp;
        for (int i = 0; i < n; i++) {
            const PluginInfo* pi = plugin_get(i);
            if (!pi) continue;
            addStr(pi->name);
            int mc = pi->modifier_count;
            size_t mc_off = buf.size();
            buf.resize(mc_off + 4);
            uint8_t* wp2 = buf.data() + mc_off;
            write_u32(wp2, (uint32_t)mc);
            for (int j = 0; j < mc; j++) {
                buf.push_back(pi->modifiers[j].id);
                addStr(pi->modifiers[j].name);
                addStr(pi->modifiers[j].category);
                addStr(pi->modifiers[j].description);
            }
        }
        sendResponse(fd, MSG_LIST_PLUGINS_RESULT, buf.data(), (uint32_t)buf.size());
        break;
    }
    case MSG_QUERY_MODIFIER_INFO: {
        if (len < 1) { sendError(type, ERR_INVALID_PARAM, "need modifier_id:u8"); break; }
        uint8_t mod_id = read_u8(p);
        if (mod_id >= MOD_TYPE_COUNT) { sendError(type, ERR_INVALID_PARAM, "modifier_id out of range"); break; }
        const char* name = plugin_modifier_name(mod_id);
        const char* cat  = plugin_modifier_category(mod_id);
        const char* desc = plugin_modifier_description(mod_id);
        const char* info = plugin_get_info_json(mod_id);
        auto addStr = [](std::vector<uint8_t>& b, const char* s) {
            if (!s) s = "";
            uint16_t slen = (uint16_t)strlen(s);
            size_t off = b.size();
            b.resize(off + 2 + slen);
            b[off] = slen & 0xFF;
            b[off+1] = (slen >> 8) & 0xFF;
            memcpy(b.data() + off + 2, s, slen);
        };
        std::vector<uint8_t> buf;
        addStr(buf, name);
        addStr(buf, cat);
        addStr(buf, desc);
        addStr(buf, info);
        sendResponse(fd, MSG_MODIFIER_INFO_RESULT, buf.data(), (uint32_t)buf.size());
        break;
    }

    // ── Consolidation ────────────────────────────────────────────────
    case MSG_SET_CONSOLIDATION_INTERVAL: {
        if (len < 4) { sendError(type, ERR_INVALID_PARAM, "need interval:u32"); break; }
        uint32_t interval = read_u32(p);
        if (interval < 10 || interval > 1000000) {
            sendError(type, ERR_INVALID_PARAM, "interval out of range [10,1000000]");
            break;
        }
        core_set_consolidation_interval(interval);
        sendOk(MSG_CONSOLIDATION_INTERVAL_RESULT);
        break;
    }
    case MSG_GET_CONSOLIDATION_STATUS: {
        std::vector<uint8_t> buf(4 + 4 + 1);
        uint8_t* wp = buf.data();
        write_u32(wp, core_get_consolidation_interval());
        write_u32(wp, (uint32_t)core_get_pending_consolidation_events());
        write_u8(wp, consolidation_started() ? 1 : 0);
        sendResponse(fd, MSG_CONSOLIDATION_STATUS, buf.data(), 9);
        break;
    }
    case MSG_GET_NUCLEUS_SNAPSHOT: {
        if (len < 4) { sendError(type, ERR_INVALID_PARAM, "need neuron_idx:u32"); break; }
        if (!isNetworkLoaded()) { sendError(type, ERR_NO_NETWORK, "no network loaded"); break; }
        uint32_t idx = read_u32(p);
        if (idx >= num_neurons()) { sendError(type, ERR_INVALID_PARAM, "neuron_idx out of range"); break; }
        std::vector<uint8_t> buf(sizeof(Nucleus));
        int ok = core_get_nucleus_snapshot(idx, (Nucleus*)buf.data());
        if (ok) sendResponse(fd, MSG_NUCLEUS_SNAPSHOT, buf.data(), sizeof(Nucleus));
        else    sendError(type, ERR_INVALID_PARAM, "neuron not found");
        break;
    }
    case MSG_RESET_GENE_EXPRESSION: {
        // Восстанавливаем исходное состояние ядер (профиль экспрессии клеточного типа)
        if (!isNetworkLoaded()) { sendError(type, ERR_NO_NETWORK, "no network loaded"); break; }
        if (initial_nuclei().empty() || !_nuclei()) {
            sendError(type, ERR_INTERNAL, "no initial gene state saved");
            break;
        }
        size_t saved_count = initial_nuclei().size() / sizeof(Nucleus);
        size_t copy_count = (saved_count < num_nuclei()) ? saved_count : num_nuclei();
        memcpy(_nuclei(), initial_nuclei().data(), copy_count * sizeof(Nucleus));
        // Сбрасываем консолидацию (накопленные сигналы устарели)
        if (consolidation_started()) {
            core_stop_consolidation_ctx(current_ctx_);
            consolidation_started() = false;
        }
        std::vector<uint8_t> buf(1);
        buf[0] = 1;
        sendResponse(fd, MSG_RESET_GENE_EXPRESSION_RESULT, buf.data(), 1);
        break;
    }

    // ── Backend info ─────────────────────────────────────────────────
    case MSG_GET_BACKEND_INFO: {
        std::vector<uint8_t> buf(2);
        buf[0] = (uint8_t)sim_backend().load();
        buf[1] = gpuAvailable() ? 1 : 0;
        sendResponse(fd, MSG_BACKEND_INFO, buf.data(), 2);
        break;
    }
    case MSG_LIST_DEVICES: {
        // Формат ответа: [count:u32][GpuDeviceInfo...]
        uint32_t count = (uint32_t)gpu_devices_.size();
        std::vector<uint8_t> buf(4 + count * sizeof(GpuDeviceInfo));
        uint8_t* p = buf.data();
        write_u32(p, count);
        for (auto& d : gpu_devices_) {
            memcpy(p, &d, sizeof(GpuDeviceInfo));
            p += sizeof(GpuDeviceInfo);
        }
        sendResponse(fd, MSG_LIST_DEVICES_RESULT, buf.data(), (uint32_t)buf.size());
        break;
    }

    case MSG_LIST_SESSIONS: {
        // Формат ответа: [count:u32][session_id:u32...]
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        uint32_t count = (uint32_t)sessions_.size();
        std::vector<uint8_t> buf(4 + count * 4);
        uint8_t* p = buf.data();
        write_u32(p, count);
        for (auto& kv : sessions_) {
            write_u32(p, kv.first);
        }
        sendResponse(fd, MSG_LIST_SESSIONS_RESULT, buf.data(), (uint32_t)buf.size());
        break;
    }

    // ── Checkpoint ───────────────────────────────────────────────────
    case MSG_SAVE_CHECKPOINT: {
        if (len == 0 || !isNetworkLoaded()) { sendError(type, ERR_NO_NETWORK, "no network loaded"); break; }
        std::string filename((const char*)payload, len);
        bool ok = saveNetwork(filename.c_str());
        std::vector<uint8_t> buf(1);
        buf[0] = ok ? 1 : 0;
        sendResponse(fd, MSG_SAVE_CHECKPOINT_RESULT, buf.data(), 1);
        break;
    }
    case MSG_LOAD_CHECKPOINT: {
        // Payload: [filename:byte[]][restore_tick:u64]
        if (len < 12) { sendError(type, ERR_INVALID_PARAM, "need filename + restore_tick"); break; }
        uint32_t fnlen = len - 8;
        std::string filename((const char*)payload, fnlen);
        uint64_t restore_tick;
        memcpy(&restore_tick, payload + fnlen, 8);

        // Загружаем BION из чекпоинта
        BionData bd;
        memset(&bd, 0, sizeof(bd));
        int ret = bion_load_host(filename.c_str(), &bd);
        if (ret != 0) { sendError(type, ERR_FILE_NOT_FOUND, "checkpoint file not found"); break; }

        // Останавливаем симуляцию перед заменой данных
        stopSim();

        // Заменяем данные сети
        {
            std::lock_guard<std::mutex> lock(sim_mutex());
            freeNetwork();
            _segments()     = bd.segments;
            _synapses()     = bd.synapses;
            _connections()  = bd.connections;
            _neurons()      = bd.neurons;
            _nuclei()       = bd.nuclei;
            num_segments()    = bd.num_segments;
            num_synapses()    = bd.num_synapses;
            num_connections() = bd.num_connections;
            num_neurons()     = bd.num_neurons;
            num_nuclei()      = bd.num_nuclei;
            simDataFromGpu()  = false;  // данные от bion_load_host — обычная память
            sim_tick_counter() = restore_tick;
            consolidation_started() = false;  // сбрасываем консолидацию при загрузке чекпоинта
        }

        // Обновляем streamer
        streamer().setDataPtrs(_segments(), num_segments(), _synapses(), num_synapses());
        DLOG_INFO(" Checkpoint loaded: %u segments, tick=%lu", num_segments(), (unsigned long)restore_tick);

        std::vector<uint8_t> buf(1);
        buf[0] = 1;
        sendResponse(fd, MSG_LOAD_CHECKPOINT_RESULT, buf.data(), 1);
        break;
    }

    // ── Session Management ─────────────────────────────────────────────
    case MSG_CREATE_SESSION: {
        // payload: [gpu_device_index:i32] (опционально)
        int gpu_idx = -1;
        if (len >= 4) {
            memcpy(&gpu_idx, p, 4);
        }
        // Валидация: gpu_device_index
        if (gpu_idx >= 0) {
            int max_dev = (int)gpu_devices_.size();
            if (!gpuAvailable() || max_dev == 0) {
                sendError(type, ERR_GPU_DEVICE_UNAVAILABLE, "GPU not available");
                break;
            }
            if (gpu_idx >= max_dev) {
                sendError(type, ERR_INVALID_PARAM, "gpu_device_index out of range");
                break;
            }
            // Проверка allowlist
            if (!config_.gpu_device_allowlist.empty()) {
                bool allowed = false;
                for (int idx : config_.gpu_device_allowlist) {
                    if (idx == gpu_idx) { allowed = true; break; }
                }
                if (!allowed) {
                    sendError(type, ERR_GPU_DEVICE_UNAVAILABLE, "GPU device not in allowlist");
                    break;
                }
            }
        }
        uint32_t new_sid = 0;
        { std::lock_guard<std::mutex> lk(sessions_mutex_); new_sid = next_session_id_; }
        // Проверка лимита сессий
        if (sessions_.size() >= config_.max_sessions) {
            sendError(type, ERR_MAX_SESSIONS, "max sessions reached");
            break;
        }
        auto* new_ctx = createSession(new_sid, gpu_idx);
        std::vector<uint8_t> buf(9);
        uint8_t* wp = buf.data();
        write_u8(wp, new_ctx ? 1 : 0);
        write_u32(wp, new_ctx ? new_sid : 0);
        write_i32(wp, gpu_idx);
        sendResponse(fd, MSG_CREATE_SESSION_RESULT, buf.data(), 9);
        if (new_ctx) setClientSessionId(fd, new_sid);
        break;
    }
    case MSG_DESTROY_SESSION: {
        uint32_t sid = session_id;
        if (len >= 4) memcpy(&sid, p, 4);
        // Валидация: нельзя удалить сессию 0
        if (sid == 0) {
            sendError(type, ERR_INVALID_PARAM, "cannot destroy default session");
            break;
        }
        // Валидация: сессия должна существовать
        if (!getSession(sid)) {
            sendError(type, ERR_INVALID_SESSION, "session not found");
            break;
        }
        destroySession(sid);
        if (sid == session_id) setClientSessionId(fd, 0);
        uint8_t ok = 1;
        sendResponse(fd, MSG_DESTROY_SESSION_RESULT, &ok, 1);
        break;
    }
    case MSG_RUN_TASK: {
        // payload: [bion_path_len:u16][bion_path:byte[]][ticks:u32]
        if (len < 6 || !ctx) { sendError(type, ERR_INVALID_PARAM, "invalid RUN_TASK payload"); break; }
        const uint8_t* q = p;
        uint16_t bion_len = (uint16_t)q[0] | ((uint16_t)q[1] << 8); q += 2;
        // Валидация: bion_path_len
        if (bion_len == 0 || bion_len > 1024) {
            sendError(type, ERR_INVALID_PARAM, "bion_path_len out of range [1,1024]"); break;
        }
        if ((size_t)(bion_len + 2 + 4) > len) {
            sendError(type, ERR_INVALID_PARAM, "RUN_TASK payload truncated"); break;
        }
        std::string bion_path((const char*)q, bion_len); q += bion_len;
        uint32_t ticks = (uint32_t)q[0] | ((uint32_t)q[1] << 8) | ((uint32_t)q[2] << 16) | ((uint32_t)q[3] << 24);
        // Валидация: ticks
        if (ticks == 0 || ticks > 100'000'000) {
            sendError(type, ERR_INVALID_PARAM, "ticks out of range [1,100000000]"); break;
        }
        // Загружаем BION и запускаем
        bool ok = loadNetworkForSession(ctx->session_id, bion_path.c_str());
        std::vector<uint8_t> buf(1);
        buf[0] = ok ? 1 : 0;
        sendResponse(fd, MSG_RUN_TASK_RESULT, buf.data(), 1);
        break;
    }
    case MSG_RUN_TASK_PARAMS: {
        // payload: [bion_path_len:u16][bion_path:byte[]][ticks:u32]
        //          [params_json_len:u16][params_json:byte[]]
        //          [bexp_output_path_len:u16][path:byte[]][seed:u32]
        if (len < 10 || !ctx) {
            sendError(type, ERR_INVALID_PARAM, "invalid RUN_TASK_PARAMS payload"); break;
        }
        const uint8_t* q = p;

        // bion_path
        uint16_t bion_len = (uint16_t)q[0] | ((uint16_t)q[1] << 8); q += 2;
        if (bion_len == 0 || bion_len > 1024 || (size_t)(q - p + bion_len) > len) {
            sendError(type, ERR_INVALID_PARAM, "bion_path_len invalid"); break;
        }
        std::string bion_path((const char*)q, bion_len); q += bion_len;

        // ticks
        if ((size_t)(q - p + 4) > len) { sendError(type, ERR_INVALID_PARAM, "ticks truncated"); break; }
        uint32_t ticks = read_u32(q);
        if (ticks == 0 || ticks > 100'000'000) {
            sendError(type, ERR_INVALID_PARAM, "ticks out of range"); break;
        }

        // params_json
        if ((size_t)(q - p + 2) > len) { sendError(type, ERR_INVALID_PARAM, "params truncated"); break; }
        uint16_t params_len = (uint16_t)q[0] | ((uint16_t)q[1] << 8); q += 2;
        std::string params_json;
        if (params_len > 0) {
            if ((size_t)(q - p + params_len) > len) {
                sendError(type, ERR_INVALID_PARAM, "params_json truncated"); break;
            }
            params_json.assign((const char*)q, params_len); q += params_len;
        }

        // bexp_output_path
        if ((size_t)(q - p + 2) > len) { sendError(type, ERR_INVALID_PARAM, "bexp path truncated"); break; }
        uint16_t bexp_len = (uint16_t)q[0] | ((uint16_t)q[1] << 8); q += 2;
        if (bexp_len == 0 || bexp_len > 1024 || (size_t)(q - p + bexp_len) > len) {
            sendError(type, ERR_INVALID_PARAM, "bexp_output_path_len invalid"); break;
        }
        std::string bexp_path((const char*)q, bexp_len); q += bexp_len;

        // seed (опционально)
        uint32_t seed = 0;
        if ((size_t)(q - p + 4) <= len) seed = read_u32(q);

        // Загружаем BION
        if (!loadNetworkForSession(ctx->session_id, bion_path.c_str())) {
            sendError(type, ERR_LOAD_FAILED, "failed to load BION");
            break;
        }

        // Применяем параметры к state[0] модификаторов (упрощённый JSON-парсер)
        if (!params_json.empty() && _segments() && num_segments() > 0) {
            const char* jp = params_json.c_str();
            while (*jp) {
                while (*jp && strchr(" \t\n\r{,}", *jp)) jp++;
                if (!*jp) break;
                std::string key;
                if (*jp == '"') { jp++; while (*jp && *jp != '"') { key += *jp; jp++; } if (*jp == '"') jp++; }
                else { while (*jp >= '0' && *jp <= '9') { key += *jp; jp++; } }
                if (key.empty()) break;
                while (*jp && strchr(" \t:", *jp)) jp++;
                char* end = nullptr;
                float val = strtof(jp, &end);
                if (end == jp) break;
                jp = end;
                uint8_t tid = 0;
                bool found = false;
                if (key[0] >= '0' && key[0] <= '9') {
                    tid = (uint8_t)atoi(key.c_str());
                    found = (tid < MOD_TYPE_COUNT);
                } else {
                    for (int t = 0; t < MOD_TYPE_COUNT; t++) {
                        const char* nm = plugin_modifier_name((uint8_t)t);
                        if (!nm || !nm[0]) continue;
                        const char* n = nm, *k = key.c_str();
                        while (*n && *k) {
                            if ((*n | 32) != (*k | 32)) break; // case-insensitive
                            n++; k++;
                        }
                        if (*k == 0) { tid = (uint8_t)t; found = true; break; }
                    }
                }
                if (found) {
                    for (uint32_t si = 0; si < num_segments(); si++) {
                        for (int mi = 0; mi < MAX_MODIFIERS_PER_SEGMENT; mi++) {
                            if (_segments()[si].modifiers[mi].type == tid) {
                                _segments()[si].modifiers[mi].state[0] = val;
                                break;
                            }
                        }
                    }
                }
            }
        }

        // Применяем seed к release_probability синапсов
        if (seed != 0 && _synapses() && num_synapses() > 0) {
            for (uint32_t si = 0; si < num_synapses(); si++) {
                uint32_t hash = seed ^ (_synapses()[si].id * 2654435761u);
                float var = (float)(hash & 0xFFFF) / 65536.0f * 0.02f;
                _synapses()[si].release_probability *= (1.0f + var);
            }
        }

        // Сохраняем контекст задачи для прогресса
        ctx->task_start_tick = ctx->sim_tick_counter;
        ctx->task_total_ticks = ticks;

        // Симуляция N тиков (синхронно)
        float dt = 0.1f;
        for (uint32_t tick = 0; tick < ticks; tick++) {
            core_run_tick_ctx(ctx,
                              _segments(), num_segments(),
                              _connections(), num_connections(),
                              _synapses(), num_synapses(),
                              0, &ctx->sim_tick_counter, dt,
                              nullptr, nullptr, nullptr, nullptr,
                              nullptr, nullptr, nullptr, nullptr,
                              nullptr, nullptr);
        }

        // Вычисляем детерминированные метрики для валидации
        char metrics_json[2048];
        core_compute_metrics(_segments(), num_segments(),
                             _neurons(), num_neurons(),
                             ctx->sim_tick_counter,
                             metrics_json, sizeof(metrics_json));

        // Сохраняем результат в .bexp
        bool saved = saveNetwork(bexp_path.c_str());
        uint8_t ok = saved ? 1 : 0;

        // Формат ответа: [ok:u8][bexp_path_len:u16][bexp_path][metrics_json_len:u16][metrics_json]
        uint16_t plen = saved ? (uint16_t)bexp_path.size() : 0;
        uint16_t mlen = (uint16_t)strlen(metrics_json);
        std::vector<uint8_t> buf(1 + 2 + plen + 2 + mlen);
        buf[0] = ok;
        buf[1] = (uint8_t)(plen & 0xFF);
        buf[2] = (uint8_t)((plen >> 8) & 0xFF);
        if (saved) memcpy(buf.data() + 3, bexp_path.c_str(), plen);
        size_t off = 3 + plen;
        buf[off]     = (uint8_t)(mlen & 0xFF);
        buf[off + 1] = (uint8_t)((mlen >> 8) & 0xFF);
        memcpy(buf.data() + off + 2, metrics_json, mlen);
        sendResponse(fd, MSG_RUN_TASK_PARAMS_RESULT, buf.data(), (uint32_t)buf.size());
        break;
    }

    case MSG_CHECKPOINT_AND_CONTINUE: {
        // payload: [bexp_path_len:u16][bexp_path:byte[]]
        if (len < 2 || !ctx) { sendError(type, ERR_INVALID_PARAM, "invalid payload"); break; }
        uint16_t plen = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
        if (plen == 0 || plen > 1024 || 2 + (size_t)plen > len) {
            sendError(type, ERR_INVALID_PARAM, "path invalid"); break;
        }
        std::string ckpt_path((const char*)(p + 2), plen);
        bool ok = saveNetwork(ckpt_path.c_str());
        uint8_t resp = ok ? 1 : 0;
        sendResponse(fd, MSG_CHECKPOINT_AND_CONTINUE_RESULT, &resp, 1);
        break;
    }
    case MSG_GET_TASK_PROGRESS: {
        if (!ctx) { sendError(type, ERR_INVALID_SESSION, "no session"); break; }
        uint64_t current = ctx->sim_tick_counter;
        uint64_t elapsed = current - ctx->task_start_tick;
        uint64_t total = ctx->task_total_ticks;
        uint8_t pct = (total > 0) ? (uint8_t)(elapsed * 100 / total) : 0;
        uint8_t buf[17];
        uint8_t* wp = buf;
        write_u64(wp, current);
        write_u64(wp, total);
        write_u8(wp, pct);
        sendResponse(fd, MSG_TASK_PROGRESS, buf, 17);
        break;
    }
    default:
        sendError(type, ERR_UNKNOWN_MSG, "unknown message type");
        break;
    }

    current_ctx_ = prev_ctx;  // восстановить контекст сессии
}

// ==================== SIMULATION ====================

bool DaemonServer::loadNetwork(const char* filename) {
    stopSim();  // остановить симуляцию перед освобождением сети
    std::lock_guard<std::mutex> lock(sim_mutex());
    freeNetwork();

    uint32_t captured_mode = 0;  // режим из заголовка .bion

    // GPU-путь: загрузка через GPU backend
    auto* bh = static_cast<GpuBackendHandle*>(current_ctx_ ? current_ctx_->gpu_backend_handle : nullptr);
    if (sim_backend().load() == 1 && bh && bh->create_) {
        if (gpu_sim()) { bh->destroy_(gpu_sim()); gpu_sim() = nullptr; }
        gpu_sim() = bh->create_();
        if (!gpu_sim()) { DLOG_ERROR("gpu_backend_create failed"); return false; }
        if (bh->load_bion_(gpu_sim(), filename) != 0) {
            DLOG_ERROR("gpu_backend_load_bion failed");
            bh->destroy_(gpu_sim()); gpu_sim() = nullptr;
            return false;
        }
        BionData* bd = (BionData*)bh->bion_data_(gpu_sim());
        captured_mode = bd->header.mode;
        _segments()     = bd->segments;
        _synapses()     = bd->synapses;
        _connections()  = bd->connections;
        _neurons()      = bd->neurons;
        _nuclei()       = bd->nuclei;
        num_segments()    = bd->num_segments;
        num_synapses()    = bd->num_synapses;
        num_connections() = bd->num_connections;
        num_neurons()     = bd->num_neurons;
        num_nuclei()      = bd->num_nuclei;
        simDataFromGpu() = true;
    } else {
        // CPU-путь
        BionData* bd = new BionData;
        int ret = bion_load_host(filename, bd);
        if (ret != 0) { delete bd; return false; }

        captured_mode = bd->header.mode;
        _segments()     = bd->segments;
        _synapses()     = bd->synapses;
        _connections()  = bd->connections;
        _neurons()      = bd->neurons;
        _nuclei()       = bd->nuclei;
        num_segments()    = bd->num_segments;
        num_synapses()    = bd->num_synapses;
        num_connections() = bd->num_connections;
        num_neurons()     = bd->num_neurons;
        num_nuclei()      = bd->num_nuclei;
        simDataFromGpu() = false;
    }

    current_ctx_->loaded_mode = captured_mode;

    // Глобальные таблицы (для обратной совместимости)
    active_library = biological_library;
    active_modulators = biological_modulators; // default

    // Per-session таблицы функций модификаторов
    current_ctx_->active_library    = biological_library;
    current_ctx_->active_modulators = biological_modulators;

    // Init consolidation (per-session)
    core_init_consolidation_ctx(current_ctx_, _neurons(), num_neurons(), _nuclei(), num_nuclei());

    // Сохраняем исходное состояние ядер для сброса генов
    if (_nuclei() && num_nuclei() > 0) {
        initial_nuclei().resize(num_nuclei() * sizeof(Nucleus));
        memcpy(initial_nuclei().data(), _nuclei(), num_nuclei() * sizeof(Nucleus));
    } else {
        initial_nuclei().clear();
    }

    // Notify streamer
    streamer().setDataPtrs(_segments(), num_segments(), _synapses(), num_synapses());

    DLOG_INFO(" Loaded network: %u segments, %u synapses, %u connections, %u neurons",
           num_segments(), num_synapses(), num_connections(), num_neurons());
    return true;
}

void DaemonServer::unloadNetwork() {
    std::lock_guard<std::mutex> lock(sim_mutex());
    stopSim();
    freeNetwork();
}

void DaemonServer::freeNetwork() {
    if (simDataFromGpu()) {
        // USM-память освободится при gpu_destroy_
        _segments() = nullptr; _synapses() = nullptr; _connections() = nullptr;
        _neurons() = nullptr; _nuclei() = nullptr;
    } else {
        if (_segments())    { delete[] _segments();     _segments()    = nullptr; }
        if (_synapses())    { delete[] _synapses();     _synapses()    = nullptr; }
        if (_connections()) { delete[] _connections();  _connections() = nullptr; }
        if (_neurons())     { delete[] _neurons();      _neurons()     = nullptr; }
        if (_nuclei())      { delete[] _nuclei();       _nuclei()      = nullptr; }
    }
    num_segments()    = 0;
    num_synapses()    = 0;
    num_connections() = 0;
    num_neurons()     = 0;
    num_nuclei()      = 0;
}

bool DaemonServer::saveNetwork(const char* filename) {
    std::lock_guard<std::mutex> lock(sim_mutex());
    if (!_segments()) return false;
    BionData bd;
    memset(&bd, 0, sizeof(bd));
    bd.segments    = _segments();
    bd.synapses    = _synapses();
    bd.connections = _connections();
    bd.neurons     = _neurons();
    bd.nuclei      = _nuclei();
    bd.num_segments    = num_segments();
    bd.num_synapses    = num_synapses();
    bd.num_connections = num_connections();
    bd.num_neurons     = num_neurons();
    bd.num_nuclei      = num_nuclei();
    return bion_save(filename, &bd) == 0;
}

bool DaemonServer::startSim(float multiplier, float target_hz) {
    if (!_segments()) return false;
    if (sim_running().load()) {
        if (sim_paused().load()) { sim_paused() = false; return true; }
        return true;
    }

    sim_multiplier() = multiplier;
    sim_target_hz()  = target_hz;

    // Start consolidation (per-session)
    if (!consolidation_started() && _nuclei() && num_nuclei() > 0) {
        core_start_consolidation_ctx(current_ctx_, _segments(), num_segments(),
                                      _synapses(), &num_synapses(),
                                      num_synapses() > 0 ? num_synapses() * 2 : 1000,
                                      &sim_tick_counter());
        consolidation_started() = true;
    }

    sim_running() = true;
    sim_paused() = false;
    sim_step_requested() = false;
    current_ctx_->sim_thread = std::thread(&DaemonServer::simulationLoop, this, current_ctx_);
    DLOG_INFO(" Simulation started");
    return true;
}

void DaemonServer::stopSim() {
    sim_running() = false;
    sim_paused() = false;
    if (current_ctx_->sim_thread.joinable()) current_ctx_->sim_thread.join();
    if (consolidation_started()) {
        core_stop_consolidation_ctx(current_ctx_);
        consolidation_started() = false;
    }
}

void DaemonServer::pauseSim() {
    sim_paused() = !sim_paused().load();
}

void DaemonServer::stepSim(uint32_t num_ticks) {
    if (!_segments()) return;
    if (!sim_running().load()) {
        sim_running() = true;
        sim_step_requested() = true;
        sim_step_count() = num_ticks;
        sim_step_only() = true;  // холодный старт → остановиться после шагов
        current_ctx_->sim_thread = std::thread(&DaemonServer::simulationLoop, this, current_ctx_);
    } else if (sim_paused().load()) {
        sim_step_requested() = true;
        sim_step_count() = num_ticks;
        // шаг из паузы → sim_paused()=true удержит цикл в паузе после шага
    }
}

void DaemonServer::requestShutdown() { shutdown_ = true; }

// ── Stimuli ──────────────────────────────────────────────────────────────

void DaemonServer::addCurrentStimulus(uint32_t seg_id, float amp_pA, float dur_ms, uint64_t start_tick) {
    std::lock_guard<std::mutex> lock(stim_mutex());
    if (stimulus_count() >= SessionContext::MAX_STIM) return;
    stimuli()[stimulus_count()] = {seg_id, amp_pA, dur_ms, start_tick};
    stimulus_count()++;
}
void DaemonServer::addVoltageClamp(uint32_t seg_id, float target_mV, uint64_t start, uint64_t end) {
    std::lock_guard<std::mutex> lock(stim_mutex());
    if (vclamp_count() >= SessionContext::MAX_STIM) return;
    vclamps()[vclamp_count()] = {seg_id, target_mV, start, end};
    vclamp_count()++;
}
void DaemonServer::addRampStimulus(uint32_t seg_id, float start_pA, float end_pA, uint64_t start, uint64_t end) {
    std::lock_guard<std::mutex> lock(stim_mutex());
    if (ramp_count() >= SessionContext::MAX_STIM) return;
    ramps()[ramp_count()] = {seg_id, start_pA, end_pA, start, end};
    ramp_count()++;
}
void DaemonServer::addIonInjection(uint32_t seg_id, int ion_idx, float amt, bool extracellular) {
    std::lock_guard<std::mutex> lock(stim_mutex());
    if (ion_inj_count() >= SessionContext::MAX_STIM) return;
    ion_inj()[ion_inj_count()] = {seg_id, (uint8_t)ion_idx, amt, (uint8_t)(extracellular ? 1 : 0), {0,0,0}};
    ion_inj_count()++;
}
void DaemonServer::addLigandInjection(uint32_t seg_id, int nt_idx, float amt, bool intracellular) {
    std::lock_guard<std::mutex> lock(stim_mutex());
    if (lig_inj_count() >= SessionContext::MAX_STIM) return;
    lig_inj()[lig_inj_count()] = {seg_id, (uint8_t)nt_idx, amt, (uint8_t)(intracellular ? 1 : 0), {0,0,0}};
    lig_inj_count()++;
}
void DaemonServer::clearStimuli() {
    std::lock_guard<std::mutex> lock(stim_mutex());
    stimulus_count() = 0;
    vclamp_count()   = 0;
    ramp_count()     = 0;
    ion_inj_count()  = 0;
    lig_inj_count()  = 0;
}

// ==================== SIMULATION LOOP ====================

void DaemonServer::simulationLoop(SessionContext* ctx) {
    const float dt = config_.default_dt;
    auto lastTime = std::chrono::steady_clock::now();

    while (ctx->sim_running.load()) {
        if (ctx->sim_paused.load() && !ctx->sim_step_requested.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            lastTime = std::chrono::steady_clock::now();
            continue;
        }

        auto now = std::chrono::steady_clock::now();
        float elapsed = std::chrono::duration<float, std::milli>(now - lastTime).count();

        int ticks_to_run;
        if (ctx->sim_step_requested.load()) {
            ticks_to_run = (int)ctx->sim_step_count;
        } else if (ctx->sim_target_hz > 0.0f) {
            ticks_to_run = 1;
        } else {
            ticks_to_run = (int)(elapsed * ctx->sim_multiplier);
            if (ticks_to_run < 1) ticks_to_run = 1;
            if (ticks_to_run > 100) ticks_to_run = 100;
        }

        for (int t = 0; t < ticks_to_run; t++) {
            // Copy stimulus buffers under lock
            CurrentStimulus stim_buf[SessionContext::MAX_STIM];
            uint32_t         stim_cnt = 0;
            VoltageClamp     vc_buf[SessionContext::MAX_STIM];
            uint32_t         vc_cnt = 0;
            RampStimulus     rm_buf[SessionContext::MAX_STIM];
            uint32_t         rm_cnt = 0;
            IonInjection     io_buf[SessionContext::MAX_STIM];
            uint32_t         io_cnt = 0;
            LigandInjection  lg_buf[SessionContext::MAX_STIM];
            uint32_t         lg_cnt = 0;
            {
                std::lock_guard<std::mutex> lock(ctx->stim_mutex);
                stim_cnt = ctx->stimulus_count;
                if (stim_cnt > 0) memcpy(stim_buf, ctx->stimuli, stim_cnt * sizeof(CurrentStimulus));
                vc_cnt = ctx->vclamp_count;
                if (vc_cnt > 0) memcpy(vc_buf, ctx->vclamps, vc_cnt * sizeof(VoltageClamp));
                rm_cnt = ctx->ramp_count;
                if (rm_cnt > 0) memcpy(rm_buf, ctx->ramps, rm_cnt * sizeof(RampStimulus));
                io_cnt = ctx->ion_inj_count;
                if (io_cnt > 0) memcpy(io_buf, ctx->ion_inj, io_cnt * sizeof(IonInjection));
                lg_cnt = ctx->lig_inj_count;
                if (lg_cnt > 0) memcpy(lg_buf, ctx->lig_inj, lg_cnt * sizeof(LigandInjection));
                ctx->ion_inj_count = 0;
                ctx->lig_inj_count = 0;
            }

            std::lock_guard<std::mutex> lock(ctx->sim_mutex);

            // GPU-путь
            auto* bh = static_cast<GpuBackendHandle*>(ctx->gpu_backend_handle);
            if (ctx->sim_backend.load() == 1 && bh && bh->run_tick_ && ctx->gpu_sim) {
                // Применяем стимулы перед GPU-тиком (как в Studio simulation_bridge)
                for (uint32_t i = 0; i < ctx->num_segments; i++) {
                    Segment* s = &ctx->segments[i];
                    for (uint32_t v = 0; v < vc_cnt; v++)
                        if (vc_buf[v].segment_id == s->id && ctx->sim_tick_counter >= vc_buf[v].start_tick && ctx->sim_tick_counter < vc_buf[v].end_tick)
                            s->V_m = vc_buf[v].target_mV;
                    for (uint32_t st = 0; st < stim_cnt; st++) {
                        uint64_t dur = (uint64_t)(stim_buf[st].duration_ms / dt);
                        if (stim_buf[st].segment_id == s->id && ctx->sim_tick_counter >= stim_buf[st].start_tick && ctx->sim_tick_counter < stim_buf[st].start_tick + dur)
                            s->I_buffer += stim_buf[st].amplitude_pA * 1e-12f;
                    }
                    for (uint32_t r = 0; r < rm_cnt; r++) {
                        if (rm_buf[r].segment_id == s->id && ctx->sim_tick_counter >= rm_buf[r].start_tick && ctx->sim_tick_counter < rm_buf[r].end_tick) {
                            float frac = (rm_buf[r].end_tick > rm_buf[r].start_tick) ? (float)(ctx->sim_tick_counter - rm_buf[r].start_tick) / (float)(rm_buf[r].end_tick - rm_buf[r].start_tick) : 1.0f;
                            s->I_buffer += (rm_buf[r].start_pA + (rm_buf[r].end_pA - rm_buf[r].start_pA) * frac) * 1e-12f;
                        }
                    }
                    for (uint32_t j = 0; j < io_cnt; j++)
                        if (io_buf[j].segment_id == s->id && io_buf[j].ion_idx < ION_COUNT)
                            (io_buf[j].extracellular ? s->extra_ions : s->intra_ions)[io_buf[j].ion_idx] += io_buf[j].amount_mM;
                    for (uint32_t l = 0; l < lg_cnt; l++)
                        if (lg_buf[l].segment_id == s->id && lg_buf[l].nt_idx < NT_COUNT)
                            (lg_buf[l].intracellular ? s->intra_nt : s->neurotransmitter)[lg_buf[l].nt_idx] += lg_buf[l].amount;
                }
                bh->run_tick_(ctx->gpu_sim, dt);
                ctx->sim_tick_counter++;
            } else {
                // CPU-путь (per-session изоляция)
                core_run_tick_ctx(ctx,
                              ctx->segments, ctx->num_segments,
                              ctx->connections, ctx->num_connections,
                              ctx->synapses, ctx->num_synapses,
                              0, &ctx->sim_tick_counter, dt,
                              stim_cnt > 0 ? stim_buf : nullptr, stim_cnt > 0 ? &stim_cnt : nullptr,
                              vc_cnt > 0 ? vc_buf : nullptr, vc_cnt > 0 ? &vc_cnt : nullptr,
                              rm_cnt > 0 ? rm_buf : nullptr, rm_cnt > 0 ? &rm_cnt : nullptr,
                              io_cnt > 0 ? io_buf : nullptr, io_cnt > 0 ? &io_cnt : nullptr,
                              lg_cnt > 0 ? lg_buf : nullptr, lg_cnt > 0 ? &lg_cnt : nullptr);
            }

            // Notify streamer
            ctx->streamer.onTick(ctx->sim_tick_counter, ctx->segments, ctx->num_segments,
                                 ctx->synapses, ctx->num_synapses, ctx->sim_tick_counter);

            // Авто-чекпоинт (Stage 5)
            if (ctx->checkpoint_interval > 0 && !ctx->checkpoint_path.empty() &&
                ctx->sim_tick_counter >= ctx->next_checkpoint_tick) {
                saveNetworkForSession(ctx->session_id, ctx->checkpoint_path.c_str());
                ctx->next_checkpoint_tick = ctx->sim_tick_counter + ctx->checkpoint_interval;
            }
        }

        bool was_step = ctx->sim_step_requested.load();
        ctx->sim_step_requested = false;
        if (was_step && ctx->sim_step_only) {
            ctx->sim_running = false;
            ctx->sim_step_only = false;
            break;
        }
        lastTime = now;

        int sleep_ms;
        if (ctx->sim_target_hz > 0.0f) {
            sleep_ms = (int)(1000.0f / ctx->sim_target_hz);
            if (sleep_ms < 100) sleep_ms = 100;
            if (sleep_ms > 5000) sleep_ms = 5000;
        } else {
            sleep_ms = 16;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }
    ctx->sim_running = false;
    DLOG_INFO(" Simulation stopped at tick %lu", (unsigned long)ctx->sim_tick_counter);
}

// ==================== MAIN EVENT LOOP ====================

void DaemonServer::run() {
    DLOG_INFO(" BioNeuronCore Daemon v%s starting...", core_version_string());
    DLOG_INFO(" Press Ctrl+C to stop");

    struct epoll_event events[MAX_EVENTS];
    uint64_t loop_iterations = 0;

    while (!shutdown_.load()) {
        int n = epoll_wait(epoll_fd_, events, MAX_EVENTS, EPOLL_TIMEOUT_MS);
        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;
            if (fd == unix_fd_ || fd == tcp_fd_) {
                acceptClient(fd);
            } else {
                processClientData(fd);
            }
        }

        // Периодический возврат памяти ОС (борьба с glibc-фрагментацией).
        // EPOLL_TIMEOUT_MS=1000ms → ~60 итераций в минуту.
        if (++loop_iterations % 60 == 0) {
            malloc_trim(0);
        }

        // Flush state frames to subscribers for all sessions
        for (auto& [id, ctx] : sessions_) {
            if (ctx && !ctx->streamer.empty()) {
                ctx->streamer.flushSubscribers();
            }
        }
    }

    // Остановка всех сессий
    for (auto& [id, ctx] : sessions_) {
        if (ctx->sim_running.load()) {
            ctx->shutdown = true;
            if (ctx->sim_thread.joinable()) ctx->sim_thread.join();
        }
    }
    DLOG_INFO(" Shutting down.");
}

// ==================== SESSION MANAGEMENT ====================

SessionContext* DaemonServer::getSession(uint32_t session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    return (it != sessions_.end()) ? it->second.get() : nullptr;
}

SessionContext* DaemonServer::createSession(uint32_t session_id, int gpu_device_index) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    // Проверка лимита
    if (sessions_.size() >= config_.max_sessions) {
        DLOG_ERROR("Max sessions (%u) reached", config_.max_sessions);
        return nullptr;
    }

    // Проверка — не существует ли уже
    if (sessions_.count(session_id)) {
        DLOG_ERROR("Session %u already exists", session_id);
        return nullptr;
    }

    // Проверка GPU allowlist
    if (gpu_device_index >= 0 && !config_.gpu_device_allowlist.empty()) {
        bool allowed = false;
        for (int idx : config_.gpu_device_allowlist) {
            if (idx == gpu_device_index) { allowed = true; break; }
        }
        if (!allowed) {
            DLOG_ERROR("GPU device %d not in allowlist", gpu_device_index);
            return nullptr;
        }
    }

    auto ctx = std::make_unique<SessionContext>();
    ctx->session_id = session_id;
    ctx->server = this;
    ctx->gpu_device_index = gpu_device_index;
    memset(ctx->stimuli, 0, sizeof(ctx->stimuli));
    memset(ctx->vclamps, 0, sizeof(ctx->vclamps));
    memset(ctx->ramps,   0, sizeof(ctx->ramps));
    memset(ctx->ion_inj, 0, sizeof(ctx->ion_inj));
    memset(ctx->lig_inj, 0, sizeof(ctx->lig_inj));

    // Создать GPU-симуляцию если нужно
    if (gpu_device_index >= 0 && !gpu_backends_.empty()) {
        // Найти бекенд для этого глобального device_index
        const GpuBackendHandle* bh = backendForDevice(gpu_device_index);
        if (bh) {
            ctx->gpu_backend_handle = (void*)bh;  // сохранить указатель на бекенд
            if (bh->create_on_device_) {
                // Локальный индекс внутри бекенда
                int local_idx = gpu_device_index;
                for (int d = 0; d < gpu_device_index && d < (int)gpu_devices_.size(); d++) {
                    if (strcmp(gpu_devices_[d].backend_type, bh->backend_type.c_str()) != 0)
                        local_idx--;
                }
                ctx->gpu_sim = bh->create_on_device_(local_idx);
            }
            if (!ctx->gpu_sim && bh->create_) {
                ctx->gpu_sim = bh->create_();  // fallback: device 0
            }
        }
    }

    auto* ptr = ctx.get();
    sessions_[session_id] = std::move(ctx);

    if (session_id >= next_session_id_) {
        next_session_id_ = session_id + 1;
    }

    DLOG_INFO("Session %u created (GPU device %d)", session_id, gpu_device_index);
    return ptr;
}

void DaemonServer::destroySession(uint32_t session_id) {
    if (session_id == 0) {
        DLOG_ERROR("Cannot destroy default session 0");
        return;
    }

    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) return;

    auto& ctx = it->second;
    if (ctx->sim_thread.joinable()) {
        ctx->shutdown = true;
        ctx->sim_thread.join();
    }
    ctx->freeNetwork();
    auto* bh = static_cast<GpuBackendHandle*>(ctx->gpu_backend_handle);
    if (ctx->gpu_sim && bh && bh->destroy_) {
        bh->destroy_(ctx->gpu_sim);
        ctx->gpu_sim = nullptr;
    }

    sessions_.erase(it);
    DLOG_INFO("Session %u destroyed", session_id);
}

uint32_t DaemonServer::getClientSessionId(int fd) {
    std::lock_guard<std::mutex> lock(client_session_mutex_);
    auto it = client_session_map_.find(fd);
    return (it != client_session_map_.end()) ? it->second : 0;
}

void DaemonServer::setClientSessionId(int fd, uint32_t sid) {
    std::lock_guard<std::mutex> lock(client_session_mutex_);
    client_session_map_[fd] = sid;
}

// ── Per-session wrappers ──────────────────────────────────────────────────

bool DaemonServer::loadNetworkForSession(uint32_t sid, const char* filename) {
    SessionContext* prev = current_ctx_;
    SessionContext* ctx = getSession(sid);
    if (!ctx) ctx = default_session_;
    current_ctx_ = ctx;
    bool ok = loadNetwork(filename);
    current_ctx_ = prev;
    return ok;
}

void DaemonServer::unloadNetworkForSession(uint32_t sid) {
    SessionContext* prev = current_ctx_;
    SessionContext* ctx = getSession(sid);
    if (!ctx) ctx = default_session_;
    current_ctx_ = ctx;
    unloadNetwork();
    current_ctx_ = prev;
}

bool DaemonServer::saveNetworkForSession(uint32_t sid, const char* filename) {
    SessionContext* prev = current_ctx_;
    SessionContext* ctx = getSession(sid);
    if (!ctx) ctx = default_session_;
    current_ctx_ = ctx;
    bool ok = saveNetwork(filename);
    current_ctx_ = prev;
    return ok;
}

bool DaemonServer::startSimForSession(uint32_t sid, float multiplier, float target_hz) {
    SessionContext* prev = current_ctx_;
    SessionContext* ctx = getSession(sid);
    if (!ctx) ctx = default_session_;
    current_ctx_ = ctx;
    bool ok = startSim(multiplier, target_hz);
    current_ctx_ = prev;
    return ok;
}

void DaemonServer::stopSimForSession(uint32_t sid) {
    SessionContext* prev = current_ctx_;
    SessionContext* ctx = getSession(sid);
    if (!ctx) ctx = default_session_;
    current_ctx_ = ctx;
    stopSim();
    current_ctx_ = prev;
}

void DaemonServer::pauseSimForSession(uint32_t sid) {
    SessionContext* prev = current_ctx_;
    SessionContext* ctx = getSession(sid);
    if (!ctx) ctx = default_session_;
    current_ctx_ = ctx;
    pauseSim();
    current_ctx_ = prev;
}

void DaemonServer::stepSimForSession(uint32_t sid, uint32_t num_ticks) {
    SessionContext* prev = current_ctx_;
    SessionContext* ctx = getSession(sid);
    if (!ctx) ctx = default_session_;
    current_ctx_ = ctx;
    stepSim(num_ticks);
    current_ctx_ = prev;
}
