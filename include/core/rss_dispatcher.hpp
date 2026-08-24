#pragma once

#include "core/packet.hpp"
#include <cstdint>
#include <vector>

namespace aetherplane {

/**
 * @brief Receive Side Scaling (RSS) 4-Tuple Symmetric Hash Dispatcher.
 * Uses a Toeplitz / Murmur3 hash over (src_ip, dest_ip, src_port, dest_port)
 * to distribute packet processing across multi-core worker ring queues.
 */
class RSSDispatcher {
public:
    explicit RSSDispatcher(uint32_t num_worker_cores);

    uint32_t compute_rss_hash(const openpath::Packet& pkt) const;
    uint32_t dispatch_core(const openpath::Packet& pkt) const;

    uint32_t num_cores() const { return num_cores_; }

private:
    uint32_t num_cores_;
    uint32_t rss_key_[10]; // 40-byte standard RSS Toeplitz key
};

} // namespace aetherplane
