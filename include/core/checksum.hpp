#pragma once

#include <cstdint>
#include <cstddef>

namespace aetherplane {

/**
 * @brief High-performance RFC 1071 Checksum Engine with 64-bit unrolled accumulator
 * and RFC 1624 incremental checksum update mechanics.
 */
class ChecksumEngine {
public:
    static uint16_t compute_checksum(const void* data, size_t len);

    static uint16_t compute_ipv4_checksum(const void* ip_header, size_t len = 20);

    static uint16_t compute_tcp_checksum(uint32_t src_ip, uint32_t dest_ip, 
                                          const void* tcp_data, size_t len);

    static uint16_t compute_udp_checksum(uint32_t src_ip, uint32_t dest_ip, 
                                          const void* udp_data, size_t len);

    /**
     * @brief RFC 1624 Incremental Checksum Update (for fast TTL decrement & NAT)
     * HC' = ~(~HC + ~m + m')
     */
    static uint16_t update_checksum_16(uint16_t old_csum, uint16_t old_val, uint16_t new_val);
    static uint16_t update_checksum_32(uint16_t old_csum, uint32_t old_val, uint32_t new_val);
};

} // namespace aetherplane
