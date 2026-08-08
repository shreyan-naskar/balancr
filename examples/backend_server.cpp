#include <iostream>
#include <memory>
#include <string>

// gRPC server classes.
#include <grpcpp/grpcpp.h>
#include <google/protobuf/empty.pb.h>

// Generated service, Request, and Response classes.
#include "proto/admin_service.grpc.pb.h"
#include "proto/load_balancer.grpc.pb.h"


// Implements the same service interface declared in:
//
//     proto/load_balancer.proto
//
// The load balancer will create a gRPC client stub and call
// HandleRequest() on this backend.
class BackendServer final
    : public loadbalancer::LoadBalancerService::Service {
public:
    // Store the port so this backend can identify itself in logs and
    // responses.
    explicit BackendServer(int port)
        : port_(port) {
    }


    // Handle a request forwarded by the load balancer.
    grpc::Status HandleRequest(
        grpc::ServerContext* context,
        const loadbalancer::Request* request,
        loadbalancer::Response* response
    ) override {
        // This basic backend does not use request metadata.
        (void)context;


        // Print information so we can observe round-robin distribution
        // in the backend terminal output.
        //
        // Example:
        //
        //     Backend on port 51001 received: Hello
        std::cout
            << "Backend on port "
            << port_
            << " received: "
            << request->message()
            << '\n';


        // Build the message returned to the load balancer.
        response->set_message(
            "Processed by backend on port " +
            std::to_string(port_)
        );


        // This identifies the backend itself.
        //
        // The load balancer later replaces this with its own Server ID:
        //
        //     127.0.0.1:51001
        //
        // That is why this field is mainly useful here for debugging.
        response->set_server_id(
            "backend_" + std::to_string(port_)
        );


        // Tell gRPC that the request was handled successfully.
        return grpc::Status::OK;
    }


private:
    // Port on which this particular backend instance listens.
    int port_;
};

class BackendAdminService final
    : public admin::AdminService::Service {
public:
    explicit BackendAdminService(int port)
        : port_(port) {
    }

    grpc::Status GetMetrics(
        grpc::ServerContext* context,
        const google::protobuf::Empty* request,
        admin::MetricsResponse* response
    ) override {
        (void)context;
        (void)request;

        response->set_cpu_usage(45.0 + static_cast<double>(port_ % 10));
        response->set_memory_usage(25.0);

        return grpc::Status::OK;
    }

private:
    int port_;
};


// Program entry point.
//
// ServerManager starts this executable with exactly one argument:
//
//     ./backend_server 51000
int main(int argc, char** argv) {
    // argv[0] is the executable name.
    //
    // argv[1] must be the listening port.
    if (argc != 2) {
        std::cerr
            << "Usage: "
            << argv[0]
            << " <port>\n";

        return 1;
    }


    int port;

    try {
        // Convert text, such as "51000", into an integer.
        port = std::stoi(argv[1]);
    } catch (const std::exception&) {
        std::cerr << "Port must be a valid integer\n";
        return 1;
    }


    // Validate the TCP-port range.
    if (port < 1 || port > 65535) {
        std::cerr
            << "Port must be between 1 and 65535\n";

        return 1;
    }


    // gRPC server address.
    //
    // 0.0.0.0 means "listen on all network interfaces."
    //
    // The load balancer connects using 127.0.0.1:<port>, which is
    // allowed because 127.0.0.1 is one of this machine's interfaces.
    const std::string server_address =
        "0.0.0.0:" + std::to_string(port);


    // Create one service instance for this backend process.
    BackendServer service(port);
    BackendAdminService admin_service(port);


    // Configure and start the gRPC server.
    grpc::ServerBuilder builder;

    builder.AddListeningPort(
        server_address,
        grpc::InsecureServerCredentials()
    );

    // Register the object that implements HandleRequest().
    builder.RegisterService(&service);
    builder.RegisterService(&admin_service);


    // BuildAndStart() returns nullptr if startup fails, for example
    // because another process already uses the port.
    std::unique_ptr<grpc::Server> grpc_server =
        builder.BuildAndStart();

    if (grpc_server == nullptr) {
        std::cerr
            << "Could not start backend on "
            << server_address
            << '\n';

        return 1;
    }


    std::cout
        << "Backend server listening on "
        << server_address
        << '\n';


    // Keep this process alive and accepting gRPC calls.
    //
    // This blocks until the server is shut down or the process receives
    // a termination signal from ServerManager::removeServerById().
    grpc_server->Wait();

    return 0;
}
