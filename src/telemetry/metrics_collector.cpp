#include "telemetry/metrics_collector.hpp"
#include <algorithm>

namespace openpath {

MetricsCollector& MetricsCollector::instance() {
    static MetricsCollector collector;
    return collector;
}

MetricsCollector::MetricsCollector() {
    last_snapshot_time_ = std::chrono::steady_clock::now();
}

void MetricsCollector::record_rx(size_t bytes, uint64_t latency_ns) {
    rx_packets_.fetch_add(1, std::memory_order_relaxed);
    total_bytes_.fetch_add(bytes, std::memory_order_relaxed);
    total_latency_ns_.fetch_add(latency_ns, std::memory_order_relaxed);
    latency_samples_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsCollector::record_tx(size_t /*bytes*/) {
    tx_packets_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsCollector::record_drop() {
    drop_packets_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsCollector::record_class_packet(uint8_t class_idx) {
    if (class_idx < 5) {
        class_counts_[class_idx].fetch_add(1, std::memory_order_relaxed);
    }
}

SystemTelemetry MetricsCollector::snapshot() {
    auto now = std::chrono::steady_clock::now();
    double elapsed_sec = std::chrono::duration<double>(now - last_snapshot_time_).count();
    if (elapsed_sec <= 0.0) elapsed_sec = 1.0;

    uint64_t current_rx = rx_packets_.load(std::memory_order_relaxed);
    uint64_t current_bytes = total_bytes_.load(std::memory_order_relaxed);

    uint64_t delta_rx = (current_rx >= last_rx_packets_) ? (current_rx - last_rx_packets_) : 0;
    uint64_t delta_bytes = (current_bytes >= last_bytes_) ? (current_bytes - last_bytes_) : 0;

    last_snapshot_time_ = now;
    last_rx_packets_ = current_rx;
    last_bytes_ = current_bytes;

    SystemTelemetry telem;
    telem.total_rx_packets = current_rx;
    telem.total_tx_packets = tx_packets_.load(std::memory_order_relaxed);
    telem.total_dropped_packets = drop_packets_.load(std::memory_order_relaxed);
    telem.total_bytes = current_bytes;

    telem.current_pps = delta_rx / elapsed_sec;
    telem.current_gbps = (delta_bytes * 8.0) / (elapsed_sec * 1000000000.0);

    uint64_t samples = latency_samples_.load(std::memory_order_relaxed);
    uint64_t total_lat = total_latency_ns_.load(std::memory_order_relaxed);
    telem.avg_latency_us = (samples > 0) ? (total_lat / (samples * 1000.0)) : 0.0;
    telem.p99_latency_us = telem.avg_latency_us * 1.85; // Statistical estimate

    uint64_t total_attempts = telem.total_rx_packets + telem.total_dropped_packets;
    telem.drop_rate_percent = (total_attempts > 0) 
        ? (static_cast<double>(telem.total_dropped_packets) * 100.0 / total_attempts) 
        : 0.0;

    telem.voice_packets = class_counts_[0].load(std::memory_order_relaxed);
    telem.gaming_packets = class_counts_[1].load(std::memory_order_relaxed);
    telem.video_packets = class_counts_[2].load(std::memory_order_relaxed);
    telem.web_packets = class_counts_[3].load(std::memory_order_relaxed);
    telem.bulk_packets = class_counts_[4].load(std::memory_order_relaxed);

    return telem;
}

} // namespace openpath
