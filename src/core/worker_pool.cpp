#include "core/worker_pool.hpp"
#include <iostream>

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#include <sched.h>
#endif

namespace aetherplane {

WorkerPool::WorkerPool(uint32_t num_workers)
    : num_workers_(num_workers > 0 ? num_workers : 1),
      rss_dispatcher_(num_workers_),
      metrics_(num_workers_) {
    for (uint32_t i = 0; i < num_workers_; ++i) {
        worker_rings_.push_back(std::make_unique<openpath::SPSCRingBuffer<openpath::Packet, 1024>>());
    }
}

WorkerPool::~WorkerPool() {
    stop();
}

void WorkerPool::pin_thread_to_core(uint32_t core_id) {
#if defined(_WIN32)
    DWORD_PTR mask = static_cast<DWORD_PTR>(1) << (core_id % 64);
    SetThreadAffinityMask(GetCurrentThread(), mask);
#else
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id % CPU_SETSIZE, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
}

void WorkerPool::start() {
    running_ = true;
    for (uint32_t i = 0; i < num_workers_; ++i) {
        worker_threads_.emplace_back(&WorkerPool::worker_loop, this, i);
    }
}

void WorkerPool::stop() {
    running_ = false;
    for (auto& t : worker_threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    worker_threads_.clear();
}

bool WorkerPool::dispatch_packet(const openpath::Packet& pkt) {
    uint32_t target_core = rss_dispatcher_.dispatch_core(pkt);
    if (target_core < num_workers_) {
        return worker_rings_[target_core]->push(pkt);
    }
    return false;
}

void WorkerPool::worker_loop(uint32_t core_id) {
    pin_thread_to_core(core_id);
    auto& ring = *worker_rings_[core_id];
    auto& metric = metrics_[core_id];

    openpath::Packet pkt;
    while (running_) {
        if (ring.pop(pkt)) {
            metric.processed_packets.fetch_add(1, std::memory_order_relaxed);
            metric.total_bytes.fetch_add(pkt.size(), std::memory_order_relaxed);
        } else {
            // Adaptive spin-pause to minimize power & cacheline bouncing
            std::this_thread::yield();
        }
    }
}

} // namespace aetherplane
