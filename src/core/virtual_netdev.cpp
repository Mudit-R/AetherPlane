#include "core/virtual_netdev.hpp"

namespace openpath {

VirtualNetDev::VirtualNetDev(uint32_t ifindex, std::string name, std::array<uint8_t, 6> mac)
    : ifindex_(ifindex), name_(std::move(name)), mac_(mac) {}

bool VirtualNetDev::enqueue_rx(const Packet& pkt) {
    if (rx_ring_.push(pkt)) {
        rx_packets_++;
        return true;
    }
    rx_drops_++;
    return false;
}

bool VirtualNetDev::dequeue_rx(Packet& pkt) {
    return rx_ring_.pop(pkt);
}

bool VirtualNetDev::enqueue_tx(const Packet& pkt) {
    if (tx_ring_.push(pkt)) {
        tx_packets_++;
        return true;
    }
    tx_drops_++;
    return false;
}

bool VirtualNetDev::dequeue_tx(Packet& pkt) {
    return tx_ring_.pop(pkt);
}

} // namespace openpath
