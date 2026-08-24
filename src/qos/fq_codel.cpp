#include "qos/fq_codel.hpp"
#include <cmath>
#include <algorithm>

namespace aetherplane {

FQCoDelScheduler::FQCoDelScheduler() {
    for (uint32_t i = 0; i < FQ_CODEL_NUM_FLOWS; ++i) {
        flows_[i].flow_id = i;
        flows_[i].deficit = FQ_CODEL_QUANTUM;
    }
}

void FQCoDelScheduler::set_link_rate_mbps(double mbps) {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);
    link_rate_mbps_ = std::max(1.0, mbps);
}

uint32_t FQCoDelScheduler::hash_flow(const openpath::Packet& pkt) const {
    const auto& m = pkt.meta();
    // 5-tuple Jenkins one-at-a-time hash
    uint32_t h = 0;
    auto add_byte = [&h](uint8_t byte) {
        h += byte;
        h += (h << 10);
        h ^= (h >> 6);
    };

    uint32_t src = m.src_ip;
    uint32_t dst = m.dest_ip;
    uint16_t sp = m.src_port;
    uint16_t dp = m.dest_port;
    uint8_t proto = m.l3_proto;

    for (int i = 0; i < 4; ++i) add_byte((src >> (i * 8)) & 0xFF);
    for (int i = 0; i < 4; ++i) add_byte((dst >> (i * 8)) & 0xFF);
    add_byte(sp & 0xFF); add_byte((sp >> 8) & 0xFF);
    add_byte(dp & 0xFF); add_byte((dp >> 8) & 0xFF);
    add_byte(proto);

    h += (h << 3);
    h ^= (h >> 11);
    h += (h << 15);

    return h % FQ_CODEL_NUM_FLOWS;
}

uint64_t FQCoDelScheduler::control_law(uint64_t t, uint32_t count) {
    if (count == 0) return t + CODEL_INTERVAL_NS;
    // Inverse square root drop schedule: t + interval / sqrt(count)
    uint64_t delta = static_cast<uint64_t>(CODEL_INTERVAL_NS / std::sqrt(count));
    return t + delta;
}

bool FQCoDelScheduler::codel_should_drop(FlowQueue& flow, const openpath::Packet& pkt, uint64_t now_ns) {
    uint64_t sojourn_time_ns = (now_ns > pkt.meta().timestamp_ns) ? (now_ns - pkt.meta().timestamp_ns) : 0;
    current_delay_ms_ = sojourn_time_ns / 1000000.0;

    if (sojourn_time_ns < CODEL_TARGET_NS || flow.total_bytes < FQ_CODEL_QUANTUM) {
        flow.codel.first_above_time_ns = 0;
        return false;
    }

    if (flow.codel.first_above_time_ns == 0) {
        flow.codel.first_above_time_ns = now_ns + CODEL_INTERVAL_NS;
    } else if (now_ns >= flow.codel.first_above_time_ns) {
        return true; // Over target for full interval window
    }

    return false;
}

bool FQCoDelScheduler::enqueue(const openpath::Packet& pkt) {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);

    uint32_t flow_idx = hash_flow(pkt);
    auto& flow = flows_[flow_idx];

    // Check if new flow list insertion is needed
    if (flow.packets.empty()) {
        flow.deficit = FQ_CODEL_QUANTUM;
        new_flows_.push_back(flow_idx);
    }

    flow.packets.push_back(pkt);
    flow.total_bytes += pkt.size();

    // Check if flow is sparse vs bulk
    flow.is_sparse = (flow.packets.size() <= 4);

    return true;
}

bool FQCoDelScheduler::dequeue(openpath::Packet& pkt) {
    std::lock_guard<std::mutex> lock(scheduler_mutex_);

    auto now_epoch = std::chrono::steady_clock::now().time_since_epoch();
    uint64_t now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now_epoch).count();

    auto process_flow_list = [&](std::deque<uint32_t>& flow_list) -> bool {
        while (!flow_list.empty()) {
            uint32_t idx = flow_list.front();
            auto& flow = flows_[idx];

            if (flow.packets.empty()) {
                flow_list.pop_front();
                continue;
            }

            if (flow.deficit <= 0) {
                flow.deficit += FQ_CODEL_QUANTUM;
                flow_list.pop_front();
                old_flows_.push_back(idx);
                continue;
            }

            // Dequeue candidate packet
            openpath::Packet candidate = flow.packets.front();
            flow.packets.pop_front();
            flow.total_bytes -= candidate.size();
            flow.deficit -= static_cast<int32_t>(candidate.size());

            // Check CoDel AQM drop condition
            if (codel_should_drop(flow, candidate, now_ns)) {
                total_drops_++;
                flow.drops++;
                bufferbloat_active_ = true;

                if (!flow.codel.dropping) {
                    flow.codel.dropping = true;
                    flow.codel.count = 1;
                    flow.codel.drop_next_ns = control_law(now_ns, flow.codel.count);
                } else if (now_ns >= flow.codel.drop_next_ns) {
                    flow.codel.count++;
                    flow.codel.drop_next_ns = control_law(flow.codel.drop_next_ns, flow.codel.count);
                }
                continue; // Dropped, loop to next packet
            }

            if (flow.codel.dropping && current_delay_ms_ < 5.0) {
                flow.codel.dropping = false;
                bufferbloat_active_ = false;
            }

            pkt = candidate;
            return true;
        }
        return false;
    };

    // 1. Process new (sparse/real-time) flows first
    if (process_flow_list(new_flows_)) {
        return true;
    }

    // 2. Process old (bulk) flows with DRR deficit accounting
    if (process_flow_list(old_flows_)) {
        return true;
    }

    return false;
}

} // namespace aetherplane
