#pragma once

#include <atomic>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <new>

namespace openpath {

// Cache line size for modern x86/ARM processors
constexpr size_t CACHE_LINE_SIZE = 64;

/**
 * @brief High-performance Lock-Free Single-Producer Single-Consumer (SPSC) Ring Buffer.
 * Emulates DPDK's rte_ring for zero-copy, microsecond packet handoffs between RX/TX cores.
 */
template <typename T, size_t Capacity>
class SPSCRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

public:
    SPSCRingBuffer() : head_(0), tail_(0) {}

    bool push(const T& item) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t current_head = head_.load(std::memory_order_acquire);

        if ((current_tail - current_head) >= Capacity) {
            return false; // Queue Full
        }

        buffer_[current_tail & (Capacity - 1)] = item;
        tail_.store(current_tail + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        const size_t current_tail = tail_.load(std::memory_order_acquire);

        if (current_head == current_tail) {
            return false; // Queue Empty
        }

        item = buffer_[current_head & (Capacity - 1)];
        head_.store(current_head + 1, std::memory_order_release);
        return true;
    }

    size_t size() const {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t current_head = head_.load(std::memory_order_relaxed);
        return (current_tail >= current_head) ? (current_tail - current_head) : 0;
    }

    bool empty() const {
        return size() == 0;
    }

    size_t capacity() const {
        return Capacity;
    }

private:
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_{0};
    alignas(CACHE_LINE_SIZE) T buffer_[Capacity];
};

} // namespace openpath
