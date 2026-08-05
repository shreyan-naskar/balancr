#include "core/process/linux_process.hpp"

// errno allows us to detect why an operating-system call failed.
#include <cerrno>

// Provides kill() and SIGTERM.
#include <csignal>

// Provides waitpid() and process-status macros such as WIFEXITED.
#include <sys/wait.h>

// Provides fork(), execl(), and _exit().
#include <unistd.h>


// Construct an object that does not currently manage a process.
//
// Unix child process IDs are positive, so -1 is a convenient value
// meaning "no child process".
LinuxProcess::LinuxProcess()
  : pid_(-1) {
}


// Ensure the backend process is cleaned up when the process-management
// object is destroyed.
//
// terminate() first checks pid_, so it is safe to call even when:
// - start() was never called.
// - process creation failed.
// - terminate() was already called.
LinuxProcess::~LinuxProcess() {
  terminate();
}


// Start a backend process.
//
// Example command:
//
//     "./backend_server 51000"
//
// fork() duplicates the current process. After fork(), execution
// continues in two different processes:
//
//     Parent: the load balancer
//     Child:  will become the backend server
bool LinuxProcess::start(const std::string& command) {
  // Do not start another child if this object already manages one.
  if (pid_ > 0) {
    return false;
  }

  // fork() creates the new child process.
  //
  // Return values:
  //     < 0 -> creation failed
  //       0 -> currently executing inside the child
  //     > 0 -> currently executing inside the parent; value is child PID
  pid_ = fork();


  // Process creation failed.
  if (pid_ < 0) {
    pid_ = -1;
    return false;
  }


  // This branch executes only inside the new child process.
  if (pid_ == 0) {
    // Replace the child process with a shell that runs command.
    //
    // For:
    //
    //     command = "./backend_server 51000"
    //
    // this is equivalent to:
    //
    //     /bin/sh -c "./backend_server 51000"
    //
    // Arguments:
    //     "/bin/sh" -> program to execute
    //     "sh"      -> argv[0], program name
    //     "-c"      -> tell the shell to execute a command string
    //     command   -> command string
    //     nullptr   -> end of argument list
    execl(
      "/bin/sh",
      "sh",
      "-c",
      command.c_str(),
      static_cast<char*>(nullptr)
    );


    // execl() only returns when execution fails.
    //
    // _exit(127) immediately ends the child process. Exit code 127
    // traditionally indicates that the requested command could not
    // be executed or found.
    //
    // We use _exit() instead of exit() after fork() because _exit()
    // does not flush or clean up state copied from the parent.
    _exit(127);
  }


  // This line executes only in the parent load-balancer process.
  //
  // pid_ now contains the positive PID of the backend child.
  return true;
}


// Check whether the backend process is still running without blocking.
bool LinuxProcess::isRunning() {
  if (pid_ <= 0) {
    return false;
  }

  int status = 0;

  // WNOHANG tells waitpid() to return immediately if the child
  // is still running.
  //
  // Possible results:
  //       0 -> child is still running
  //     pid -> child has exited and its status was collected
  //      -1 -> an error occurred
  pid_t result = waitpid(pid_, &status, WNOHANG);


  if (result == 0) {
    // The child has not exited.
    return true;
  }


  if (result == pid_) {
    // The child exited. waitpid() also collected the child, so this
    // LinuxProcess object should no longer treat pid_ as active.
    pid_ = -1;
    return false;
  }


  // If the child was already collected elsewhere, waitpid() can return
  // -1 with ECHILD. Either way, the process is not manageable here.
  if (result == -1) {
    pid_ = -1;
  }

  return false;
}


// Stop the backend process and collect its process-table entry.
void LinuxProcess::terminate() {
  if (pid_ <= 0) {
    return;
  }


  // SIGTERM politely asks the process to terminate.
  //
  // It gives the backend an opportunity to perform normal shutdown
  // handling, unlike SIGKILL, which immediately forces termination.
  if (kill(pid_, SIGTERM) == -1) {
    // ESRCH means no process with this PID exists. The backend has
    // probably already exited.
    if (errno == ESRCH) {
      pid_ = -1;
      return;
    }
  }


  int status = 0;
  pid_t result;

  // Wait until the child exits.
  //
  // waitpid() can be interrupted by a signal, in which case it returns
  // -1 and sets errno to EINTR. We retry in that situation.
  do {
    result = waitpid(pid_, &status, 0);
  } while (result == -1 && errno == EINTR);


  // This object no longer manages a running process.
  pid_ = -1;
}


// Return the backend process's exit code if it has already exited.
int LinuxProcess::getExitCode() {
  if (pid_ <= 0) {
    return -1;
  }

  int status = 0;

  // Do not block if the process is still running.
  pid_t result = waitpid(pid_, &status, WNOHANG);


  if (result == 0) {
    // The backend is still running, so there is no exit code yet.
    return -1;
  }


  if (result == pid_) {
    // waitpid() collected the child, so this PID is no longer active.
    pid_ = -1;

    // WIFEXITED is true when the process ended normally by returning
    // from main() or calling exit().
    if (WIFEXITED(status)) {
      // Extract the actual numeric exit code.
      return WEXITSTATUS(status);
    }

    // The process may have ended because of a signal.
    return -1;
  }


  // waitpid() failed or the child was already collected.
  pid_ = -1;
  return -1;
}


// CPU monitoring is outside our current implementation.
//
// Returning zero satisfies the Process interface and keeps the existing
// monitoring-related code compilable.
double LinuxProcess::getCPUUsage() {
  return 0.0;
}


// Memory monitoring is outside our current implementation.
double LinuxProcess::getMemoryUsage() {
  return 0.0;
}