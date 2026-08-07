#include "api/admin_service.hpp"


// Store a shared reference to the same ServerManager used by the
// LoadBalancerService.
//
// AdminService does not own ServerManager exclusively. Both services
// must operate on the same backend-server collection.
AdminService::AdminService(
    std::shared_ptr<ServerManager> server_manager
)
    : server_manager_(std::move(server_manager)) {
}


// Return all currently managed backend servers.
grpc::Status AdminService::ListServers(
    grpc::ServerContext* context,
    const google::protobuf::Empty* request,
    admin::ListServersResponse* response
) {
    // This RPC does not need client metadata or input fields.
    (void)context;
    (void)request;


    // Get a copy of the server references from ServerManager.
    //
    // This returns all managed servers, including a server that may be
    // unhealthy. That is useful for administration because an operator
    // should be able to see the entire managed collection.
    auto servers = server_manager_->getAllServers();


    // Convert every C++ Server object into the Protocol Buffers
    // ServerInfo message returned through gRPC.
    for (const auto& server : servers) {
        // add_servers() appends a new ServerInfo to the repeated field
        // and returns a pointer to it.
        admin::ServerInfo* info =
            response->add_servers();


        // Copy the Server object's values into the generated message.
        info->set_id(server->getId());
        info->set_host(server->getAddress());
        info->set_port(server->getPort());

        // The proto field is named isHealthy. The generated C++ method
        // normalizes this as set_ishealthy().
        info->set_ishealthy(server->isHealthy());

        info->set_request_count(
            server->getRequestCount()
        );
    }


    // gRPC serializes response and sends it to the caller.
    return grpc::Status::OK;
}


// Create one new backend server.
grpc::Status AdminService::AddServer(
    grpc::ServerContext* context,
    const google::protobuf::Empty* request,
    admin::AddServerResponse* response
) {
    // AddServer requires no data from the caller.
    (void)context;
    (void)request;


    // ServerManager performs the actual work:
    //
    // - verifies the maximum server limit
    // - chooses the next port
    // - starts backend_server
    // - creates and stores a Server object
    auto new_server = server_manager_->addServer();


    // nullptr means that the maximum count was reached or the process
    // could not be started.
    if (new_server == nullptr) {
        return grpc::Status(
            grpc::StatusCode::RESOURCE_EXHAUSTED,
            "Maximum server count reached or backend could not start"
        );
    }


    // Return the new backend's unique ID.
    //
    // Example:
    //
    //     127.0.0.1:51003
    response->set_id(new_server->getId());


    return grpc::Status::OK;
}


// Stop and remove one backend server.
grpc::Status AdminService::RemoveServer(
    grpc::ServerContext* context,
    const admin::RemoveServerRequest* request,
    google::protobuf::Empty* response
) {
    // This basic RPC does not use client metadata or response fields.
    (void)context;
    (void)response;


    // Read the ID supplied by the administrative client.
    //
    // Example:
    //
    //     "127.0.0.1:51001"
    const std::string server_id = request->id();


    // ServerManager:
    //
    // - checks the minimum-server limit
    // - finds the matching Server
    // - terminates its backend process
    // - removes it from the server collection
    const bool removed =
        server_manager_->removeServerById(server_id);


    // Removal fails if:
    //
    // - No server has this ID.
    // - Removing it would go below min_servers_.
    if (!removed) {
        return grpc::Status(
            grpc::StatusCode::FAILED_PRECONDITION,
            "Server was not found or minimum server count was reached"
        );
    }


    // A successful RemoveServer RPC returns an empty response body.
    return grpc::Status::OK;
}