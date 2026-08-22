#pragma once

// Provides std::size_t and std::uint64_t.
#include <cstddef>
#include <cstdint>

// Provides std::pair.
#include <utility>

// Provides std::string.
#include <string>

// Provides std::vector.
#include <vector>

#include "strategies/strategy.hpp"


// Selects backend servers using consistent hashing.
//
// Consistent hashing maps both request keys and backend servers onto the same
// hash ring. A request is assigned to the first server position at or after the
// request hash, wrapping around to the beginning of the ring when needed.
//
// This is useful for affinity-style routing:
//
//     same request key + same active server set -> same backend
//
// When a backend is added or removed, only a portion of keys move to different
// backends instead of reshuffling all traffic.
class ConsistentHashingStrategy final : public Strategy {
public:
    // virtual_node_count controls how many ring positions each real backend
    // receives.
    //
    // More virtual nodes generally improve key distribution across backends,
    // at the cost of building a larger ring for each selection.
    explicit ConsistentHashingStrategy(
        std::size_t virtual_node_count = 100
    );


    // Select the backend responsible for this request's key.
    //
    // Current key choice:
    //
    //     request.message()
    //
    // If the message is empty, all empty-key requests map consistently to the
    // same backend for a stable active server set.
    std::shared_ptr<Server> selectServer(
        const std::vector<std::shared_ptr<Server>>& servers,
        const loadbalancer::Request& request
    ) override;


private:
    using RingEntry = std::pair<std::uint64_t, std::shared_ptr<Server>>;


    // Stable 64-bit FNV-1a hash.
    //
    // We avoid std::hash here because its result is not guaranteed to be stable
    // across standard-library implementations.
    static std::uint64_t hashKey(const std::string& key);


    // Build a sorted hash ring from the active server list.
    std::vector<RingEntry> buildRing(
        const std::vector<std::shared_ptr<Server>>& servers
    ) const;


    // Number of virtual nodes placed on the ring for each backend.
    std::size_t virtual_node_count_;
};
