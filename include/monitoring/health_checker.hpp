#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "proto/admin_service.grpc.pb.h"


// These thresholds control autoscaling.

// If backend CPU goes above 80%, we scale up.
// If backend CPU goes below 20%, we scale down.

// They are declared here as extern, meaning:
// - header only announces them
// - actual values are defined in health_checker.cpp

extern const double SCALE_UP_CPU_THRESHOLD;
extern const double SCALE_DOWN_CPU_THRESHOLD;


// Time gap between health checks.

// Example:
// health_checker_sleep_time = 15 means the health checker
// checks all backend servers every 15 seconds.

extern int health_checker_sleep_time;

class HealthChecker {
public:
    
    // lb_admin_address is the address of the load balancer's admin gRPC server.

    // Example:
    // 127.0.0.1:5000

    // We use this address to call:
    // - ListServers
    // - AddServer
    // - RemoveServer
    // - UpdateServerHealth
    // - GetServerConstraints
    
    explicit HealthChecker(const std::string& lb_admin_address);

    
    // Destructor stops the background thread safely.
    
    ~HealthChecker();

    
    // We do not allow copying because this class owns:
    // - a running thread
    // - a gRPC stub
    // - shared running state

    // Copying that would be dangerous.
    
    HealthChecker(const HealthChecker&) = delete;
    HealthChecker& operator=(const HealthChecker&) = delete;

    
    // Start continuous health checking in a background thread.
    
    void start();

    
    // Stop the background health checker thread.
    
    void stop();

    
    // Run exactly one health check cycle.

    // This is useful for testing because you can call one cycle
    // and immediately see what happens.
    
    void checkServersOnce();

private:
    
    // Loop function used by the background thread.

    // It keeps calling checkServersOnce() until running_ becomes false.
    
    void checkHealth();

    
    // Ask the load balancer for the current backend server list.
    
    std::vector<admin::ServerInfo> listAllServers();

    
    // Check whether a backend server is reachable.

    // If this returns false, we treat that backend as unhealthy.
    
    bool isServerResponding(const std::string& host, int port);

    
    // Send health/CPU/memory updates back to the load balancer.
    
    void updateServerHealth(
        const std::vector<admin::UpdateServerHealthRequest>& updates
    );

    
    // Ask one backend server for CPU and memory usage.
    
    bool getServerMetrics(
        const std::string& host,
        int port,
        double& out_cpu,
        double& out_memory
    );

    
    // Ask load balancer for min/max/active server limits.
    
    admin::ServerConstraintsResponse getServerLimits();

    
    // Ask load balancer to start one more backend server.
    
    void addServer();

    
    // Ask load balancer to remove one backend server.
    
    void removeServer(const std::string& server_id);

    
    // Decide whether to scale up or scale down using CPU usage.
    
    void handleAutoScaling(
        double cpu,
        const std::string& server_id,
        const admin::ServerConstraintsResponse& constraints
    );

    
    // True while the background health checker loop should run.
    
    std::atomic<bool> running_{false};

    
    // Background thread that runs checkHealth().
    
    std::unique_ptr<std::thread> health_check_thread_;

    
    // gRPC client used to call the load balancer's AdminService.
    
    std::unique_ptr<admin::AdminService::Stub> admin_stub_;
};