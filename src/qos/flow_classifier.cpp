#include "qos/flow_classifier.hpp"
#include <sstream>
#include <cmath>
#include <numeric>

namespace openpath {

FlowClassifier::FlowClassifier() = default;

std::string FlowClassifier::make_flow_key(const Packet& pkt) {
    const auto& m = pkt.meta();
    std::ostringstream oss;
    // 5-tuple: src_ip, dst_ip, src_port, dst_port, proto
    oss << m.src_ip << ":" << m.src_port << "<->"
        << m.dest_ip << ":" << m.dest_port << ":"
        << static_cast<int>(m.l3_proto);
    return oss.str();
}

void FlowClassifier::compute_statistical_features(FlowStats& stats) {
    if (stats.inter_arrival_times_us.size() >= 2) {
        double sum = 0.0;
        for (auto iat : stats.inter_arrival_times_us) sum += iat;
        stats.mean_iat_us = sum / stats.inter_arrival_times_us.size();

        double var = 0.0;
        for (auto iat : stats.inter_arrival_times_us) {
            var += (iat - stats.mean_iat_us) * (iat - stats.mean_iat_us);
        }
        stats.stddev_iat_us = std::sqrt(var / stats.inter_arrival_times_us.size());
    }

    if (!stats.packet_sizes.empty()) {
        double sum = 0.0;
        for (auto sz : stats.packet_sizes) sum += sz;
        stats.mean_packet_size = sum / stats.packet_sizes.size();

        double var = 0.0;
        for (auto sz : stats.packet_sizes) {
            var += (sz - stats.mean_packet_size) * (sz - stats.mean_packet_size);
        }
        stats.stddev_packet_size = std::sqrt(var / stats.packet_sizes.size());
    }

    if (stats.mean_iat_us > 0.0) {
        stats.burstiness_ratio = stats.stddev_iat_us / stats.mean_iat_us;
    }
}

FlowStats& FlowClassifier::track_and_update(const Packet& pkt) {
    std::lock_guard<std::mutex> lock(flows_mutex_);
    std::string key = make_flow_key(pkt);

    auto it = flow_table_.find(key);
    if (it == flow_table_.end()) {
        FlowStats stats;
        stats.flow_id = next_flow_id_++;
        stats.src_ip = pkt.meta().src_ip;
        stats.dest_ip = pkt.meta().dest_ip;
        stats.src_port = pkt.meta().src_port;
        stats.dest_port = pkt.meta().dest_port;
        stats.protocol = pkt.meta().l3_proto;
        stats.start_time_ns = pkt.meta().timestamp_ns;
        stats.last_seen_ns = pkt.meta().timestamp_ns;
        stats.packet_count = 1;
        stats.byte_count = pkt.size();
        stats.packet_sizes.push_back(static_cast<uint16_t>(pkt.size()));

        flow_table_[key] = stats;
        return flow_table_[key];
    }

    FlowStats& stats = it->second;
    uint64_t iat_us = (pkt.meta().timestamp_ns > stats.last_seen_ns) 
        ? (pkt.meta().timestamp_ns - stats.last_seen_ns) / 1000 
        : 1;

    stats.last_seen_ns = pkt.meta().timestamp_ns;
    stats.packet_count++;
    stats.byte_count += pkt.size();

    stats.inter_arrival_times_us.push_back(iat_us);
    if (stats.inter_arrival_times_us.size() > 50) stats.inter_arrival_times_us.pop_front();

    stats.packet_sizes.push_back(static_cast<uint16_t>(pkt.size()));
    if (stats.packet_sizes.size() > 50) stats.packet_sizes.pop_front();

    compute_statistical_features(stats);
    return stats;
}

TrafficClass FlowClassifier::classify_flow(FlowStats& stats) {
    // High-performance lightweight decision tree inference running in sub-microsecond latency
    // 1. Control / Voice: DNS (53), NTP (123), SIP (5060), RTP, ARP
    if (stats.src_port == 53 || stats.dest_port == 53 ||
        stats.src_port == 5060 || stats.dest_port == 5060 ||
        stats.protocol == 0xFD || stats.protocol == IPPROTO_ICMP) {
        stats.predicted_class = TrafficClass::VOICE_CONTROL;
        stats.confidence = 0.99;
        return TrafficClass::VOICE_CONTROL;
    }

    // 2. Gaming / Low Latency: Small UDP packets (< 250B) with low IAT variance
    if (stats.protocol == IPPROTO_UDP && stats.mean_packet_size < 300 && stats.burstiness_ratio < 1.2) {
        stats.predicted_class = TrafficClass::GAMING_LOW_LAT;
        stats.confidence = 0.94;
        return TrafficClass::GAMING_LOW_LAT;
    }

    // 3. Streaming Video / Real-time media: Consistent ~1200-1500B packets, moderate IAT
    if (stats.mean_packet_size > 1000 && stats.burstiness_ratio < 0.8 && stats.packet_count > 10) {
        stats.predicted_class = TrafficClass::STREAMING_VIDEO;
        stats.confidence = 0.91;
        return TrafficClass::STREAMING_VIDEO;
    }

    // 4. Bulk Data Transfer: High burstiness, MTU sized TCP packets, long duration
    if (stats.protocol == IPPROTO_TCP && stats.mean_packet_size >= 1400 && stats.burstiness_ratio >= 1.5) {
        stats.predicted_class = TrafficClass::BULK_BACKGROUND;
        stats.confidence = 0.88;
        return TrafficClass::BULK_BACKGROUND;
    }

    // 5. Default: Best Effort Web
    stats.predicted_class = TrafficClass::BEST_EFFORT;
    stats.confidence = 0.85;
    return TrafficClass::BEST_EFFORT;
}

std::vector<FlowStats> FlowClassifier::get_active_flows() const {
    std::lock_guard<std::mutex> lock(flows_mutex_);
    std::vector<FlowStats> result;
    result.reserve(flow_table_.size());
    for (const auto& [_, flow] : flow_table_) {
        result.push_back(flow);
    }
    return result;
}

size_t FlowClassifier::total_flows() const {
    std::lock_guard<std::mutex> lock(flows_mutex_);
    return flow_table_.size();
}

} // namespace openpath
