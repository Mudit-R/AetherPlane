#include "core/packet.hpp"
#include <chrono>
#include <iostream>

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace openpath {

Packet::Packet(const uint8_t* raw_data, size_t len) 
    : buffer_(raw_data, raw_data + len) {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    meta_.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
    parse_headers();
}

Packet::Packet(std::vector<uint8_t> buffer) 
    : buffer_(std::move(buffer)) {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    meta_.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
    parse_headers();
}

bool Packet::parse_headers() {
    if (buffer_.size() < sizeof(EthernetHeader)) {
        return false;
    }

    const auto* eth = reinterpret_cast<const EthernetHeader*>(buffer_.data());
    uint16_t eth_type = ntohs(eth->ether_type);
    meta_.l2_type = eth_type;

    size_t offset = sizeof(EthernetHeader);

    // Handle 802.1Q VLAN Tagging
    if (eth_type == ETH_P_VLAN) {
        if (buffer_.size() < offset + 4) return false;
        eth_type = ntohs(*reinterpret_cast<const uint16_t*>(buffer_.data() + offset + 2));
        offset += 4;
    }

    // Handle ARP
    if (eth_type == ETH_P_ARP) {
        if (buffer_.size() < offset + sizeof(ARPHeader)) return false;
        const auto* arp = reinterpret_cast<const ARPHeader*>(buffer_.data() + offset);
        meta_.l3_proto = 0xFD; // Custom ARP ID
        meta_.src_ip = ntohl(arp->sender_ip);
        meta_.dest_ip = ntohl(arp->target_ip);
        meta_.traffic_class = TrafficClass::VOICE_CONTROL;
        return true;
    }

    // Handle IPv4
    if (eth_type == ETH_P_IP) {
        if (buffer_.size() < offset + sizeof(IPv4Header)) return false;
        const auto* ip = reinterpret_cast<const IPv4Header*>(buffer_.data() + offset);
        uint8_t ihl = (ip->version_ihl & 0x0F) * 4;
        if (ihl < sizeof(IPv4Header) || buffer_.size() < offset + ihl) return false;

        meta_.l3_proto = ip->protocol;
        meta_.src_ip = ntohl(ip->src_ip);
        meta_.dest_ip = ntohl(ip->dest_ip);

        offset += ihl;

        if (meta_.l3_proto == IPPROTO_TCP) {
            if (buffer_.size() >= offset + sizeof(TCPHeader)) {
                const auto* tcp = reinterpret_cast<const TCPHeader*>(buffer_.data() + offset);
                meta_.src_port = ntohs(tcp->src_port);
                meta_.dest_port = ntohs(tcp->dest_port);
                uint8_t data_offset = ((ntohs(tcp->offset_reserved_flags) >> 12) & 0x0F) * 4;
                offset += data_offset;
            }
        } else if (meta_.l3_proto == IPPROTO_UDP) {
            if (buffer_.size() >= offset + sizeof(UDPHeader)) {
                const auto* udp = reinterpret_cast<const UDPHeader*>(buffer_.data() + offset);
                meta_.src_port = ntohs(udp->src_port);
                meta_.dest_port = ntohs(udp->dest_port);
                offset += sizeof(UDPHeader);
            }
        }

        meta_.payload_size = (buffer_.size() > offset) ? static_cast<uint16_t>(buffer_.size() - offset) : 0;
        return true;
    }

    return false;
}

std::string Packet::ip_to_string(uint32_t ip) {
    std::ostringstream oss;
    oss << ((ip >> 24) & 0xFF) << "."
        << ((ip >> 16) & 0xFF) << "."
        << ((ip >> 8) & 0xFF) << "."
        << (ip & 0xFF);
    return oss.str();
}

std::string Packet::mac_to_string(const uint8_t mac[6]) {
    std::ostringstream oss;
    for (int i = 0; i < 6; ++i) {
        if (i > 0) oss << ":";
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(mac[i]);
    }
    return oss.str();
}

std::string Packet::to_string() const {
    std::ostringstream oss;
    oss << "[Packet Len=" << buffer_.size() << " B | "
        << ip_to_string(meta_.src_ip) << ":" << meta_.src_port << " -> "
        << ip_to_string(meta_.dest_ip) << ":" << meta_.dest_port
        << " | Proto=" << static_cast<int>(meta_.l3_proto)
        << " | Class=" << static_cast<int>(meta_.traffic_class) << "]";
    return oss.str();
}

} // namespace openpath
