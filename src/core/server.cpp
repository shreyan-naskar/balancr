#include "core/server.hpp"

Server::Server(const string &host, int port):
  host_(host),
  port_(port),
  is_healthy_(true),
  id_(host + ":" + to_string(port)),
  last_health_check_time_(chrono::system_clock::now()),
  request_count_(0),
  active_connections_(0),
  process_(nullptr),
  cpu_usage_(0.0),
  memory_usage_(0.0){}

string Server::getAddress() const {return host_; }

int Server::getPort() const {return port_;}

string Server::getId() const {return id_;}

bool Server::isHealthy() const {return is_healthy_.load();}

void Server::setHealthStatus(bool status) {
  is_healthy_.store(status);
  last_health_check_time_ = chrono::system_clock::now();
}

double Server::getCPUUsage() const {return cpu_usage_;}

void Server::setCPUUsage(double usage) {cpu_usage_ = usage;}

double Server::getMemoryUsage() const {return memory_usage_;}

void Server::setCPUUsage(double usage) {memory_usage_ = usage;}

chrono::system_clock::time_point Server::getLastHealthCheckTime() const {return last_health_check_time_;}

void Server::setLastHealthCheckTime(chrono::system_clock::time_point time) {
  last_health_check_time_ = time;
}

void Server::incrementActiveConnections() {
  active_connections_.fetch_add(
    1,
    memory_order_relaxed
  );
}

void Server::decrementActiveConnections() {
  active_connections_.fetch_sub(
    1,
    memory_order_relaxed
  );
}