#include "core/xsk_umem.hpp"
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace aetherplane {

XSKUmemPool::XSKUmemPool() {
    // Allocate 64-byte aligned contiguous hugepage UMEM memory
#if defined(_WIN32)
    umem_buffer_ = static_cast<uint8_t*>(_aligned_malloc(umem_size_, 64));
#else
    if (posix_memalign(reinterpret_cast<void**>(&umem_buffer_), 64, umem_size_) != 0) {
        umem_buffer_ = nullptr;
    }
#endif

    if (!umem_buffer_) {
        throw std::runtime_error("Failed to allocate aligned UMEM buffer");
    }

    std::memset(umem_buffer_, 0, umem_size_);

    // Populate free frame ring with initial frame offsets
    for (size_t i = 0; i < XSK_UMEM_NUM_FRAMES; ++i) {
        uint64_t addr = i * XSK_UMEM_FRAME_SIZE;
        free_frame_ring_.push(addr);
    }
}

XSKUmemPool::~XSKUmemPool() {
    if (umem_buffer_) {
#if defined(_WIN32)
        _aligned_free(umem_buffer_);
#else
        free(umem_buffer_);
#endif
        umem_buffer_ = nullptr;
    }
}

uint8_t* XSKUmemPool::get_frame_ptr(uint64_t addr) {
    if (addr + XSK_UMEM_FRAME_SIZE > umem_size_) {
        return nullptr;
    }
    return umem_buffer_ + addr;
}

const uint8_t* XSKUmemPool::get_frame_ptr(uint64_t addr) const {
    if (addr + XSK_UMEM_FRAME_SIZE > umem_size_) {
        return nullptr;
    }
    return umem_buffer_ + addr;
}

bool XSKUmemPool::alloc_frame(uint64_t& out_addr) {
    return free_frame_ring_.pop(out_addr);
}

void XSKUmemPool::free_frame(uint64_t addr) {
    free_frame_ring_.push(addr);
}

bool XSKUmemPool::enqueue_fill_ring(uint64_t addr) {
    return fill_ring_.push(addr);
}

bool XSKUmemPool::dequeue_rx_ring(XSKFrameDescriptor& desc) {
    return rx_ring_.pop(desc);
}

bool XSKUmemPool::enqueue_tx_ring(const XSKFrameDescriptor& desc) {
    return tx_ring_.push(desc);
}

bool XSKUmemPool::dequeue_completion_ring(uint64_t& out_addr) {
    return completion_ring_.pop(out_addr);
}

} // namespace aetherplane
