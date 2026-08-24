#include "core/lpm_trie.hpp"
#include <sstream>
#include <stdexcept>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

namespace openpath {

LPMTrie::LPMTrie() : root_(std::make_unique<TrieNode>()) {}

uint32_t LPMTrie::ip_from_string(const std::string& ip_str) {
    struct in_addr addr;
    if (inet_pton(AF_INET, ip_str.c_str(), &addr) <= 0) {
        return 0;
    }
    return ntohl(addr.s_addr);
}

std::string LPMTrie::ip_to_string(uint32_t ip) {
    struct in_addr addr;
    addr.s_addr = htonl(ip);
    char buf[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &addr, buf, sizeof(buf))) {
        return std::string(buf);
    }
    return "";
}

bool LPMTrie::insert_route(uint32_t prefix, uint8_t prefix_len, uint32_t next_hop, uint32_t egress_ifindex, uint32_t metric) {
    if (prefix_len > 32) return false;

    TrieNode* current = root_.get();
    for (uint8_t i = 0; i < prefix_len; ++i) {
        uint32_t bit = (prefix >> (31 - i)) & 1;
        if (!current->children[bit]) {
            current->children[bit] = std::make_unique<TrieNode>();
        }
        current = current->children[bit].get();
    }

    if (!current->entry.has_value()) {
        route_count_++;
    }

    current->entry = RouteEntry{
        .prefix = prefix,
        .prefix_len = prefix_len,
        .next_hop_ip = next_hop,
        .egress_ifindex = egress_ifindex,
        .metric = metric
    };

    return true;
}

bool LPMTrie::insert_route(const std::string& cidr, const std::string& next_hop, uint32_t egress_ifindex, uint32_t metric) {
    size_t slash_pos = cidr.find('/');
    std::string ip_part = (slash_pos == std::string::npos) ? cidr : cidr.substr(0, slash_pos);
    uint8_t prefix_len = (slash_pos == std::string::npos) ? 32 : static_cast<uint8_t>(std::stoi(cidr.substr(slash_pos + 1)));

    uint32_t prefix = ip_from_string(ip_part);
    uint32_t next_hop_ip = ip_from_string(next_hop);

    return insert_route(prefix, prefix_len, next_hop_ip, egress_ifindex, metric);
}

std::optional<RouteEntry> LPMTrie::lookup(uint32_t dest_ip) const {
    const TrieNode* current = root_.get();
    std::optional<RouteEntry> best_match = std::nullopt;

    if (current->entry.has_value()) {
        best_match = current->entry;
    }

    for (uint8_t i = 0; i < 32; ++i) {
        uint32_t bit = (dest_ip >> (31 - i)) & 1;
        if (!current->children[bit]) {
            break;
        }
        current = current->children[bit].get();
        if (current->entry.has_value()) {
            best_match = current->entry;
        }
    }

    return best_match;
}

std::optional<RouteEntry> LPMTrie::lookup(const std::string& ip_str) const {
    return lookup(ip_from_string(ip_str));
}

bool LPMTrie::delete_route(uint32_t prefix, uint8_t prefix_len) {
    if (prefix_len > 32) return false;

    TrieNode* current = root_.get();
    for (uint8_t i = 0; i < prefix_len; ++i) {
        uint32_t bit = (prefix >> (31 - i)) & 1;
        if (!current->children[bit]) {
            return false;
        }
        current = current->children[bit].get();
    }

    if (current->entry.has_value()) {
        current->entry = std::nullopt;
        route_count_--;
        return true;
    }
    return false;
}

} // namespace openpath
