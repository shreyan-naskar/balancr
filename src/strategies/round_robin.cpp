#include "strategies/round_robin.hpp"


// Select the next active backend using round-robin order.
std::shared_ptr<Server>
RoundRobinStrategy::selectServer(
    const std::vector<std::shared_ptr<Server>>& servers,
    const loadbalancer::Request& request
) {
    // Round-robin does not need request data.
    //
    // The parameter remains part of the common Strategy interface so
    // other algorithms can inspect it in the future.
    (void)request;


    // No server can be selected from an empty list.
    //
    // This happens when:
    //
    // - No backend was started successfully.
    // - Every backend is unhealthy.
    // - Every backend process has stopped.
    if (servers.empty()) {
        return nullptr;
    }


    // Atomically obtain the current position, then increase it by one.
    //
    // fetch_add(1) returns the old value before incrementing.
    //
    // Example:
    //
    //     next_index_ is 2
    //
    // After this line:
    //
    //     index       = 2
    //     next_index_ = 3
    //
    // Atomic access ensures simultaneous client requests do not receive
    // the same round-robin position.
    const std::size_t index =
        next_index_.fetch_add(1);


    // Convert the continuously increasing index into a valid vector
    // position using modulo arithmetic.
    //
    // Example with 3 servers:
    //
    //     index = 0 -> 0 % 3 = 0
    //     index = 1 -> 1 % 3 = 1
    //     index = 2 -> 2 % 3 = 2
    //     index = 3 -> 3 % 3 = 0
    //     index = 4 -> 4 % 3 = 1
    const std::size_t selected_index =
        index % servers.size();


    // Return a shared_ptr to the selected backend.
    //
    // The caller can safely use this Server even after the local
    // `servers` vector is destroyed.
    return servers[selected_index];
}