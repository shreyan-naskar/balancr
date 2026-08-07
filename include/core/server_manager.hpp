#pragma once

// Provides std::size_t.
#include <cstddef>

// Provides std::shared_ptr.
#include <memory>

// Protects the server collection from concurrent access.
#include <mutex>

// Used for the backend executable path and server IDs.
#include <string>

// Stores the managed backend servers.
#include <vector>

#include "core/server.hpp"


// Manages the complete collection of backend servers.
//
// Responsibilities:
//
// 1. Start the minimum number of backend servers during construction.
// 2. Add new backend processes.
// 3. Remove existing backend processes.
// 4. Enforce minimum and maximum server limits.
// 5. Return all known servers.
// 6. Return only healthy and running servers to the load balancer.
// 7. Find a server by its ID.
//
// ServerManager is shared by:
//
//     LoadBalancerService
//         Uses getActiveServers() to select request destinations.
//
//     AdminService
//         Uses addServer(), removeServerById(), and getAllServers().
class ServerManager {
public:
    // Create the manager and start the configured minimum number
    // of backend servers.
    //
    // Example:
    //
    //     ServerManager manager(
    //         "./backend_server", // executable_path
    //         51000,              // first backend port
    //         2,                  // minimum servers
    //         5                   // maximum servers
    //     );
    //
    // The constructor will attempt to start:
    //
    //     ./backend_server 51000
    //     ./backend_server 51001
    ServerManager(
        const std::string& executable_path,
        int start_port,
        std::size_t min_servers,
        std::size_t max_servers
    );


    // ---------------------------------------------------------------
    // Server creation
    // ---------------------------------------------------------------

    // Start one new backend server.
    //
    // The manager:
    //
    // 1. Checks the maximum-server limit.
    // 2. Chooses the next port.
    // 3. Creates an OS-specific Process.
    // 4. Starts the backend executable.
    // 5. Creates a Server object.
    // 6. Stores it in servers_.
    //
    // Returns:
    //
    //     shared_ptr<Server> -> server was added successfully
    //     nullptr            -> limit reached or process start failed
    std::shared_ptr<Server> addServer();


    // ---------------------------------------------------------------
    // Server removal
    // ---------------------------------------------------------------

    // Stop and remove a backend identified by host and port.
    //
    // Example ID:
    //
    //     "127.0.0.1:51000"
    //
    // Returns:
    //
    //     true  -> server was found and removed
    //     false -> server not found or minimum limit was reached
    bool removeServerById(const std::string& id);


    // ---------------------------------------------------------------
    // Server queries
    // ---------------------------------------------------------------

    // Return a copy of the complete shared-pointer collection.
    //
    // Copying shared_ptr does not copy the actual Server objects.
    // It creates additional references to the same objects.
    std::vector<std::shared_ptr<Server>> getAllServers();


    // Return servers that are eligible to receive requests.
    //
    // A backend is active when:
    //
    // - It is marked healthy.
    // - It has an associated process.
    // - Its process is still running.
    std::vector<std::shared_ptr<Server>> getActiveServers();


    // Find a backend using its unique ID.
    //
    // Returns nullptr when no matching server exists.
    std::shared_ptr<Server> findServerById(
        const std::string& id
    );


    // ---------------------------------------------------------------
    // Existing health-monitoring compatibility
    // ---------------------------------------------------------------

    // Store a health and resource update for one server.
    //
    // Round-robin and manual server management do not require the CPU
    // and memory values, but the existing health checker calls this.
    void updateServerHealth(
        const std::string& id,
        bool healthy,
        double cpu_usage,
        double memory_usage
    );


    // Summary returned to the existing admin and autoscaling APIs.
    struct ServerStats {
        std::size_t total_servers;
        std::size_t active_servers;
        std::size_t inactive_servers;
        std::size_t min_servers;
        std::size_t max_servers;
    };


    // Return the current server counts and configured limits.
    ServerStats getServerStats();


private:
    // Add a server while the caller already holds mutex_.
    //
    // We use a separate unlocked helper because the constructor and
    // public addServer() need the same creation logic.
    std::shared_ptr<Server> addServerUnlocked();


    // Find a server while the caller already holds mutex_.
    //
    // Calling the public findServerById() from another locked function
    // would try to lock the same non-recursive mutex twice and deadlock.
    std::shared_ptr<Server> findServerByIdUnlocked(
        const std::string& id
    );


    // Full path or relative path of the backend executable.
    //
    // Example:
    //
    //     "./backend_server"
    std::string executable_path_;


    // Port that will be assigned to the next backend.
    //
    // After adding a server, this value is incremented.
    int next_port_;


    // Lowest number of active backends allowed.
    std::size_t min_servers_;


    // Highest number of active backends allowed.
    std::size_t max_servers_;


    // Number of currently active backend servers.
    //
    // Explicit initialization is important. The original project used
    // an atomic counter without reliably initializing it.
    //
    // Every read and write occurs while mutex_ is held, so this does
    // not need to be atomic.
    std::size_t active_servers_{0};


    // All backend Server objects currently managed by this class.
    std::vector<std::shared_ptr<Server>> servers_;


    // Protects:
    //
    // - servers_
    // - active_servers_
    // - next_port_
    //
    // gRPC can invoke admin and load-balancing methods concurrently,
    // so these shared values must not be accessed without locking.
    mutable std::mutex mutex_;
};