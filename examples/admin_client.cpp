#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include <google/protobuf/empty.pb.h>

#include "proto/admin_service.grpc.pb.h"

void listServers(admin::AdminService::Stub* stub) {
    google::protobuf::Empty request;
    admin::ListServersResponse response;
    grpc::ClientContext context;

    grpc::Status status = stub->ListServers(
        &context,
        request,
        &response
    );

    if (!status.ok()) {
        std::cerr << "ListServers failed: "
                  << status.error_message() << "\n";
        return;
    }

    std::cout << "\nServers:\n";

    for (const auto& server : response.servers()) {
        std::cout
            << "- id=" << server.id()
            << ", host=" << server.host()
            << ", port=" << server.port()
            << ", healthy=" << server.ishealthy()
            << ", requests=" << server.request_count()
            << "\n";
    }
}

void addServer(admin::AdminService::Stub* stub) {
    google::protobuf::Empty request;
    admin::AddServerResponse response;
    grpc::ClientContext context;

    grpc::Status status = stub->AddServer(
        &context,
        request,
        &response
    );

    if (!status.ok()) {
        std::cerr << "AddServer failed: "
                  << status.error_message() << "\n";
        return;
    }

    std::cout << "Added server id=" << response.id() << "\n";
}

void removeServer(admin::AdminService::Stub* stub, const std::string& id) {
    admin::RemoveServerRequest request;
    google::protobuf::Empty response;
    grpc::ClientContext context;

    request.set_id(id);

    grpc::Status status = stub->RemoveServer(
        &context,
        request,
        &response
    );

    if (!status.ok()) {
        std::cerr << "RemoveServer failed: "
                  << status.error_message() << "\n";
        return;
    }

    std::cout << "Removed server id=" << id << "\n";
}

int main(int argc, char** argv) {
    /*
        Expected commands:

        ./admin_client <load_balancer_address> list
        ./admin_client <load_balancer_address> add
        ./admin_client <load_balancer_address> remove <server_id>

        Examples:

        ./admin_client localhost:5000 list
        ./admin_client localhost:5000 add
        ./admin_client localhost:5000 remove 127.0.0.1:6002
    */
    if (argc < 3) {
        std::cerr
            << "Usage:\n"
            << "  " << argv[0] << " <load_balancer_address> list\n"
            << "  " << argv[0] << " <load_balancer_address> add\n"
            << "  " << argv[0] << " <load_balancer_address> remove <server_id>\n";

        return 1;
    }

    const std::string load_balancer_address = argv[1];
    const std::string command = argv[2];

    auto channel = grpc::CreateChannel(
        load_balancer_address,
        grpc::InsecureChannelCredentials()
    );

    auto stub = admin::AdminService::NewStub(channel);

    if (command == "list") {
        listServers(stub.get());
        return 0;
    }

    if (command == "add") {
        addServer(stub.get());
        return 0;
    }

    if (command == "remove") {
        if (argc != 4) {
            std::cerr << "remove requires <server_id>\n";
            return 1;
        }

        removeServer(stub.get(), argv[3]);
        return 0;
    }

    std::cerr << "Unknown command: " << command << "\n";
    return 1;
}