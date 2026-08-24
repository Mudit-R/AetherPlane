#include "core/rss_dispatcher.hpp"

namespace aetherplane {

RSSDispatcher::RSSDispatcher(uint32_t num_worker_cores)
    : num_cores_(num_worker_cores > 0 ? num_worker_cores : 1) {
    // Standard Microsoft / DPDK RSS 40-byte hash key
    rss_key_[0] = 0x6D5A56DA;
    rss_key_[1] = 0x255B0EC2;
    rss_key_[2] = 0x4167253D;
    rss_key_[3] = 0x43A38FB0;
    rss_key_[4] = 0xD0CA2BCB;
    rss_key_[5] = 0xAE7B30B4;
    rss_key_[6] = 0x77CB2DA3;
    rss_key_[7] = 0x8030F20C;
    rss_key_[8] = 0x6A42B73B;
    rss_key_[9] = 0xBEAC01FA;
}

uint32_t RSSDispatcher::compute_rss_hash(const openpath::Packet& pkt) const {
    const auto& meta = pkt.meta();
    
    // Murmur-inspired symmetric 4-tuple hash
    uint32_t h = 0x811C9DC5;
    uint32_t ip_min = std::min(meta.src_ip, meta.dest_ip);
    uint32_t ip_max = std::max(meta.src_ip, meta.dest_ip);
    uint16_t port_min = std::min(meta.src_port, meta.dest_port);
    uint16_t port_max = std::max(meta.src_port, meta.dest_port);

    h ^= ip_min;
    h *= 0x01000193;
    h ^= ip_max;
    h *= 0x01000193;
    h ^= static_cast<uint32_t>(port_min) | (static_cast<uint32_t>(port_max) << 16);
    h *= 0x01000193;
    h ^= static_cast<uint32_t>(meta.l3_proto);
    h *= 0x01000193;

    return h;
}

uint32_t RSSDispatcher::dispatch_core(const openpath::Packet& pkt) const {
    uint32_t hash = compute_rss_hash(pkt);
    return hash % num_cores_;
}

} // namespace aetherplane
