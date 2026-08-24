#pragma once

#include "packet.hpp"
#include "ring_buffer.hpp"
#include <string>
#include <memory>
#include <vector>

namespace openpath {

constexpr size_t RX_RING_SIZE = 1024;
constexpr size_t TX_RING_SIZE = 1024;

class VirtualNetDev {
public:
    VirtualNetDev(uint32_t ifindex, std::string name, std::array<uint8_t, 6> mac);

    uint32_t ifindex() const { return ifindex_; }
    const std::string& name() const { return name_; }
    const std::array<uint8_t, 6>& mac() const { return mac_; }

    bool enqueue_rx(const Packet& pkt);
    bool dequeue_rx(Packet& pkt);

    bool enqueue_tx(const Packet& pkt);
    bool dequeue_tx(Packet& pkt);

    size_t rx_depth() const { return rx_ring_.size(); }
    size_t tx_depth() const { return tx_ring_.size(); }

    uint64_t rx_packets() const { return rx_packets_; }
    uint64_t tx_packets() const { return tx_packets_; }
    uint64_t rx_drops() const { return rx_drops_; }
    uint64_t tx_drops() const { return tx_drops_; }

private:
    uint32_t ifindex_;
    std::string name_;
    std::array<uint8_t, 6> mac_;

    SPSCRingBuffer<Packet, RX_RING_SIZE> rx_ring_;
    SPSCRingBuffer<Packet, TX_RING_SIZE> tx_ring_;

    uint64_t rx_packets_{0};
    uint64_t tx_packets_{0};
    uint64_t rx_drops_{0};
    uint64_t tx_drops_{0};
};

} // namespace openpath
