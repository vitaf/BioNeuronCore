// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#pragma once
// daemon_server.h — Core Daemon event loop: epoll, accept, dispatch, multi-session.
#include "daemon_config.h"
#include "daemon_state.h"
#include "daemon_session.h"
#include "types.h"
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <string>
#include <unordered_map>
#include <memory>

struct ClientConnection {
    int fd = -1;
    std::vector<uint8_t> recv_buf;
    size_t recv_wanted = 0;
    uint32_t recv_msg_type = 0;
    uint32_t recv_payload_len = 0;
    uint32_t current_session_id = 0;
};

class DaemonServer {
public:
    DaemonServer();
    ~DaemonServer();

    bool init(const DaemonConfig& cfg);
    void run();
    void requestShutdown();

    // ── Старые методы (обратная совместимость, реализация в .cpp) ──
    bool loadNetwork(const char* filename);
    void unloadNetwork();
    bool isNetworkLoaded() const { return current_ctx_ && current_ctx_->segments != nullptr; }
    bool saveNetwork(const char* filename);
    void freeNetwork();
    bool startSim(float multiplier, float target_hz);
    void stopSim();
    void pauseSim();
    void stepSim(uint32_t num_ticks);
    bool isRunning() const;
    bool isPaused() const;

    Segment*    segs()       { return current_ctx_ ? current_ctx_->segments : nullptr; }
    uint32_t    numSegs()    { return current_ctx_ ? current_ctx_->num_segments : 0; }
    Synapse*    syns()       { return current_ctx_ ? current_ctx_->synapses : nullptr; }
    uint32_t    numSyns()    { return current_ctx_ ? current_ctx_->num_synapses : 0; }
    IntraConnection* conns() { return current_ctx_ ? current_ctx_->connections : nullptr; }
    uint32_t    numConns()   { return current_ctx_ ? current_ctx_->num_connections : 0; }
    Neuron*     neurons()    { return current_ctx_ ? current_ctx_->neurons : nullptr; }
    uint32_t    numNeurons() { return current_ctx_ ? current_ctx_->num_neurons : 0; }
    Nucleus*    nuclei()     { return current_ctx_ ? current_ctx_->nuclei : nullptr; }
    uint32_t    simMode()    { return current_ctx_ ? current_ctx_->loaded_mode : 0; }
    StateStreamer& streamer() { return current_ctx_->streamer; }

    void addCurrentStimulus(uint32_t seg_id, float amp_pA, float dur_ms, uint64_t start_tick);
    void addVoltageClamp(uint32_t seg_id, float target_mV, uint64_t start, uint64_t end);
    void addRampStimulus(uint32_t seg_id, float start_pA, float end_pA, uint64_t start, uint64_t end);
    void addIonInjection(uint32_t seg_id, int ion_idx, float amt, bool extracellular);
    void addLigandInjection(uint32_t seg_id, int nt_idx, float amt, bool intracellular);
    void clearStimuli();

    bool loadNetworkForSession(uint32_t sid, const char* filename);
    void unloadNetworkForSession(uint32_t sid);
    bool saveNetworkForSession(uint32_t sid, const char* filename);
    bool startSimForSession(uint32_t sid, float multiplier, float target_hz);
    void stopSimForSession(uint32_t sid);
    void pauseSimForSession(uint32_t sid);
    void stepSimForSession(uint32_t sid, uint32_t num_ticks);

    // ── Доступ к контексту текущей сессии ──
    SessionContext* currentCtx() { return current_ctx_; }
    DaemonConfig&  config()     { return config_; }

    // ── Multi-session API ──
    SessionContext* getSession(uint32_t session_id);
    SessionContext* createSession(uint32_t session_id, int gpu_device_index = -1);
    void destroySession(uint32_t session_id);
    uint32_t getClientSessionId(int fd);
    void setClientSessionId(int fd, uint32_t sid);

    // GPU
    struct GpuBackendHandle {
        void* lib_handle = nullptr;
        void* (*create_)() = nullptr;
        void* (*create_on_device_)(int) = nullptr;
        void  (*destroy_)(void*) = nullptr;
        int   (*load_bion_)(void*, const char*) = nullptr;
        void  (*run_tick_)(void*, float) = nullptr;
        void* (*bion_data_)(void*) = nullptr;
        int   (*device_count_)() = nullptr;
        int   (*get_device_info_)(int, GpuDeviceInfo*) = nullptr;
        std::string backend_type;  // "intel" или "cuda"
    };
    const std::vector<GpuDeviceInfo>& gpuDevices() const { return gpu_devices_; }
    bool gpuAvailable() const { return !gpu_backends_.empty(); }
    const GpuBackendHandle* backendForDevice(int global_device_index) const;

private:
    DaemonConfig config_;

    // Listen sockets
    int unix_fd_ = -1;
    int tcp_fd_  = -1;
    int epoll_fd_ = -1;

    std::atomic<bool> shutdown_{false};

    // ── Текущий контекст сессии (устанавливается dispatchMessage) ──
    SessionContext* current_ctx_ = nullptr;
    SessionContext* default_session_ = nullptr;  // сессия 0 по умолчанию

    // ── Сессии ──
    std::unordered_map<uint32_t, std::unique_ptr<SessionContext>> sessions_;
    std::mutex sessions_mutex_;
    uint32_t next_session_id_ = 1;

    // ── Клиент → сессия ──
    std::unordered_map<int, uint32_t> client_session_map_;
    std::mutex client_session_mutex_;

    // ── GPU backend (dlopen, один/два .so) ──
    std::vector<GpuBackendHandle> gpu_backends_;
    std::vector<GpuDeviceInfo>    gpu_devices_;  // плоский список со всех бекендов

    // Connected clients
    std::vector<ClientConnection> clients_;
    std::mutex clients_mutex_;

    // ── Указатели-алиасы для обратной совместимости (перенаправляются на current_ctx_) ──
    // Эти поля существуют только для совместимости с существующим кодом.
    // В dispatchMessage current_ctx_ устанавливается в нужную сессию.
    Segment*&         _segments()     { return current_ctx_->segments; }
    Synapse*&         _synapses()     { return current_ctx_->synapses; }
    IntraConnection*& _connections()  { return current_ctx_->connections; }
    Neuron*&          _neurons()      { return current_ctx_->neurons; }
    Nucleus*&         _nuclei()       { return current_ctx_->nuclei; }
    uint32_t&         num_segments()    { return current_ctx_->num_segments; }
    uint32_t&         num_synapses()    { return current_ctx_->num_synapses; }
    uint32_t&         num_connections() { return current_ctx_->num_connections; }
    uint32_t&         num_neurons()     { return current_ctx_->num_neurons; }
    uint32_t&         num_nuclei()      { return current_ctx_->num_nuclei; }

    std::atomic<bool>& sim_running()     { return current_ctx_->sim_running; }
    std::atomic<bool>& sim_paused()      { return current_ctx_->sim_paused; }
    std::atomic<int>&  sim_backend()     { return current_ctx_->sim_backend; }
    float&             sim_multiplier()  { return current_ctx_->sim_multiplier; }
    float&             sim_target_hz()   { return current_ctx_->sim_target_hz; }
    uint64_t&          sim_tick_counter(){ return current_ctx_->sim_tick_counter; }
    std::mutex&        sim_mutex()       { return current_ctx_->sim_mutex; }
    std::atomic<bool>& sim_step_requested() { return current_ctx_->sim_step_requested; }
    uint32_t&          sim_step_count()  { return current_ctx_->sim_step_count; }
    bool&              sim_step_only()   { return current_ctx_->sim_step_only; }

    void*&            gpu_sim()          { return current_ctx_->gpu_sim; }
    bool&             simDataFromGpu()   { return current_ctx_->simDataFromGpu; }

    // Стимулы
    CurrentStimulus*   stimuli()         { return current_ctx_->stimuli; }
    uint32_t&          stimulus_count()  { return current_ctx_->stimulus_count; }
    VoltageClamp*      vclamps()         { return current_ctx_->vclamps; }
    uint32_t&          vclamp_count()    { return current_ctx_->vclamp_count; }
    RampStimulus*      ramps()           { return current_ctx_->ramps; }
    uint32_t&          ramp_count()      { return current_ctx_->ramp_count; }
    IonInjection*      ion_inj()         { return current_ctx_->ion_inj; }
    uint32_t&          ion_inj_count()   { return current_ctx_->ion_inj_count; }
    LigandInjection*   lig_inj()         { return current_ctx_->lig_inj; }
    uint32_t&          lig_inj_count()   { return current_ctx_->lig_inj_count; }
    std::mutex&        stim_mutex()      { return current_ctx_->stim_mutex; }

    bool&              consolidation_started() { return current_ctx_->consolidation_started; }
    std::vector<uint8_t>& initial_nuclei()     { return current_ctx_->initial_nuclei; }

    // ── Internal methods ──
    int  createUnixListener(const char* path);
    int  createTcpListener(const char* host, uint16_t port);
    void acceptClient(int listen_fd);
    void removeClient(int fd);
    void processClientData(int fd);
    void dispatchMessage(int fd, uint32_t type, const uint8_t* payload, uint32_t len);
    void sendResponse(int fd, uint32_t type, const uint8_t* payload, uint32_t len);
    void initDefaultSession();

public:
    void simulationLoop(SessionContext* ctx);

private:

    friend struct SessionContext;
};
