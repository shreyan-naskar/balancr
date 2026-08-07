#include "core/server_manager.hpp"
#include "core/process/process_factory.hpp"

#include<stdexcept>
#include<algorithm>

// Create the manager responsible for backend servers.
//
// Example:
//
//     ServerManager manager(
//         "./backend_server",
//         51000,
//         2,
//         5
//     );
//
// This will attempt to start two initial backends:
//
//     ./backend_server 51000
//     ./backend_server 51001
ServerManager::ServerManager(
    const std::string& executable_path,
    int start_port,
    std::size_t min_servers,
    std::size_t max_servers
)
    // Store the path of the backend executable that addServer() will run.
    : executable_path_(executable_path),

      // The first server created receives start_port.
      //
      // Each successful server creation increments next_port_.
      next_port_(start_port),

      // Store the configured limits.
      min_servers_(min_servers),
      max_servers_(max_servers),

      // Begin with zero active backend servers.
      active_servers_(0) {

    // A configuration such as:
    //
    //     min_servers = 5
    //     max_servers = 2
    //
    // is impossible to satisfy, so reject it immediately.
    if (min_servers_ > max_servers_) {
        throw std::invalid_argument(
            "min_servers cannot be greater than max_servers"
        );
    }

    // The manager needs an executable path to start backend processes.
    //
    // An empty path would build an invalid command such as:
    //
    //     " 51000"
    if (executable_path_.empty()) {
        throw std::invalid_argument(
            "backend executable path cannot be empty"
        );
    }

    // Start the initial backend pool.
    //
    // If min_servers_ is 3, the loop attempts:
    //
    //     addServer() -> backend on start_port
    //     addServer() -> backend on start_port + 1
    //     addServer() -> backend on start_port + 2
    //
    // addServer() is declared already and will be implemented next.
    for (std::size_t i = 0; i < min_servers_; ++i) {
        auto server = addServer();

        // If starting a backend fails, stop trying.
        //
        // We do not continue because a partial startup is usually a
        // configuration problem, such as an invalid backend path or a
        // port conflict.
        //
        // The application can later decide whether to treat this as a
        // fatal error or expose the partially created server pool.
        if (server == nullptr) {
            break;
        }
    }
}

// Public, thread-safe entry point for adding a backend server.
//
// AdminService will call this when an administrator requests a new
// server. The constructor also calls it to create the initial pool.
std::shared_ptr<Server> ServerManager::addServer() {
    // Only one thread may create a server at a time.
    //
    // This protects:
    //
    // - active_servers_
    // - next_port_
    // - servers_
    std::lock_guard<std::mutex> lock(mutex_);

    // The actual creation work happens in the helper because the lock
    // is already held here.
    return addServerUnlocked();
}


// Create and register a new backend while mutex_ is already locked.
//
// This private helper must not be called without first locking mutex_.
std::shared_ptr<Server> ServerManager::addServerUnlocked() {
    // Do not create more backends than the configured limit.
    //
    // Example:
    //
    //     active_servers_ = 5
    //     max_servers_    = 5
    //
    // Then adding another backend is not allowed.
    if (active_servers_ >= max_servers_) {
        return nullptr;
    }


    // Save the current value before incrementing it.
    //
    // Example:
    //
    //     next_port_ = 51002
    //
    // The new server uses port 51002. Only after successful creation
    // will next_port_ become 51003.
    const int server_port = next_port_;


    // This project starts backend servers on the local machine.
    //
    // In a distributed version, host could be supplied by configuration,
    // service discovery, Docker, Kubernetes, or another machine.
    const std::string server_host = "127.0.0.1";


    // Ask the factory for the correct platform-specific process object.
    //
    // On macOS/Linux:
    //
    //     std::unique_ptr<LinuxProcess>
    //
    // On Windows:
    //
    //     std::unique_ptr<WindowsProcess>
    //
    // The returned pointer is stored through the common Process type.
    auto process = ProcessFactory::createProcess();


    // Build the command executed by the process object.
    //
    // Example:
    //
    //     executable_path_ = "./backend_server"
    //     server_port      = 51002
    //
    // Result:
    //
    //     "./backend_server" 51002
    //
    // Quoting the path lets executable paths containing spaces work,
    // for example:
    //
    //     "/Users/name/My Project/backend_server" 51002
    const std::string command =
        "\"" + executable_path_ + "\" " +
        std::to_string(server_port);


    // Start the backend process.
    //
    // On Unix, this performs fork() and execl().
    // On Windows, this performs CreateProcessW().
    //
    // If process creation fails, do not modify the manager's state.
    if (!process->start(command)) {
        return nullptr;
    }


    // Create the in-memory representation of the backend.
    //
    // At this point, the backend has:
    //
    //     host = 127.0.0.1
    //     port = server_port
    //     id   = 127.0.0.1:<server_port>
    auto server = std::make_shared<Server>(
        server_host,
        server_port
    );


    // Transfer exclusive ownership of the OS process object to Server.
    //
    // Before this line:
    //
    //     local process variable owns Process
    //
    // After this line:
    //
    //     server->process_ owns Process
    // process is moved and becomes nullptr.
    server->setProcess(std::move(process));


    // Store a shared reference to this Server in the manager's collection.
    servers_.push_back(server);


    // Update manager state only after successful process and Server
    // creation.
    ++active_servers_;
    ++next_port_;


    // Return a shared reference to the newly created server.
    //
    // AdminService uses this to return the new server ID to the caller.
    return server;
}

// Stop and remove a backend server identified by its ID.
//
// Example ID:
//
//     "127.0.0.1:51001"
//
// Returns true only when a server was actually removed.
bool ServerManager::removeServerById(
    const std::string& id
) {
    // Prevent another thread from adding, removing, or listing in the
    // middle of this operation.
    std::lock_guard<std::mutex> lock(mutex_);


    // Never allow removal below the configured minimum.
    //
    // Example:
    //
    //     active_servers_ = 2
    //     min_servers_    = 2
    //
    // Removing one would leave only one backend, so reject the request.
    if (active_servers_ <= min_servers_) {
        return false;
    }


    // Find the matching Server object in the collection.
    //
    // std::find_if returns:
    //
    // - an iterator pointing at the matching server, or
    // - servers_.end() when no server has this ID.
    auto iterator = std::find_if(
        servers_.begin(),
        servers_.end(),
        [&id](const std::shared_ptr<Server>& server) {
            // Compare the requested ID with each stored server ID.
            return server->getId() == id;
        }
    );


    // The requested ID does not belong to a managed backend.
    if (iterator == servers_.end()) {
        return false;
    }


    // Dereference the vector iterator to obtain the shared pointer.
    auto server = *iterator;


    // Stop the operating-system process if the Server owns one.
    //
    // getProcess() returns a non-owning raw pointer, so we only use it
    // to call terminate(). We do not delete it manually.
    Process* process = server->getProcess();

    if (process != nullptr) {
        process->terminate();
    }


    // Mark the in-memory object unhealthy before removing it.
    //
    // This helps any request thread that already holds another
    // shared_ptr reference to the same Server object.
    server->setHealthStatus(false);


    // Remove the manager's shared_ptr from the vector.
    //
    // This does not necessarily destroy the Server immediately:
    //
    // - The load balancer may still hold a shared_ptr while completing
    //   an already-started request.
    // - When the last shared_ptr disappears, Server is destroyed.
    servers_.erase(iterator);


    // Keep the active-server count consistent with servers_.
    --active_servers_;


    return true;
}

// Return a copy of all backend server references.
//
// The vector itself is copied, but Server objects are not copied.
// Every shared_ptr in the returned vector still refers to the same
// Server object stored by ServerManager.
std::vector<std::shared_ptr<Server>>
ServerManager::getAllServers() {
    // Lock while copying servers_ so another thread cannot modify the
    // vector during the copy.
    std::lock_guard<std::mutex> lock(mutex_);

    return servers_;
}


// Public, thread-safe lookup for a server ID.
//
// Example:
//
//     auto server = server_manager->findServerById(
//         "127.0.0.1:51001"
//     );
std::shared_ptr<Server>
ServerManager::findServerById(
    const std::string& id
) {
    // Public callers do not know whether the manager is already locked,
    // so this public method acquires the lock itself.
    std::lock_guard<std::mutex> lock(mutex_);

    return findServerByIdUnlocked(id);
}


// Private lookup helper.
//
// Important: the caller must already hold mutex_.
//
// This helper exists so other locked methods, such as
// updateServerHealth(), can find a server without attempting to lock
// the same std::mutex twice.
std::shared_ptr<Server>
ServerManager::findServerByIdUnlocked(
    const std::string& id
) {
    // Check every managed backend server.
    for (const auto& server : servers_) {
        if (server->getId() == id) {
            // Return a shared reference to the matching server.
            return server;
        }
    }

    // No server matches this ID.
    return nullptr;
}


// Return only servers that can receive new requests.
//
// LoadBalancerService will call this before selecting a backend:
//
//     auto servers = server_manager->getActiveServers();
//
// A server is considered active only when:
//
// 1. It is marked healthy.
// 2. It has an associated process object.
// 3. That backend process is still running.
std::vector<std::shared_ptr<Server>> ServerManager::getActiveServers() {
    // Protect servers_ while iterating through it.
    std::lock_guard<std::mutex> lock(mutex_);


    // This vector will contain only usable request destinations.
    std::vector<std::shared_ptr<Server>> active_servers;


    // Examine each managed backend.
    for (const auto& server : servers_) {
        // First verify its logical health flag.
        //
        // A health checker can mark a server unhealthy after failed
        // network checks, even when its process still exists.
        if (!server->isHealthy()) {
            continue;
        }


        // A server without a Process cannot be managed or verified.
        Process* process = server->getProcess();

        if (process == nullptr) {
            continue;
        }


        // Check whether the operating-system process is still alive.
        //
        // If a backend unexpectedly exits or crashes, isRunning()
        // returns false. Mark it unhealthy so later requests also skip it.
        if (!process->isRunning()) {
            server->setHealthStatus(false);
            continue;
        }


        // This backend is healthy and running, so it may receive
        // incoming requests.
        active_servers.push_back(server);
    }


    return active_servers;
}

// Update one server's health and latest resource measurements.
//
// This is normally called by the external health checker.
//
// Example:
//
//     updateServerHealth(
//         "127.0.0.1:51000",
//         true,
//         35.5,
//         120.0
//     );
void ServerManager::updateServerHealth(
    const std::string& id,
    bool healthy,
    double cpu_usage,
    double memory_usage
) {
    // We modify both manager state and one Server object, so lock the
    // collection before finding and updating the server.
    std::lock_guard<std::mutex> lock(mutex_);


    // Use the unlocked helper because mutex_ is already locked.
    auto server = findServerByIdUnlocked(id);

    // Ignore updates for servers no longer managed by this load balancer.
    if (server == nullptr) {
        return;
    }


    // Remember the previous state before applying the new one.
    const bool was_healthy = server->isHealthy();


    // Keep active_servers_ aligned with health state.
    //
    // Healthy -> unhealthy:
    //
    //     active_servers_ decreases by one.
    //
    // Unhealthy -> healthy:
    //
    //     active_servers_ increases by one.
    //
    // This protects against decrementing or incrementing repeatedly
    // when the reported state has not actually changed.
    if (was_healthy && !healthy) {
        if (active_servers_ > 0) {
            --active_servers_;
        }
    } else if (!was_healthy && healthy) {
        if (active_servers_ < max_servers_) {
            ++active_servers_;
        }
    }


    // Store the current health state and resource measurements.
    server->setHealthStatus(healthy);
    server->setCPUUsage(cpu_usage);
    server->setMemoryUsage(memory_usage);
}


// Return a snapshot of current server counts and configured limits.
ServerManager::ServerStats ServerManager::getServerStats() {
    // Protect the values while creating the snapshot.
    std::lock_guard<std::mutex> lock(mutex_);


    ServerStats stats;

    // Number of Server objects currently stored by ServerManager.
    stats.total_servers = servers_.size();

    // Number currently considered active.
    stats.active_servers = active_servers_;

    // Servers that are known but not currently active.
    //
    // Example:
    //
    //     total_servers  = 5
    //     active_servers = 3
    //
    // Then inactive_servers is 2.
    stats.inactive_servers =
        stats.total_servers - stats.active_servers;

    // Return the configured constraints as part of the snapshot.
    stats.min_servers = min_servers_;
    stats.max_servers = max_servers_;

    return stats;
}

