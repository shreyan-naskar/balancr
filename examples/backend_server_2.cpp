#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <chrono>
#include <sys/resource.h>

// gRPC server classes.
#include <grpcpp/grpcpp.h>

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
    : public loadbalancer::LoadBalancerService::Service,
      public admin::AdminService::Service {
public:
    // Store the port so this backend can identify itself in logs and
    // responses.
    explicit BackendServer(int port)
        : port_(port),
          last_cpu_seconds_(readProcessCpuSeconds()),
          last_sample_time_(std::chrono::steady_clock::now()) {
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

    grpc::Status GetMetrics(
        grpc::ServerContext* context,
        const google::protobuf::Empty* request,
        admin::MetricsResponse* response
    ) override {
        (void)context;
        (void)request;

        const double memory_mb = readProcessMemoryMb();
        const double cpu_percent = readProcessCpuPercent();

        response->set_cpu_usage(cpu_percent);
        response->set_memory_usage(memory_mb);

        return grpc::Status::OK;
    }


private:
    static double readProcessCpuSeconds() {
        rusage usage{};
        if (getrusage(RUSAGE_SELF, &usage) != 0) {
            return 0.0;
        }

        const double user_seconds =
            static_cast<double>(usage.ru_utime.tv_sec) +
            static_cast<double>(usage.ru_utime.tv_usec) / 1000000.0;

        const double system_seconds =
            static_cast<double>(usage.ru_stime.tv_sec) +
            static_cast<double>(usage.ru_stime.tv_usec) / 1000000.0;

        return user_seconds + system_seconds;
    }

    static double readProcessMemoryMb() {
        rusage usage{};
        if (getrusage(RUSAGE_SELF, &usage) != 0) {
            return 0.0;
        }

#ifdef __APPLE__
        return static_cast<double>(usage.ru_maxrss) / (1024.0 * 1024.0);
#else
        return static_cast<double>(usage.ru_maxrss) / 1024.0;
#endif
    }

    double readProcessCpuPercent() {
        std::lock_guard<std::mutex> lock(metrics_mutex_);

        const double current_cpu_seconds = readProcessCpuSeconds();
        const auto current_time = std::chrono::steady_clock::now();

        const std::chrono::duration<double> elapsed =
            current_time - last_sample_time_;

        double cpu_percent = 0.0;
        if (elapsed.count() > 0.0) {
            cpu_percent =
                ((current_cpu_seconds - last_cpu_seconds_) / elapsed.count()) *
                100.0;
        }

        last_cpu_seconds_ = current_cpu_seconds;
        last_sample_time_ = current_time;

        if (cpu_percent < 0.0) {
            return 0.0;
        }

        return cpu_percent;
    }

    // Port on which this particular backend instance listens.
    int port_;

    std::mutex metrics_mutex_;
    double last_cpu_seconds_;
    std::chrono::steady_clock::time_point last_sample_time_;
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


    // Configure and start the gRPC server.
    grpc::ServerBuilder builder;

    builder.AddListeningPort(
        server_address,
        grpc::InsecureServerCredentials()
    );

    // Register both services implemented by this backend.
    builder.RegisterService(
        static_cast<loadbalancer::LoadBalancerService::Service*>(&service)
    );
    builder.RegisterService(
        static_cast<admin::AdminService::Service*>(&service)
    );


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