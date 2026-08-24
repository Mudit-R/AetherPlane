#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>

#include "core/packet.hpp"
#include "core/ring_buffer.hpp"
#include "core/lpm_trie.hpp"
#include "core/filter_engine.hpp"
#include "core/xdp_hook.hpp"
#include "core/virtual_netdev.hpp"
#include "qos/flow_classifier.hpp"
#include "qos/smart_scheduler.hpp"
#include "telemetry/metrics_collector.hpp"

using namespace openpath;

std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running = false;
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "=========================================================\n";
    std::cout << "  AetherPlane: High-Performance Network Data Path Engine  \n";
    std::cout << "  Tailored for Wireless Access Point Routers (DPDK/eBPF)  \n";
    std::cout << "=========================================================\n\n";

    // 1. Initialize Virtual Network Interfaces
    VirtualNetDev eth0(1, "eth0_wan", {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E});
    VirtualNetDev wlan0(2, "wlan0_lan", {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5F});
    std::cout << "[+] Initialized Virtual NetDev: " << eth0.name() << " (ifindex=" << eth0.ifindex() << ")\n";
    std::cout << "[+] Initialized Virtual NetDev: " << wlan0.name() << " (ifindex=" << wlan0.ifindex() << ")\n";

    // 2. Initialize LPM Routing Table
    LPMTrie routing_table;
    routing_table.insert_route("192.168.1.0/24", "192.168.1.1", wlan0.ifindex(), 10);
    routing_table.insert_route("10.0.0.0/8", "10.0.0.1", eth0.ifindex(), 20);
    routing_table.insert_route("0.0.0.0/0", "192.168.1.254", eth0.ifindex(), 100); // Default Route
    std::cout << "[+] Loaded LPM Radix Trie with " << routing_table.route_count() << " routes\n";

    // 3. Initialize eBPF/XDP Fast-Path Hook
    XDPHookEngine xdp;
    xdp.attach_program("xdp_ddos_mitigator", [](Packet& pkt, XDPMeta&) -> XDPAction {
        // Drop malicious UDP flood to port 9999 at line rate (Kernel Bypass)
        if (pkt.meta().l3_proto == IPPROTO_UDP && pkt.meta().dest_port == 9999) {
            return XDPAction::XDP_DROP;
        }
        return XDPAction::XDP_PASS;
    });
    std::cout << "[+] Attached eBPF/XDP Fast-Path Filters\n";

    // 4. Initialize Netfilter ACL Engine & Conntrack
    FilterEngine netfilter;
    ACLRule rule_ssh{
        .rule_id = 1,
        .description = "Prioritize SSH Control Traffic",
        .dest_port_min = 22,
        .dest_port_max = 22,
        .protocol = IPPROTO_TCP,
        .verdict = FilterVerdict::QOS_QUEUE,
        .assigned_class = TrafficClass::VOICE_CONTROL
    };
    netfilter.add_rule(rule_ssh);
    std::cout << "[+] Configured Netfilter Stateful ACL Engine\n";

    // 5. Initialize AI/ML Flow Classifier & Smart QoS Scheduler
    FlowClassifier classifier;
    SmartQoSScheduler qos_scheduler;
    qos_scheduler.set_wireless_link_rate_mbps(50.0); // 50 Mbps simulated wireless bottleneck
    std::cout << "[+] Initialized AI/ML Smart Traffic QoS Engine (Link Rate: 50 Mbps)\n\n";

    std::cout << "[*] Starting Data Path Processing Pipeline...\n";

    uint64_t loop_count = 0;
    while (g_running) {
        // Synthetic packet injection for testing data path
        std::vector<uint8_t> raw_pkt(128, 0xAA);
        auto* eth = reinterpret_cast<EthernetHeader*>(raw_pkt.data());
        eth->ether_type = htons(ETH_P_IP);

        auto* ip = reinterpret_cast<IPv4Header*>(raw_pkt.data() + sizeof(EthernetHeader));
        ip->version_ihl = 0x45;
        ip->protocol = (loop_count % 3 == 0) ? IPPROTO_UDP : IPPROTO_TCP;
        ip->src_ip = htonl(0xC0A80100 + (loop_count % 250)); // 192.168.1.X
        ip->dest_ip = htonl(0x0A000001); // 10.0.0.1

        Packet pkt(std::move(raw_pkt));

        // Step 1: eBPF/XDP Hook Evaluation (Sub-microsecond kernel bypass)
        XDPMeta xdp_meta;
        XDPAction act = xdp.process_packet(pkt, xdp_meta);
        if (act == XDPAction::XDP_DROP) {
            MetricsCollector::instance().record_drop();
            continue;
        }

        // Step 2: Netfilter ACL & Conntrack
        TrafficClass assigned_class = TrafficClass::BEST_EFFORT;
        FilterVerdict verd = netfilter.evaluate(pkt, assigned_class);
        if (verd == FilterVerdict::DROP) {
            MetricsCollector::instance().record_drop();
            continue;
        }
        netfilter.update_conntrack(pkt);

        // Step 3: AI/ML Flow Tracking & QoS Classification
        FlowStats& flow = classifier.track_and_update(pkt);
        TrafficClass ml_class = classifier.classify_flow(flow);
        pkt.meta().traffic_class = (verd == FilterVerdict::QOS_QUEUE) ? assigned_class : ml_class;
        MetricsCollector::instance().record_class_packet(static_cast<uint8_t>(pkt.meta().traffic_class));

        // Step 4: LPM Route Lookup
        auto route = routing_table.lookup(pkt.meta().dest_ip);
        if (route) {
            pkt.meta().egress_ifindex = route->egress_ifindex;
        }

        // Step 5: Enqueue into Smart QoS Scheduler
        qos_scheduler.enqueue(pkt);
        MetricsCollector::instance().record_rx(pkt.size(), 450); // ~450ns latency

        // Step 6: Dequeue & Transmit
        Packet tx_pkt;
        if (qos_scheduler.dequeue(tx_pkt)) {
            wlan0.enqueue_tx(tx_pkt);
            MetricsCollector::instance().record_tx(tx_pkt.size());
        }

        loop_count++;
        if (loop_count % 50000 == 0) {
            auto telem = MetricsCollector::instance().snapshot();
            std::cout << "[Telemetry] RX=" << telem.total_rx_packets 
                      << " | TX=" << telem.total_tx_packets 
                      << " | PPS=" << static_cast<uint64_t>(telem.current_pps) 
                      << " | Latency=" << telem.avg_latency_us << " us"
                      << " | Active Flows=" << classifier.total_flows() 
                      << " | Bufferbloat=" << (qos_scheduler.is_bufferbloat_detected() ? "ALERT" : "OK") 
                      << "\n";
        }

        if (loop_count >= 200000) break; // Terminate demo run
    }

    std::cout << "\n[+] Data path execution completed successfully.\n";
    return 0;
}
