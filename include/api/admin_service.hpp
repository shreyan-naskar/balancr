#pragma once

// Provides std::shared_ptr.
#include <memory>

// Provides gRPC types such as grpc::ServerContext and grpc::Status.
#include <grpcpp/grpcpp.h>

// Generated AdminService base class and generated messages.
#include "proto/admin_service.grpc.pb.h"

#include "core/server_manager.hpp"


// Implements administrative gRPC operations for ServerManager.
//
// The class inherits from the service generated from:
//
//     proto/admin_service.proto
//
// It acts as a thin API layer:
//
//     Admin client
//          │
//          ▼
//     AdminService
//          │
//          ▼
//     ServerManager
//          │
//          ├── addServer()
//          ├── removeServerById()
//          └── getAllServers()
class AdminService final
    : public admin::AdminService::Service {
public:
    // Store a shared reference to the same ServerManager used by
    // LoadBalancerService.
    explicit AdminService(
        std::shared_ptr<ServerManager> server_manager
    );


    // Return information about all servers currently managed.
    //
    // Request:
    //
    //     google::protobuf::Empty
    //
    // Response:
    //
    //     admin::ListServersResponse
    grpc::Status ListServers(
        grpc::ServerContext* context,
        const google::protobuf::Empty* request,
        admin::ListServersResponse* response
    ) override;


    // Create and start one new backend server.
    //
    // Request:
    //
    //     google::protobuf::Empty
    //
    // Response:
    //
    //     admin::AddServerResponse
    //
    // The response contains the ID of the newly created backend.
    grpc::Status AddServer(
        grpc::ServerContext* context,
        const google::protobuf::Empty* request,
        admin::AddServerResponse* response
    ) override;


    // Stop and remove one backend server.
    //
    // Request:
    //
    //     admin::RemoveServerRequest
    //
    // The request contains:
    //
    //     id = "127.0.0.1:51001"
    //
    // Response:
    //
    //     google::protobuf::Empty
    grpc::Status RemoveServer(
        grpc::ServerContext* context,
        const admin::RemoveServerRequest* request,
        google::protobuf::Empty* response
    ) override;


private:
    // Shared server manager used by both administrative actions and
    // request-routing actions.
    std::shared_ptr<ServerManager> server_manager_;
};