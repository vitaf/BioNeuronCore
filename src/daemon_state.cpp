// Copyright (c) 2026 Фешин Виталий Александрович / Vitaly Alexandrovich Feshin.
// E-mail: feshin2004@mail.ru, vfeshin@mindcopy.ru. Тел.: +7(918)345-75-18.
// Краснодар, Россия / Krasnodar, Russia. Все права защищены. All rights reserved.

#include "daemon_state.h"
#include <unistd.h>
#include <sys/uio.h>
#include <cstring>
#include <algorithm>
#include <cerrno>
#include <poll.h>

StateStreamer::StateStreamer() = default;
StateStreamer::~StateStreamer() = default;

void StateStreamer::addSubscriber(int fd, uint32_t interval_ticks, bool delta) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& s : subs_) {
        if (s.fd == fd) {
            s.interval_ticks = interval_ticks;
            s.delta_enabled = delta;
            s.last_sent_tick = 0;
            return;
        }
    }
    ClientSubscription cs;
    cs.fd = fd;
    cs.interval_ticks = interval_ticks;
    cs.delta_enabled = delta;
    cs.last_sent_tick = 0;
    subs_.push_back(cs);
}

void StateStreamer::removeSubscriber(int fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    subs_.erase(
        std::remove_if(subs_.begin(), subs_.end(),
                       [fd](const ClientSubscription& s) { return s.fd == fd; }),
        subs_.end());
}

bool StateStreamer::hasSubscriber(int fd) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& s : subs_)
        if (s.fd == fd) return true;
    return false;
}

void StateStreamer::setDataPtrs(Segment* segs, uint32_t num_segs,
                                 Synapse* syns, uint32_t num_syns) {
    std::lock_guard<std::mutex> lock(mutex_);
    segments_ = segs;
    synapses_ = syns;
    num_segments_ = num_segs;
    num_synapses_ = num_syns;
}

void StateStreamer::onTick(uint64_t tick, Segment* segs, uint32_t num_segs,
                            Synapse* syns, uint32_t num_syns, uint64_t total_ticks) {
    current_tick_.store(tick, std::memory_order_release);
    total_ticks_.store(total_ticks, std::memory_order_release);
    // Pointers are refreshed (they may change on reload)
    setDataPtrs(segs, num_segs, syns, num_syns);
}

void StateStreamer::buildFullFrame(std::vector<uint8_t>& buf, uint64_t tick) {
    // Layout: tick:u64 + num_segments:u32 + num_synapses:u32 + raw segments + raw synapses
    size_t total = 8 + 4 + 4 + (size_t)num_segments_ * SEGMENT_SIZE + (size_t)num_synapses_ * SYNAPSE_SIZE;
    buf.resize(total);
    uint8_t* p = buf.data();
    using namespace biocore_proto;
    write_u64(p, tick);
    write_u32(p, num_segments_);
    write_u32(p, num_synapses_);
    if (num_segments_ > 0)
        write_bytes(p, segments_, num_segments_ * SEGMENT_SIZE);
    if (num_synapses_ > 0)
        write_bytes(p, synapses_, num_synapses_ * SYNAPSE_SIZE);
}

bool StateStreamer::sendBuffer(int fd, const uint8_t* data, size_t len) {
    uint8_t header[8];
    {
        uint8_t* p = header;
        using namespace biocore_proto;
        write_u32(p, MSG_STATE_FRAME);
        write_u32(p, (uint32_t)len);
    }
    struct iovec iov[2];
    iov[0].iov_base = (void*)header;
    iov[0].iov_len  = 8;
    iov[1].iov_base = (void*)data;
    iov[1].iov_len  = len;
    ssize_t total = 8 + (ssize_t)len;
    ssize_t written = 0;
    while (written < total) {
        ssize_t n = writev(fd, iov, 2);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                struct pollfd pfd = {fd, POLLOUT, 0};
                poll(&pfd, 1, 100);
                continue;
            }
            return false;
        }
        if (n == 0) return false;
        written += n;
        if (written >= 8) {
            size_t payload_offset = (size_t)(written - 8);
            iov[1].iov_base = (uint8_t*)data + payload_offset;
            iov[1].iov_len  = len - payload_offset;
            iov[0].iov_len  = 0;
        } else {
            iov[0].iov_base = (uint8_t*)header + written;
            iov[0].iov_len  = 8 - (size_t)written;
        }
    }
    return true;
}

void StateStreamer::flushSubscribers() {
    uint64_t tick = current_tick_.load(std::memory_order_acquire);
    if (tick == 0) return;

    std::lock_guard<std::mutex> lock(mutex_);
    if (subs_.empty() || num_segments_ == 0) return;

    // Build full frame once
    buildFullFrame(frame_buf_, tick);

    for (auto& sub : subs_) {
        uint64_t delta = (tick >= sub.last_sent_tick) ? (tick - sub.last_sent_tick) : 0;
        if (delta >= sub.interval_ticks) {
            // Frame already includes the header via sendBuffer
            sendBuffer(sub.fd, frame_buf_.data(), frame_buf_.size());
            sub.last_sent_tick = tick;
        }
    }
}

bool StateStreamer::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return subs_.empty();
}
