#include <exception>
#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "api/admin_service.hpp"
#include "core/load_balancer.hpp"
#include "core/server_manager.hpp"
#include "strategies/round_robin.hpp"

int main(int argc, char** argv) {
    /*
        Expected command:

        ./load_balancer <backend_executable_path> <load_balancer_port> <backend_start_port> <min_servers> <max_servers>

        Example:

        ./load_balancer ./backend_server 5000 6000 2 5

        Meaning:
        - Load balancer listens on port 5000
        - First backend starts on port 6000
        - Minimum 2 backend servers will be started automatically
        - Maximum 5 backend servers are allowed
    */
    if (argc != 6) {
        std::cerr
            << "Usage: " << argv[0]
            << " <backend_executable_path>"
            << " <load_balancer_port>"
            << " <backend_start_port>"
            << " <min_servers>"
            << " <max_servers>\n";

        return 1;
    }

    try {
        const std::string backend_executable_path = argv[1];

        /*
            Convert command-line strings into numbers.

            argv values always come as text, so:
            - std::stoi converts to int
            - std::stoul converts to unsigned long

            We later store min/max as size_t because server counts
            should never be negative.
        */
        const int load_balancer_port = std::stoi(argv[2]);
        const int backend_start_port = std::stoi(argv[3]);
        const std::size_t min_servers = std::stoul(argv[4]);
        const std::size_t max_servers = std::stoul(argv[5]);

        /*
            Basic validation.

            Ports must be valid TCP ports.
            Also, min_servers cannot be greater than max_servers,
            because the manager cannot start more minimum servers
            than the allowed maximum.
        */
        if (load_balancer_port < 1 || load_balancer_port > 65535) {
            std::cerr << "Load balancer port must be between 1 and 65535\n";
            return 1;
        }

        if (backend_start_port < 1 || backend_start_port > 65535) {
            std::cerr << "Backend start port must be between 1 and 65535\n";
            return 1;
        }

        if (min_servers > max_servers) {
            std::cerr << "min_servers cannot be greater than max_servers\n";
            return 1;
        }

        /*
            ServerManager owns backend server management.

            During construction, it starts min_servers backend processes.

            Example:
            backend_start_port = 6000
            min_servers = 2

            It starts:
            - backend on port 6000
            - backend on port 6001
        */
        auto server_manager = std::make_shared<ServerManager>(
            backend_executable_path,
            backend_start_port,
            min_servers,
            max_servers
        );

        /*
            Strategy decides how traffic is distributed.

            Here we use RoundRobinStrategy:

            request 1 -> server 1
            request 2 -> server 2
            request 3 -> server 3
            request 4 -> server 1 again
        */
        auto strategy = std::make_shared<RoundRobinStrategy>();

        /*
            Main client-facing gRPC service.

            This receives client requests, asks the strategy to choose
            a backend, then forwards the request to that backend.
        */
        LoadBalancerService load_balancer_service(
            server_manager,
            strategy
        );

        /*
            Admin gRPC service.

            This gives us server management operations:

            - ListServers
            - AddServer
            - RemoveServer
        */
        AdminService admin_service(server_manager);

        /*
            Address where the load balancer itself listens.

            0.0.0.0 means:
            "listen on all network interfaces"

            So clients can call:
            localhost:<load_balancer_port>
        */
        const std::string load_balancer_address =
            "0.0.0.0:" + std::to_string(load_balancer_port);

        grpc::ServerBuilder builder;

        /*
            Add the network port for this gRPC server.
        */
        builder.AddListeningPort(
            load_balancer_address,
            grpc::InsecureServerCredentials()
        );

        /*
            Register both services on the same gRPC server.

            One physical gRPC server can expose multiple services.
            So this same process handles:

            - LoadBalancerService.HandleRequest
            - AdminService.ListServers/AddServer/RemoveServer
        */
        builder.RegisterService(&load_balancer_service);
        builder.RegisterService(&admin_service);

        /*
            BuildAndStart actually starts the gRPC server.
        */
        std::unique_ptr<grpc::Server> server = builder.BuildAndStart();

        if (!server) {
            std::cerr << "Failed to start load balancer on "
                      << load_balancer_address << "\n";
            return 1;
        }

        std::cout << "Load balancer listening on "
                  << load_balancer_address << "\n";

        std::cout << "Backend executable: "
                  << backend_executable_path << "\n";

        std::cout << "Backend ports start from: "
                  << backend_start_port << "\n";

        std::cout << "Server limits: min="
                  << min_servers
                  << ", max="
                  << max_servers << "\n";

        /*
            Wait blocks forever while the load balancer runs.

            The program stays alive here and keeps accepting requests.
        */
        server->Wait();

        return 0;
    } catch (const std::exception& error) {
        /*
            If std::stoi/std::stoul fails, or ServerManager throws,
            we print the error here.
        */
        std::cerr << "Startup error: " << error.what() << "\n";
        return 1;
    }
}