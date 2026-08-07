#pragma once

// Provides std::shared_ptr.
#include <memory>

// Provides std::vector.
#include <vector>

#include "core/server.hpp"

// Provides loadbalancer::Request, generated from:
//
//     proto/load_balancer.proto
#include "proto/load_balancer.pb.h"


// Abstract base class for load-balancing algorithms.
//
// Every strategy receives the currently active backend servers and
// returns one selected server.
//
// Current strategy:
//
//     RoundRobinStrategy
//
// Possible future strategies:
//
//     LeastConnectionsStrategy
//     ResourceBasedStrategy
//     WeightedRoundRobinStrategy
class Strategy {
public:
    // Virtual destructor is required because strategies are handled
    // through pointers to the base class:
    //
    //     std::shared_ptr<Strategy> strategy =
    //         std::make_shared<RoundRobinStrategy>();
    //
    // When strategy is destroyed, the derived class destructor will
    // run correctly.
    virtual ~Strategy() = default;


    // Select one backend server for an incoming request.
    //
    // Parameters:
    //
    // servers
    //     Contains only healthy, running backends. It comes from:
    //
    //         ServerManager::getActiveServers()
    //
    // request
    //     The client request. Round-robin does not inspect it, but
    //     future strategies may use it for routing decisions.
    //
    // Return value:
    //
    //     shared_ptr<Server>
    //         The selected backend.
    //
    //     nullptr
    //         No backend can be selected, normally because servers is
    //         empty.
    virtual std::shared_ptr<Server> selectServer(
        const std::vector<std::shared_ptr<Server>>& servers,
        const loadbalancer::Request& request
    ) = 0;
};