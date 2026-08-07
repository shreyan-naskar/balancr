#include "core/load_balancer.hpp"


// Store shared references to the manager and selected strategy.
//
// The constructor does not create new objects. It receives the same
// ServerManager and Strategy instances created in main.cpp.
LoadBalancerService::LoadBalancerService(
    std::shared_ptr<ServerManager> server_manager,
    std::shared_ptr<Strategy> strategy
)
    : server_manager_(std::move(server_manager)),
      strategy_(std::move(strategy)) {
}


// Handle one request received from a client.
grpc::Status LoadBalancerService::HandleRequest(
    grpc::ServerContext* context,
    const loadbalancer::Request* request,
    loadbalancer::Response* response
) {
    // We do not use client metadata in this basic implementation.
    //
    // The context is still required because gRPC passes it for every RPC.
    (void)context;


    // Ask ServerManager for backends that are both:
    //
    // - marked healthy
    // - still running as operating-system processes
    //
    // This prevents the load balancer from selecting a removed, crashed,
    // or unhealthy backend.
    auto active_servers = server_manager_->getActiveServers();


    // Ask the selected strategy to choose one server.
    //
    // At this stage, strategy_ is RoundRobinStrategy.
    auto selected_server = strategy_->selectServer(
        active_servers,
        *request
    );


    // No request can be forwarded when no active backend exists.
    if (selected_server == nullptr) {
        return grpc::Status(
            grpc::StatusCode::UNAVAILABLE,
            "No active backend servers are available"
        );
    }


    // Build the address of the selected backend.
    //
    // Example:
    //
    //     address = "127.0.0.1"
    //     port    = 51001
    //
    // Result:
    //
    //     backend_address = "127.0.0.1:51001"
    const std::string backend_address =
        selected_server->getAddress() +
        ":" +
        std::to_string(selected_server->getPort());


    // Create a gRPC communication channel to the selected backend.
    //
    // InsecureChannelCredentials() is suitable for local development.
    // Production systems should normally use TLS credentials.
    auto channel = grpc::CreateChannel(
        backend_address,
        grpc::InsecureChannelCredentials()
    );


    // Create a client stub for calling the backend's
    // LoadBalancerService.
    //
    // The backend implements the same HandleRequest RPC that the client
    // called on this load balancer.
    auto backend_stub =
        loadbalancer::LoadBalancerService::NewStub(channel);


    // Context for the load-balancer-to-backend RPC.
    //
    // This is separate from the incoming client context.
    grpc::ClientContext backend_context;


    // This object receives the backend's response.
    loadbalancer::Response backend_response;


    // Forward the original request to the selected backend.
    //
    // This call waits until the backend sends a response or returns an
    // error, such as UNAVAILABLE when the backend cannot be reached.
    grpc::Status backend_status =
        backend_stub->HandleRequest(
            &backend_context,
            *request,
            &backend_response
        );


    // If the backend RPC failed, return that error directly to the
    // original client.
    if (!backend_status.ok()) {
        // Mark it unhealthy so the next request does not keep selecting
        // a backend that just failed.
        //
        // A future health checker can confirm whether it recovered.
        selected_server->setHealthStatus(false);

        return backend_status;
    }


    // Count the request only after the backend successfully handled it.
    selected_server->incrementRequestCount();


    // Copy the backend message into the response sent to the original
    // client.
    response->set_message(backend_response.message());


    // Report which backend the load balancer selected.
    //
    // We use Server's ID rather than backend_response.server_id() so
    // the response always matches the load balancer's own server record.
    response->set_server_id(selected_server->getId());


    // gRPC sends response back to the client.
    return grpc::Status::OK;
}