#pragma once

#include "core/packet.hpp"
#include "core/ring_buffer.hpp"
#include <array>
#include <queue>
#include <mutex>
#include <chrono>

namespace openpath {

constexpr size_t NUM_QOS_QUEUES = 5; // Matches TrafficClass enum count
constexpr size_t QOS_QUEUE_DEPTH = 256;

struct QueueStats {
    uint64_t enqueued_packets{0};
    uint64_t dequeued_packets{0};
    uint64_t dropped_packets{0};
    uint64_t current_occupancy{0};
    double current_delay_ms{0.0};
};

class SmartQoSScheduler {
public:
    SmartQoSScheduler();

    bool enqueue(const Packet& pkt);
    bool dequeue(Packet& pkt);

    void set_wireless_link_rate_mbps(double mbps);
    void update_congestion_state();

    std::array<QueueStats, NUM_QOS_QUEUES> get_stats() const;
    bool is_bufferbloat_detected() const { return bufferbloat_detected_; }
    double estimated_queue_latency_ms() const { return total_latency_ms_; }

private:
    std::array<std::deque<Packet>, NUM_QOS_QUEUES> queues_;
    std::array<uint32_t, NUM_QOS_QUEUES> weights_;
    std::array<int32_t, NUM_QOS_QUEUES> deficits_;
    std::array<QueueStats, NUM_QOS_QUEUES> stats_;

    mutable std::mutex queue_mutex_;
    double link_rate_mbps_{100.0}; // Default 100 Mbps simulated wireless bottleneck
    bool bufferbloat_detected_{false};
    double total_latency_ms_{0.0};

    // CoDel / Active Queue Management parameters
    uint64_t target_latency_ns_{5000000}; // 5ms target
    uint64_t interval_ns_{100000000};     // 100ms interval
};

} // namespace openpath
