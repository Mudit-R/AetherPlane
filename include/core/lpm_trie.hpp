#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <optional>
#include <vector>

namespace openpath {

struct RouteEntry {
    uint32_t prefix;      // Network prefix (Host byte order)
    uint8_t  prefix_len;  // 0 - 32
    uint32_t next_hop_ip;
    uint32_t egress_ifindex;
    uint32_t metric;
};

class LPMTrie {
public:
    LPMTrie();
    ~LPMTrie() = default;

    bool insert_route(uint32_t prefix, uint8_t prefix_len, uint32_t next_hop, uint32_t egress_ifindex, uint32_t metric = 1);
    bool insert_route(const std::string& cidr, const std::string& next_hop, uint32_t egress_ifindex, uint32_t metric = 1);

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

        TrieNode() = default;
    };

    std::unique_ptr<TrieNode> root_;
    size_t route_count_{0};
};

} // namespace openpath
