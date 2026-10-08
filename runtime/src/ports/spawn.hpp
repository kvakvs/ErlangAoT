#pragma once
#include "port.hpp"

// Starting a spawned program for a port (docs/ports.md#drivers); the platform part lives in spawn_windows.cpp and
// spawn_posix.cpp.
namespace clause::runtime::detail {
// A started program: the descriptor its stdout is read from and the one its stdin is written to (-1 when the port
// is opened only out or only in), the handle the I/O thread waits on (a process handle on Windows, a pid
// elsewhere) and its operating system pid.
struct Spawned final {
    int input = -1;
    int output = -1;
    std::int64_t child = 0;
    std::int64_t os_pid = 0;
};

// Start the program of `request` with its stdin, stdout and (with stderr_to_stdout) stderr connected to new pipes,
// and the arguments, environment and directory of `options`.
std::expected<Spawned, DriverError> spawn_program(const SpawnRequest &request, const PortOptions &options);
} // namespace clause::runtime::detail
