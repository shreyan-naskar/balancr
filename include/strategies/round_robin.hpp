#pragma once

// Provides std::atomic.
#include <atomic>

// Provides std::size_t.
#include <cstddef>

#include "strategies/strategy.hpp"


// Selects backend servers in circular order.
//
// With these active servers:
//
//     [51000, 51001, 51002]
//
// Requests are assigned like this:
//
//     Request 1 -> 51000
//     Request 2 -> 51001
//     Request 3 -> 51002
//     Request 4 -> 51000
//     Request 5 -> 51001
class RoundRobinStrategy final : public Strategy {
public:
    // Select the next backend in the round-robin sequence.
    //
    // The function body will be implemented in round_robin.cpp.
    std::shared_ptr<Server> selectServer(
        const std::vector<std::shared_ptr<Server>>& servers,
        const loadbalancer::Request& request
    ) override;


private:
    // Index used to determine which backend is selected next.
    //
    // Atomic is required because gRPC may call HandleRequest from
    // multiple threads at the same time.
    //
    // Initial value:
    //
    //     0
    //
    // means the first request selects servers[0].
    std::atomic<std::size_t> next_index_{0};
};