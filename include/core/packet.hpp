#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <array>
#include <memory>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace openpath {

// L2 Ethernet Protocols
constexpr uint16_t ETH_P_IP   = 0x0800;
constexpr uint16_t ETH_P_ARP  = 0x0806;
constexpr uint16_t ETH_P_IPV6 = 0x86DD;
constexpr uint16_t ETH_P_VLAN = 0x8100;

// L3 Protocols
constexpr uint8_t IPPROTO_ICMP = 1;
constexpr uint8_t IPPROTO_TCP  = 6;
constexpr uint8_t IPPROTO_UDP  = 17;

#pragma pack(push, 1)

struct EthernetHeader {
    uint8_t  dest_mac[6];
    uint8_t  src_mac[6];
    uint16_t ether_type; // Big-endian
};

struct ARPHeader {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t  hw_size;
    uint8_t  proto_size;
    uint16_t opcode; // 1 = Request, 2 = Reply
    uint8_t  sender_mac[6];
    uint32_t sender_ip;
    uint8_t  target_mac[6];
    uint32_t target_ip;
};

struct IPv4Header {
    uint8_t  version_ihl;   // Version (4 bits) + IHL (4 bits)
    uint8_t  tos;           // Type of service (DSCP/ECN)
    uint16_t total_length;  // Big-endian
    uint16_t id;
    uint16_t flags_fragment_offset;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_ip;        // Big-endian
    uint32_t dest_ip;       // Big-endian
};

struct IPv6Header {
    uint32_t ver_tc_fl;     // Version (4b), Traffic Class (8b), Flow Label (20b)
    uint16_t payload_len;
    uint8_t  next_header;
    uint8_t  hop_limit;
    uint8_t  src_ip[16];
    uint8_t  dest_ip[16];
};

struct TCPHeader {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint16_t offset_reserved_flags; // Data offset (4b), Flags (9b)
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_pointer;
};

struct UDPHeader {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
};

#pragma pack(pop)

enum class TrafficClass : uint8_t {
    VOICE_CONTROL = 0, // Highest Priority (VoIP, ARP, Routing)
    GAMING_LOW_LAT = 1, // Interactive UDP / Gaming
    STREAMING_VIDEO = 2, // Video conferencing / Adaptive streaming
    BEST_EFFORT = 3,    // Standard HTTP/HTTPS
    BULK_BACKGROUND = 4 // Torrents, large file sync, backups
};

struct PacketMeta {
    uint64_t timestamp_ns{0};
    uint32_t ingress_ifindex{0};
    uint32_t egress_ifindex{0};
    uint16_t l2_type{0};
    uint8_t  l3_proto{0};
    uint32_t src_ip{0};
    uint32_t dest_ip{0};
    uint16_t src_port{0};
    uint16_t dest_port{0};
    uint16_t payload_size{0};
    TrafficClass traffic_class{TrafficClass::BEST_EFFORT};
    bool is_drop{false};
};

class Packet {
public:
    Packet() = default;
    explicit Packet(const uint8_t* raw_data, size_t len);
    explicit Packet(std::vector<uint8_t> buffer);

    bool parse_headers();
    
    // Accessors
    const uint8_t* data() const { return buffer_.data(); }
    uint8_t* data() { return buffer_.data(); }
    size_t size() const { return buffer_.size(); }
    const PacketMeta& meta() const { return meta_; }
    PacketMeta& meta() { return meta_; }

    std::string to_string() const;
    static std::string ip_to_string(uint32_t ip);
    static std::string mac_to_string(const uint8_t mac[6]);

private:
    std::vector<uint8_t> buffer_;
    PacketMeta meta_;
};

} // namespace openpath
