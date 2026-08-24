#pragma once

#include "core/packet.hpp"
#include <cstdint>
#include <vector>
#include <deque>
#include <array>
#include <mutex>
#include <chrono>

namespace aetherplane {

constexpr size_t FQ_CODEL_NUM_FLOWS = 1024;
constexpr uint32_t FQ_CODEL_QUANTUM = 1514;      // MTU deficit quantum per round
constexpr uint64_t CODEL_TARGET_NS  = 5000000;   // 5ms target sojourn delay
constexpr uint64_t CODEL_INTERVAL_NS= 100000000; // 100ms sliding observation window

struct CoDelState {
    uint64_t first_above_time_ns{0};
    uint64_t drop_next_ns{0};
    uint32_t count{0};
    bool dropping{false};
};

struct FlowQueue {
    uint32_t flow_id{0};
    int32_t  deficit{0};
    std::deque<openpath::Packet> packets;
    CoDelState codel;
    uint64_t total_bytes{0};
    uint64_t drops{0};
    bool is_sparse{true};
};

class FQCoDelScheduler {
public:
    FQCoDelScheduler();

    bool enqueue(const openpath::Packet& pkt);
    bool dequeue(openpath::Packet& pkt);

    void set_link_rate_mbps(double mbps);
    bool is_bufferbloat_active() const { return bufferbloat_active_; }
    double mean_sojourn_delay_ms() const { return current_delay_ms_; }
    uint64_t total_aqm_drops() const { return total_drops_; }

private:
    std::array<FlowQueue, FQ_CODEL_NUM_FLOWS> flows_;
    std::deque<uint32_t> new_flows_;
    std::deque<uint32_t> old_flows_;

    mutable std::mutex scheduler_mutex_;
    double link_rate_mbps_{100.0};
    bool bufferbloat_active_{false};
    double current_delay_ms_{0.0};
    uint64_t total_drops_{0};

    uint32_t hash_flow(const openpath::Packet& pkt) const;
    bool codel_should_drop(FlowQueue& flow, const openpath::Packet& pkt, uint64_t now_ns);
    uint64_t control_law(uint64_t t, uint32_t count);
};

} // namespace aetherplane
