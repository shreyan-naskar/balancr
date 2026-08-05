#pragma once

// Used for the command passed to start().
#include <string>


// Abstract interface for managing one operating-system process.
//
// Process does not contain platform-specific implementation.
// Instead, subclasses provide it:
//
//     Process
//       ├── LinuxProcess
//       └── WindowsProcess
//
// Server and ServerManager can work with Process without knowing
// which operating system implementation is being used.
class Process {
public:
  // Virtual destructor is essential because objects are deleted
  // through a pointer to this base class.
  //
  // For example:
  //
  //     std::unique_ptr<Process> process =
  //         std::make_unique<LinuxProcess>();
  //
  // When process is destroyed, the virtual destructor ensures that
  // LinuxProcess::~LinuxProcess() runs before Process::~Process().
  virtual ~Process() = default;


  // Start a new operating-system process using the supplied command.
  //
  // Example command:
  //
  //     "./backend_server 51000"
  //
  // Return:
  //     true  -> process started successfully
  //     false -> process could not be started
  virtual bool start(const std::string& command) = 0;


  // Check whether the backend process is still running.
  //
  // Return:
  //     true  -> process is currently running
  //     false -> process stopped, crashed, or never started
  virtual bool isRunning() = 0;


  // Ask the operating system to stop the backend process.
  //
  // ServerManager calls this when an administrator removes a server.
  virtual void terminate() = 0;


  // Return the process's exit code.
  //
  // A negative value, such as -1, can mean:
  // - The process is still running.
  // - The exit code is unavailable.
  // - No process was started.
  virtual int getExitCode() = 0;


  // Return the process's CPU usage percentage.
  //
  // This method belongs to the existing monitoring feature.
  // Basic server management and round-robin balancing do not require it.
  virtual double getCPUUsage() = 0;


  // Return the process's memory usage.
  //
  // The exact unit must be defined by the platform implementation.
  // The existing Windows implementation reports megabytes.
  virtual double getMemoryUsage() = 0;
};