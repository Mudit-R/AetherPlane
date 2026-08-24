#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <atomic>
#include <memory>
#include "core/ring_buffer.hpp"

namespace aetherplane {

constexpr size_t XSK_UMEM_FRAME_SIZE = 2048; // 2KB frames (MTU + headroom)
constexpr size_t XSK_UMEM_NUM_FRAMES = 4096; // 8MB memory pool

struct XSKFrameDescriptor {
    uint64_t addr;      // Byte offset within UMEM
    uint32_t len;       // Packet payload length
    uint32_t flags;     // RX/TX flags
};

/**
 * @brief AF_XDP Zero-Copy UMEM Frame Pool Manager.
 * Emulates Linux kernel AF_XDP (XSK) memory-mapped UMEM registration,
 * Fill Ring, Rx Ring, Tx Ring, and Completion Ring semantics.
 */
class XSKUmemPool {
public:
    XSKUmemPool();
    ~XSKUmemPool();

    // Disable copy
    XSKUmemPool(const XSKUmemPool&) = delete;
    XSKUmemPool& operator=(const XSKUmemPool&) = delete;

    uint8_t* get_frame_ptr(uint64_t addr);
    const uint8_t* get_frame_ptr(uint64_t addr) const;

    // Fast-path frame allocation (Lock-free O(1))
    bool alloc_frame(uint64_t& out_addr);
    void free_frame(uint64_t addr);

    // XSK Ring operations
    bool enqueue_fill_ring(uint64_t addr);
    bool dequeue_rx_ring(XSKFrameDescriptor& desc);

    bool enqueue_tx_ring(const XSKFrameDescriptor& desc);
    bool dequeue_completion_ring(uint64_t& out_addr);

    size_t available_frames() const { return free_frame_ring_.size(); }
    size_t total_frames() const { return XSK_UMEM_NUM_FRAMES; }

private:
    uint8_t* umem_buffer_{nullptr};
    size_t umem_size_{XSK_UMEM_NUM_FRAMES * XSK_UMEM_FRAME_SIZE};

    openpath::SPSCRingBuffer<uint64_t, XSK_UMEM_NUM_FRAMES> free_frame_ring_;
    openpath::SPSCRingBuffer<uint64_t, XSK_UMEM_NUM_FRAMES> fill_ring_;
    openpath::SPSCRingBuffer<XSKFrameDescriptor, XSK_UMEM_NUM_FRAMES> rx_ring_;
    openpath::SPSCRingBuffer<XSKFrameDescriptor, XSK_UMEM_NUM_FRAMES> tx_ring_;
    openpath::SPSCRingBuffer<uint64_t, XSK_UMEM_NUM_FRAMES> completion_ring_;
};

} // namespace aetherplane
