#pragma once

#include "packet.hpp"
#include <functional>
#include <vector>
#include <string>

namespace openpath {

enum class XDPAction {
    XDP_ABORTED = 0,
    XDP_DROP    = 1,
    XDP_PASS    = 2,
    XDP_TX      = 3,
    XDP_REDIRECT= 4
};

struct XDPMeta {
    uint32_t rx_ring_index{0};
    uint32_t redirect_ifindex{0};
    uint64_t cpu_cycles_spent{0};
};

using XDPProgram = std::function<XDPAction(Packet&, XDPMeta&)>;

class XDPHookEngine {
public:
    XDPHookEngine();

    void attach_program(const std::string& name, XDPProgram prog);
    void detach_all();

    XDPAction process_packet(Packet& pkt, XDPMeta& meta);

    uint64_t get_total_processed() const { return total_processed_; }
    uint64_t get_total_dropped() const { return total_dropped_; }
    uint64_t get_total_redirected() const { return total_redirected_; }

private:
    std::vector<std::pair<std::string, XDPProgram>> attached_programs_;
    uint64_t total_processed_{0};
    uint64_t total_dropped_{0};
    uint64_t total_redirected_{0};
};

} // namespace openpath
