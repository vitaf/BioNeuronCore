// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#pragma once
// daemon_state.h — state streaming engine: subscriptions, frame serialization.
#include "types.h"
#include "daemon_protocol.h"
#include <vector>
#include <mutex>
#include <atomic>
#include <cstdint>

struct ClientSubscription {
    int fd;
    uint32_t interval_ticks;   // 0 = every tick
    uint64_t last_sent_tick;
    bool delta_enabled;
};

class StateStreamer {
public:
    StateStreamer();
    ~StateStreamer();

    void addSubscriber(int fd, uint32_t interval_ticks, bool delta);
    void removeSubscriber(int fd);
    bool hasSubscriber(int fd) const;

    // Called from simulation thread after each tick batch
    void onTick(uint64_t tick, Segment* segs, uint32_t num_segs,
                Synapse* syns, uint32_t num_syns, uint64_t total_ticks);

    // Set the raw array pointers once (updated in-place by sim)
    void setDataPtrs(Segment* segs, uint32_t num_segs,
                     Synapse* syns, uint32_t num_syns);

    // Build and send frames to all eligible subscribers (called from main thread after epoll_wait)
    void flushSubscribers();

    bool empty() const;

private:
    mutable std::mutex mutex_;
    std::vector<ClientSubscription> subs_;

    // Cached data pointers (updated in-place by sim thread)
    Segment* segments_ = nullptr;
    Synapse* synapses_ = nullptr;
    uint32_t num_segments_ = 0;
    uint32_t num_synapses_ = 0;

    // Latest tick from simulation
    std::atomic<uint64_t> current_tick_{0};
    std::atomic<uint64_t> total_ticks_{0};

    // Pre-built frame buffer for full frames
    std::vector<uint8_t> frame_buf_;

    void buildFullFrame(std::vector<uint8_t>& buf, uint64_t tick);
    bool sendBuffer(int fd, const uint8_t* data, size_t len);

    static constexpr size_t SEGMENT_SIZE = sizeof(Segment);    // 988
    static constexpr size_t SYNAPSE_SIZE = sizeof(Synapse);    // 824
};
