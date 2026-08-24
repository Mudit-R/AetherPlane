#pragma once

#include <cstdint>
#include <atomic>
#include <string>
#include <chrono>

namespace openpath {

struct SystemTelemetry {
    uint64_t total_rx_packets{0};
    uint64_t total_tx_packets{0};
    uint64_t total_dropped_packets{0};
    uint64_t total_bytes{0};

    double current_pps{0.0};
    double current_gbps{0.0};
    double avg_latency_us{0.0};
    double p99_latency_us{0.0};
    double drop_rate_percent{0.0};
    bool bufferbloat_active{false};

    uint64_t voice_packets{0};
    uint64_t gaming_packets{0};
    uint64_t video_packets{0};
    uint64_t web_packets{0};
    uint64_t bulk_packets{0};
};

class MetricsCollector {
public:
    static MetricsCollector& instance();

    void record_rx(size_t bytes, uint64_t latency_ns);
    void record_tx(size_t bytes);
    void record_drop();
    void record_class_packet(uint8_t class_idx);

    SystemTelemetry snapshot();

private:
    MetricsCollector();

    std::atomic<uint64_t> rx_packets_{0};
    std::atomic<uint64_t> tx_packets_{0};
    std::atomic<uint64_t> drop_packets_{0};
    std::atomic<uint64_t> total_bytes_{0};

    std::atomic<uint64_t> total_latency_ns_{0};
    std::atomic<uint64_t> latency_samples_{0};

    std::atomic<uint64_t> class_counts_[5]{};

    std::chrono::steady_clock::time_point last_snapshot_time_;
    uint64_t last_rx_packets_{0};
    uint64_t last_bytes_{0};
};

} // namespace openpath
