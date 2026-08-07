#pragma once

// Provides std::shared_ptr.
#include <memory>

// Provides gRPC server types such as grpc::ServerContext and
// grpc::Status.
#include <grpcpp/grpcpp.h>

#include "core/server_manager.hpp"

// Provides the generated LoadBalancerService base class,
// plus loadbalancer::Request and loadbalancer::Response.
#include "proto/load_balancer.grpc.pb.h"

#include "strategies/strategy.hpp"


// gRPC implementation of the client-facing load-balancing service.
//
// It inherits from the class generated from this protocol:
//
//     proto/load_balancer.proto
//
// That generated class requires an implementation of:
//
//     HandleRequest(...)
class LoadBalancerService final
    : public loadbalancer::LoadBalancerService::Service {
public:
    // Construct the load balancer using:
    //
    // server_manager
    //     Tracks backend processes and provides healthy/running servers.
    //
    // strategy
    //     Selects one server from the active server list.
    //
    // Example:
    //
    //     auto manager = std::make_shared<ServerManager>(...);
    //     auto strategy = std::make_shared<RoundRobinStrategy>();
    //
    //     LoadBalancerService service(manager, strategy);
    LoadBalancerService(
        std::shared_ptr<ServerManager> server_manager,
        std::shared_ptr<Strategy> strategy
    );


    // Handle one client request.
    //
    // Full request flow:
    //
    //     Client
    //       |
    //       | HandleRequest(Request)
    //       v
    //     LoadBalancerService
    //       |
    //       | 1. getActiveServers()
    //       | 2. strategy_->selectServer(...)
    //       | 3. forward HandleRequest(...) to selected backend
    //       v
    //     Backend Server
    //       |
    //       | Response
    //       v
    //     Client
    //
    // Parameters:
    //
    // context
    //     Contains metadata and cancellation/deadline information for
    //     the client-to-load-balancer RPC.
    //
    // request
    //     The request received from the client.
    //
    // response
    //     The object that this method fills before returning to the
    //     client.
    grpc::Status HandleRequest(
        grpc::ServerContext* context,
        const loadbalancer::Request* request,
        loadbalancer::Response* response
    ) override;


private:
    // Shared manager for backend lifecycle and server queries.
    std::shared_ptr<ServerManager> server_manager_;

    // Shared load-balancing algorithm.
    //
    // Although we use RoundRobinStrategy now, this base type allows
    // another strategy to be substituted without changing this service.
    std::shared_ptr<Strategy> strategy_;
};