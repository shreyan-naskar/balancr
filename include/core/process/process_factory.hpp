#pragma once

// Provides std::unique_ptr.
#include <memory>

// Provides the common Process interface returned by the factory.
#include "core/process/process.hpp"


// Creates the correct OS-specific Process implementation.
//
// ServerManager should not directly construct LinuxProcess or
// WindowsProcess. It asks this factory for a generic Process:
//
//     auto process = ProcessFactory::createProcess();
//
// The factory decides which concrete class should be created:
//
//     Windows       -> WindowsProcess
//     Linux/macOS   -> LinuxProcess
class ProcessFactory {
public:
  // Create and return an OS-specific process-management object.
  //
  // The return type is unique_ptr<Process>, even though the actual
  // object will be either LinuxProcess or WindowsProcess.
  //
  // Returning unique_ptr clearly transfers ownership to the caller.
  //
  // Example:
  //
  //     std::unique_ptr<Process> process =
  //         ProcessFactory::createProcess();
  //
  //     process->start("./backend_server 51000");
  static std::unique_ptr<Process> createProcess();
};