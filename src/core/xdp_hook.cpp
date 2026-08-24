#include "core/xdp_hook.hpp"

namespace openpath {

XDPHookEngine::XDPHookEngine() = default;

void XDPHookEngine::attach_program(const std::string& name, XDPProgram prog) {
    attached_programs_.emplace_back(name, std::move(prog));
}

void XDPHookEngine::detach_all() {
    attached_programs_.clear();
}

XDPAction XDPHookEngine::process_packet(Packet& pkt, XDPMeta& meta) {
    total_processed_++;

    if (attached_programs_.empty()) {
        return XDPAction::XDP_PASS;
    }

    for (const auto& [name, prog] : attached_programs_) {
        XDPAction action = prog(pkt, meta);
        switch (action) {
            case XDPAction::XDP_DROP:
                total_dropped_++;
                pkt.meta().is_drop = true;
                return XDPAction::XDP_DROP;
            case XDPAction::XDP_REDIRECT:
                total_redirected_++;
                return XDPAction::XDP_REDIRECT;
            case XDPAction::XDP_TX:
                return XDPAction::XDP_TX;
            case XDPAction::XDP_ABORTED:
                total_dropped_++;
                return XDPAction::XDP_ABORTED;
            case XDPAction::XDP_PASS:
            default:
                break;
        }
    }

    return XDPAction::XDP_PASS;
}

} // namespace openpath
