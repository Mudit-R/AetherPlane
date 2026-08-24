#pragma once

#include "core/packet.hpp"
#include <unordered_map>
#include <vector>
#include <deque>
#include <mutex>

namespace openpath {

struct FlowStats {
    uint32_t flow_id;
    uint32_t src_ip;
    uint32_t dest_ip;
    uint16_t src_port;
    uint16_t dest_port;
    uint8_t  protocol;

    uint64_t start_time_ns{0};
    uint64_t last_seen_ns{0};
    uint64_t packet_count{0};
    uint64_t byte_count{0};

    // Statistical feature tracking for ML inference
    std::deque<uint64_t> inter_arrival_times_us;
    std::deque<uint16_t> packet_sizes;
    
    double mean_iat_us{0.0};
    double stddev_iat_us{0.0};
    double mean_packet_size{0.0};
    double stddev_packet_size{0.0};
    double burstiness_ratio{0.0};

    TrafficClass predicted_class{TrafficClass::BEST_EFFORT};
    double confidence{0.0};
};

class FlowClassifier {
public:
    FlowClassifier();

    FlowStats& track_and_update(const Packet& pkt);
    TrafficClass classify_flow(FlowStats& stats);

    std::vector<FlowStats> get_active_flows() const;
    size_t total_flows() const;

private:
    mutable std::mutex flows_mutex_;
    std::unordered_map<std::string, FlowStats> flow_table_;
    uint32_t next_flow_id_{1};

    void compute_statistical_features(FlowStats& stats);
    static std::string make_flow_key(const Packet& pkt);
};

} // namespace openpath
