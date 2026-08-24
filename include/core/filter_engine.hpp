#pragma once

#include "packet.hpp"
#include <vector>
#include <unordered_map>
#include <mutex>
#include <string>

namespace openpath {

enum class FilterVerdict {
    ACCEPT,
    DROP,
    REJECT,
    QOS_QUEUE
};

struct ACLRule {
    uint32_t rule_id;
    std::string description;
    uint32_t src_ip{0};
    uint32_t src_mask{0};
    uint32_t dest_ip{0};
    uint32_t dest_mask{0};
    uint16_t src_port_min{0};
    uint16_t src_port_max{65535};
    uint16_t dest_port_min{0};
    uint16_t dest_port_max{65535};
    uint8_t  protocol{0}; // 0 = Any, 6 = TCP, 17 = UDP, 1 = ICMP
    FilterVerdict verdict{FilterVerdict::ACCEPT};
    TrafficClass assigned_class{TrafficClass::BEST_EFFORT};
};

struct ConnectionState {
    uint64_t last_seen_ns{0};
    uint64_t packets_forwarded{0};
    uint64_t bytes_forwarded{0};
    bool is_established{false};
};

class FilterEngine {
public:
    FilterEngine();

    void add_rule(const ACLRule& rule);
    void clear_rules();

    FilterVerdict evaluate(const Packet& packet, TrafficClass& out_class);
    void update_conntrack(const Packet& packet);
    size_t active_connections() const;

private:
    std::vector<ACLRule> rules_;
    mutable std::mutex conntrack_mutex_;
    std::unordered_map<std::string, ConnectionState> conntrack_table_;

    static std::string make_flow_key(const Packet& pkt);
};

} // namespace openpath
