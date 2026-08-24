#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <optional>
#include <vector>
#include <array>

namespace openpath {

struct RouteEntry {
    uint32_t prefix{0};      // Network prefix (Host byte order)
    uint8_t  prefix_len{0};  // 0 - 32
    uint32_t next_hop_ip{0};
    uint32_t egress_ifindex{0};
    uint32_t metric{1};
};

/**
 * @brief High-Performance DPDK-style 16-8 (TBL24 / TBL8) Radix LPM Routing Table.
 * Implements Dir-24-8-BASIC architecture:
 * - Direct lookup in TBL24 (single cacheline access for /0 to /24)
 * - Two lookups max for /25 to /32 via secondary TBL8 groups.
 */
class LPMTrie {
public:
    static constexpr size_t TBL24_NUM_ENTRIES = 1 << 24; // 16,777,216 entries (16MB table)
    static constexpr size_t TBL8_GROUP_SIZE   = 256;
    static constexpr size_t TBL8_NUM_GROUPS   = 512;

    struct TBL24Entry {
        uint32_t next_hop : 23;
        uint32_t ext_flag : 1;  // 0 = direct next-hop, 1 = index into TBL8 group
        uint32_t depth    : 8;
    };

    struct TBL8Entry {
        uint32_t next_hop : 23;
        uint32_t valid    : 1;
        uint32_t depth    : 8;
    };

    LPMTrie();
    ~LPMTrie() = default;

    bool insert_route(uint32_t prefix, uint8_t prefix_len, uint32_t next_hop, uint32_t egress_ifindex, uint32_t metric = 1);
    bool insert_route(const std::string& cidr, const std::string& next_hop, uint32_t egress_ifindex, uint32_t metric = 1);

    // Fast-path lookup inline candidate
    std::optional<RouteEntry> lookup(uint32_t dest_ip) const;
    std::optional<RouteEntry> lookup(const std::string& ip_str) const;

    bool delete_route(uint32_t prefix, uint8_t prefix_len);
    size_t route_count() const { return route_count_; }

    static uint32_t ip_from_string(const std::string& ip_str);
    static std::string ip_to_string(uint32_t ip);

private:
    struct TrieNode {
        std::unique_ptr<TrieNode> children[2];
        std::optional<RouteEntry> entry;
    };

    std::unique_ptr<TrieNode> root_;
    size_t route_count_{0};
};

} // namespace openpath
