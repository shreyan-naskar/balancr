#pragma once

// Defines pid_t, the data type used for Unix process IDs.
#include <sys/types.h>

#include "core/process/process.hpp"


// Unix implementation of the Process interface.
//
// Although this class is named LinuxProcess, the process-management
// functions used by it are POSIX functions.
//
// It will use:
//
//     fork()     -> create a child process
//     execl()    -> replace the child with the backend executable
//     waitpid()  -> inspect or wait for the child process
//     kill()     -> send a termination signal
class LinuxProcess final : public Process {
public:
  // Construct an object that does not yet manage any process.
  //
  // pid_ will initially be -1.
  LinuxProcess();


  // Clean up the managed backend process when this object is destroyed.
  //
  // "override" confirms that this destructor overrides the virtual
  // destructor declared by Process.
  ~LinuxProcess() override;


  // Start a child process using a command such as:
  //
  //     "./backend_server 51000"
  //
  // Return true if fork() successfully creates the child.
  //
  // Note: a true return means the child was created. The executable
  // could still fail inside the child—for example, if the path does
  // not exist. We can improve that detection later if necessary.
  bool start(const std::string& command) override;


  // Determine whether the child process is still running.
  //
  // This check must not block while the process is running.
  bool isRunning() override;


  // Send a termination signal to the child and wait for it to exit.
  void terminate() override;


  // Return the child process's exit code when available.
  //
  // Return -1 if:
  //
  // - No process was started.
  // - The process is still running.
  // - The process ended because of a signal.
  int getExitCode() override;


  // Return CPU usage for the managed process.
  //
  // We will initially return 0.0 because resource monitoring is
  // outside our two current features.
  double getCPUUsage() override;


  // Return memory usage for the managed process.
  //
  // We will initially return 0.0 because resource monitoring is
  // outside our two current features.
  double getMemoryUsage() override;

private:
  // Operating-system process identifier of the backend child.
  //
  // Values used by this class:
  //
  //     pid_ > 0  -> a child process was created
  //     pid_ == -1 -> this object is not managing a process
  //
  // A real child PID is always positive.
  pid_t pid_;
};