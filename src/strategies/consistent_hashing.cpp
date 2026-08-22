#include "strategies/consistent_hashing.hpp"

// Provides std::lower_bound and std::sort.
#include <algorithm>

// Provides std::string and std::to_string.
#include <string>


ConsistentHashingStrategy::ConsistentHashingStrategy(
    std::size_t virtual_node_count
)
    : virtual_node_count_(virtual_node_count == 0 ? 1 : virtual_node_count) {
}


std::shared_ptr<Server>
ConsistentHashingStrategy::selectServer(
    const std::vector<std::shared_ptr<Server>>& servers,
    const loadbalancer::Request& request
) {
    if (servers.empty()) {
        return nullptr;
    }

    std::vector<RingEntry> ring = buildRing(servers);

    if (ring.empty()) {
        return nullptr;
    }

    const std::uint64_t request_hash =
        hashKey(request.message());

    auto iterator = std::lower_bound(
        ring.begin(),
        ring.end(),
        request_hash,
        [](const RingEntry& entry, std::uint64_t value) {
            return entry.first < value;
        }
    );

    if (iterator == ring.end()) {
        iterator = ring.begin();
    }

    return iterator->second;
}


std::uint64_t
ConsistentHashingStrategy::hashKey(
    const std::string& key
) {
    constexpr std::uint64_t fnv_offset_basis =
        14695981039346656037ull;
    constexpr std::uint64_t fnv_prime =
        1099511628211ull;

    std::uint64_t hash = fnv_offset_basis;

    for (unsigned char character : key) {
        hash ^= static_cast<std::uint64_t>(character);
        hash *= fnv_prime;
    }

    return hash;
}


std::vector<ConsistentHashingStrategy::RingEntry>
ConsistentHashingStrategy::buildRing(
    const std::vector<std::shared_ptr<Server>>& servers
) const {
    std::vector<RingEntry> ring;

    ring.reserve(
        servers.size() * virtual_node_count_
    );

    for (const auto& server : servers) {
        if (server == nullptr) {
            continue;
        }

        const std::string server_id = server->getId();

        for (std::size_t i = 0; i < virtual_node_count_; ++i) {
            const std::string virtual_node_key =
                server_id + "#" + std::to_string(i);

            ring.emplace_back(
                hashKey(virtual_node_key),
                server
            );
        }
    }

    std::sort(
        ring.begin(),
        ring.end(),
        [](const RingEntry& left, const RingEntry& right) {
            return left.first < right.first;
        }
    );

    return ring;
}

