#include "core/checksum.hpp"

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace aetherplane {

uint16_t ChecksumEngine::compute_checksum(const void* data, size_t len) {
    const auto* ptr = static_cast<const uint16_t*>(data);
    uint64_t sum = 0;

    // 64-bit unrolled accumulation (processes 8 words / 16 bytes per iteration)
    while (len >= 16) {
        sum += ptr[0];
        sum += ptr[1];
        sum += ptr[2];
        sum += ptr[3];
        sum += ptr[4];
        sum += ptr[5];
        sum += ptr[6];
        sum += ptr[7];
        ptr += 8;
        len -= 16;
    }

    // Process remaining 16-bit words
    while (len >= 2) {
        sum += *ptr++;
        len -= 2;
    }

    // Trailing odd byte
    if (len == 1) {
        sum += *reinterpret_cast<const uint8_t*>(ptr);
    }

    // Fold 64-bit sum down to 16 bits
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return static_cast<uint16_t>(~sum);
}

uint16_t ChecksumEngine::compute_ipv4_checksum(const void* ip_header, size_t len) {
    return compute_checksum(ip_header, len);
}

uint16_t ChecksumEngine::compute_tcp_checksum(uint32_t src_ip, uint32_t dest_ip, 
                                             const void* tcp_data, size_t len) {
    struct PseudoHeader {
        uint32_t src;
        uint32_t dst;
        uint8_t  zero;
        uint8_t  proto;
        uint16_t length;
    } ph;

    ph.src = htonl(src_ip);
    ph.dst = htonl(dest_ip);
    ph.zero = 0;
    ph.proto = 6; // IPPROTO_TCP
    ph.length = htons(static_cast<uint16_t>(len));

    uint64_t sum = 0;
    const auto* ph_ptr = reinterpret_cast<const uint16_t*>(&ph);
    for (size_t i = 0; i < sizeof(ph) / 2; ++i) {
        sum += ph_ptr[i];
    }

    const auto* tcp_ptr = static_cast<const uint16_t*>(tcp_data);
    size_t rem = len;
    while (rem >= 2) {
        sum += *tcp_ptr++;
        rem -= 2;
    }
    if (rem == 1) {
        sum += *reinterpret_cast<const uint8_t*>(tcp_ptr);
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return static_cast<uint16_t>(~sum);
}

uint16_t ChecksumEngine::compute_udp_checksum(uint32_t src_ip, uint32_t dest_ip, 
                                             const void* udp_data, size_t len) {
    struct PseudoHeader {
        uint32_t src;
        uint32_t dst;
        uint8_t  zero;
        uint8_t  proto;
        uint16_t length;
    } ph;

    ph.src = htonl(src_ip);
    ph.dst = htonl(dest_ip);
    ph.zero = 0;
    ph.proto = 17; // IPPROTO_UDP
    ph.length = htons(static_cast<uint16_t>(len));

    uint64_t sum = 0;
    const auto* ph_ptr = reinterpret_cast<const uint16_t*>(&ph);
    for (size_t i = 0; i < sizeof(ph) / 2; ++i) {
        sum += ph_ptr[i];
    }

    const auto* udp_ptr = static_cast<const uint16_t*>(udp_data);
    size_t rem = len;
    while (rem >= 2) {
        sum += *udp_ptr++;
        rem -= 2;
    }
    if (rem == 1) {
        sum += *reinterpret_cast<const uint8_t*>(udp_ptr);
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return static_cast<uint16_t>(~sum);
}

uint16_t ChecksumEngine::update_checksum_16(uint16_t old_csum, uint16_t old_val, uint16_t new_val) {
    uint32_t sum = (~old_csum & 0xFFFF) + (~old_val & 0xFFFF) + new_val;
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return static_cast<uint16_t>(~sum);
}

uint16_t ChecksumEngine::update_checksum_32(uint16_t old_csum, uint32_t old_val, uint32_t new_val) {
    uint16_t old_high = static_cast<uint16_t>(old_val >> 16);
    uint16_t old_low  = static_cast<uint16_t>(old_val & 0xFFFF);
    uint16_t new_high = static_cast<uint16_t>(new_val >> 16);
    uint16_t new_low  = static_cast<uint16_t>(new_val & 0xFFFF);

    uint16_t intermediate = update_checksum_16(old_csum, old_high, new_high);
    return update_checksum_16(intermediate, old_low, new_low);
}

} // namespace aetherplane
