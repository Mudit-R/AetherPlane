#include "core/filter_engine.hpp"
#include <sstream>

namespace openpath {

FilterEngine::FilterEngine() = default;

void FilterEngine::add_rule(const ACLRule& rule) {
    rules_.push_back(rule);
}

void FilterEngine::clear_rules() {
    rules_.clear();
}

std::string FilterEngine::make_flow_key(const Packet& pkt) {
    const auto& m = pkt.meta();
    std::ostringstream oss;
    oss << m.src_ip << ":" << m.src_port << "->"
        << m.dest_ip << ":" << m.dest_port << "/"
        << static_cast<int>(m.l3_proto);
    return oss.str();
}

FilterVerdict FilterEngine::evaluate(const Packet& packet, TrafficClass& out_class) {
    const auto& meta = packet.meta();

    for (const auto& rule : rules_) {
        // Match Protocol
        if (rule.protocol != 0 && rule.protocol != meta.l3_proto) {
            continue;
        }

        // Match Source IP
        if (rule.src_mask != 0 && (meta.src_ip & rule.src_mask) != (rule.src_ip & rule.src_mask)) {
            continue;
        }

        // Match Destination IP
        if (rule.dest_mask != 0 && (meta.dest_ip & rule.dest_mask) != (rule.dest_ip & rule.dest_mask)) {
            continue;
        }

        // Match Port Ranges
        if (meta.src_port < rule.src_port_min || meta.src_port > rule.src_port_max) {
            continue;
        }
        if (meta.dest_port < rule.dest_port_min || meta.dest_port > rule.dest_port_max) {
            continue;
        }

        // Rule Matched!
        if (rule.verdict == FilterVerdict::QOS_QUEUE) {
            out_class = rule.assigned_class;
        }
        return rule.verdict;
    }

    return FilterVerdict::ACCEPT; // Default policy
}

void FilterEngine::update_conntrack(const Packet& packet) {
    std::lock_guard<std::mutex> lock(conntrack_mutex_);
    std::string key = make_flow_key(packet);

    auto& entry = conntrack_table_[key];
    entry.last_seen_ns = packet.meta().timestamp_ns;
    entry.packets_forwarded++;
    entry.bytes_forwarded += packet.size();
    if (entry.packets_forwarded > 3) {
        entry.is_established = true;
    }
}

size_t FilterEngine::active_connections() const {
    std::lock_guard<std::mutex> lock(conntrack_mutex_);
    return conntrack_table_.size();
}

} // namespace openpath
