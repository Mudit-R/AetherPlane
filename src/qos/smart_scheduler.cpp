#include "qos/smart_scheduler.hpp"
#include <algorithm>

namespace openpath {

SmartQoSScheduler::SmartQoSScheduler() {
    // Priority / Deficit Round Robin weights
    // Q0 (VOICE_CONTROL)  : Strict Priority (Weight 100)
    // Q1 (GAMING_LOW_LAT) : High Priority (Weight 60)
    // Q2 (STREAMING_VIDEO): Medium Priority (Weight 40)
    // Q3 (BEST_EFFORT)    : Normal Priority (Weight 20)
    // Q4 (BULK_BACKGROUND): Low Priority (Weight 5)
    weights_ = {100, 60, 40, 20, 5};
    deficits_ = {0, 0, 0, 0, 0};
}

void SmartQoSScheduler::set_wireless_link_rate_mbps(double mbps) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    link_rate_mbps_ = std::max(1.0, mbps);
}

bool SmartQoSScheduler::enqueue(const Packet& pkt) {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    size_t q_idx = static_cast<size_t>(pkt.meta().traffic_class);
    if (q_idx >= NUM_QOS_QUEUES) {
        q_idx = static_cast<size_t>(TrafficClass::BEST_EFFORT);
    }

    auto& q = queues_[q_idx];
    auto& st = stats_[q_idx];

    // Check if bufferbloat / Active Queue Management drop condition is triggered
    if (q.size() >= QOS_QUEUE_DEPTH) {
        st.dropped_packets++;
        return false; // Tail drop under extreme saturation
    }

    q.push_back(pkt);
    st.enqueued_packets++;
    st.current_occupancy = q.size();

    update_congestion_state();
    return true;
}

bool SmartQoSScheduler::dequeue(Packet& pkt) {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    // 1. Strict Priority for Q0 (VOICE_CONTROL)
    if (!queues_[0].empty()) {
        pkt = queues_[0].front();
        queues_[0].pop_front();
        stats_[0].dequeued_packets++;
        stats_[0].current_occupancy = queues_[0].size();
        return true;
    }

    // 2. Deficit Round Robin for Q1 - Q4
    for (size_t i = 1; i < NUM_QOS_QUEUES; ++i) {
        deficits_[i] += weights_[i];

        while (!queues_[i].empty() && deficits_[i] >= static_cast<int32_t>(queues_[i].front().size())) {
            pkt = queues_[i].front();
            queues_[i].pop_front();

            deficits_[i] -= static_cast<int32_t>(pkt.size());
            stats_[i].dequeued_packets++;
            stats_[i].current_occupancy = queues_[i].size();
            return true;
        }

        if (queues_[i].empty()) {
            deficits_[i] = 0; // Reset deficit if empty
        }
    }

    return false; // All queues empty
}

void SmartQoSScheduler::update_congestion_state() {
    size_t total_buffered_bytes = 0;
    for (size_t i = 0; i < NUM_QOS_QUEUES; ++i) {
        for (const auto& pkt : queues_[i]) {
            total_buffered_bytes += pkt.size();
        }
    }

    // Delay = (Buffered Bits) / (Link Rate in bps)
    double delay_sec = (total_buffered_bytes * 8.0) / (link_rate_mbps_ * 1000000.0);
    total_latency_ms_ = delay_sec * 1000.0;

    // Bufferbloat condition: Queue latency exceeds 25ms threshold on bottlenecked wireless link
    bufferbloat_detected_ = (total_latency_ms_ > 25.0);

    for (size_t i = 0; i < NUM_QOS_QUEUES; ++i) {
        size_t q_bytes = 0;
        for (const auto& p : queues_[i]) q_bytes += p.size();
        stats_[i].current_delay_ms = (q_bytes * 8.0) / (link_rate_mbps_ * 1000000.0) * 1000.0;
    }
}

std::array<QueueStats, NUM_QOS_QUEUES> SmartQoSScheduler::get_stats() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return stats_;
}

} // namespace openpath
