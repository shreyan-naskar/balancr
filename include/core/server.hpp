#pragma once

#include<atomic>
#include<chrono>
#include<memory>
#include<string>

#include"core/process/process.hpp"

using namespace std;

class Server {

public:
  Server (const string &host, int port);

  string getAddress() const;

  int getPort() const;

  string getId() const;

  bool isHealthy() const;

  void setHealthStatus(bool status);

  double getCPUUsage() const;

  void setCPUUsage(double usage);

  double getMemoryUsage() const;

  void setMemoryUsage(double usage);

  chrono::system_clock::time_point getLastHealthCheckTime() const;

  void setLastHealthCheckTime(chrono::system_clock::time_point time);

  int getRequestCount() const {
    return request_count_.load();
  }

  void incrementRequestCount() {
    request_count_.fetch_add(1);
  }

  void incrementActiveConnections();

  void decrementActiveConnections();

  int getActiveConnections() const;

  void setProcess (unique_ptr<Process> process) {
    process_ = move(process);
  }

  Process* getProcess() const {
    return process_.get();
  }

private:
  string host_;

  int port_;

  atomic<bool> is_healthy_{true};

  string id_;

  chrono::system_clock::time_point last_health_check_time_;

  atomic<int> request_count_{0};

  atomic<int> active_connections_{0};

  unique_ptr<Process> process_;

  double cpu_usage_{0.0};

  double memory_usage_{0.0};
};