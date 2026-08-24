#pragma once

#include "core/packet.hpp"
#include "core/ring_buffer.hpp"
#include "core/rss_dispatcher.hpp"
#include <vector>
#include <thread>
#include <atomic>
#include <functional>

namespace aetherplane {

struct CoreMetrics {
    std::atomic<uint64_t> processed_packets{0};
    std::atomic<uint64_t> dropped_packets{0};
    std::atomic<uint64_t> total_bytes{0};
    std::atomic<uint64_t> core_cycles{0};
};

class WorkerPool {
public:
    explicit WorkerPool(uint32_t num_workers);
    ~WorkerPool();

    void start();
    void stop();

    bool dispatch_packet(const openpath::Packet& pkt);

    CoreMetrics& get_core_metrics(uint32_t core_id) { return metrics_[core_id]; }
    uint32_t num_workers() const { return num_workers_; }

private:
    uint32_t num_workers_;
    std::atomic<bool> running_{false};
    RSSDispatcher rss_dispatcher_;

    std::vector<std::thread> worker_threads_;
    std::vector<std::unique_ptr<openpath::SPSCRingBuffer<openpath::Packet, 1024>>> worker_rings_;
    std::vector<CoreMetrics> metrics_;

    void worker_loop(uint32_t core_id);
    void pin_thread_to_core(uint32_t core_id);
};

} // namespace aetherplane
