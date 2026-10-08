#pragma once
#include <cstddef>
#include <cstdint>
#include <erlang_aot/runtime/terms.hpp>
#include <expected>
#include <memory>
#include <optional>
#include <signals.hpp>
#include <span>
#include <string>
#include <utility>
#include <vector>

// Ports (docs/ports.md): the port record the executor keeps in its port table, the options open_port/2 accepts and
// the interface of the drivers behind ports.
namespace erlang_aot::runtime::detail {
// How a port's data is framed, in both directions.
enum class Framing : std::uint8_t { stream, packet, line };

// The options of open_port/2 (docs/ports.md#data-modes-and-options).
struct PortOptions final {
    Framing framing = Framing::stream;
    // Header bytes of {packet, N} (1, 2 or 4), or the line length L of {line, L}.
    std::size_t packet_bytes = 0;
    std::size_t line_length = 0;
    // Data as binaries instead of byte lists.
    bool binary = false;
    // Send {Port, eof} at end of input and stay open, instead of closing.
    bool eof = false;
    // Send {Port, {exit_status, S}} when a spawned program exits.
    bool exit_status = false;
    // The directions the port is opened for (options in and out).
    bool input = true;
    bool output = true;
    // Spawned programs: use stdin/stdout (use_stdio), merge stderr into stdout.
    bool use_stdio = true;
    bool stderr_to_stdout = false;
    // Spawned programs: arguments, argv[0], environment changes (unset when the value is missing), directory.
    std::vector<std::string> args;
    std::optional<std::string> arg0;
    std::vector<std::pair<std::string, std::optional<std::string>>> env;
    std::optional<std::string> cd;
};

// A failed driver operation: the POSIX error name a port closes with or an operation reports (epipe, enoent, ...).
struct DriverError final {
    std::string reason;
};

// The resource behind one port. The executor calls a driver under its mutex; a driver never calls back into it.
class PortDriver {
  public:
    PortDriver() = default;
    PortDriver(const PortDriver &) = delete;
    PortDriver &operator=(const PortDriver &) = delete;
    PortDriver(PortDriver &&) = delete;
    PortDriver &operator=(PortDriver &&) = delete;
    // Release the resource; a closed port's driver is destroyed at once.
    virtual ~PortDriver() = default;
    // Write one output of the port (port_command, {command, Data}), framing already applied.
    virtual std::expected<void, DriverError> write(std::span<const std::byte> bytes) = 0;

    // The operating system process id of a spawned program, for port_info's os_pid.
    virtual std::optional<std::int64_t> os_pid() const noexcept { return std::nullopt; }

    // Whether port_control/3 works on this driver; others make it badarg.
    virtual bool controllable() const noexcept { return false; }
};

// One open port of the executor's port table.
struct Port final {
    // The port's number (its word is port_word(number)).
    Word number = 0;
    // The connected process: its opener, or the process it was connected to since.
    Word connected = 0;
    // Pid words of linked processes, oldest link first.
    std::vector<Word> links;
    // Monitors held on the port: reference to the monitoring process.
    Signals::Monitors watchers;
    // Registered name (an atom word), or 0.
    Word name = 0;
    // Name reported by port_info/2: the command, file or "In/Out" of the port.
    std::string spelling;
    PortOptions options;
    // Bytes read from and written to the port.
    std::size_t input = 0;
    std::size_t output = 0;
    std::unique_ptr<PortDriver> driver;
};

// Apply the output framing of `options` to `bytes`: a {packet, N} header; nothing for stream and line ports.
// Empty when the data is too long for the header.
std::optional<std::vector<std::byte>> framed(const PortOptions &options, std::vector<std::byte> bytes);

// The driver of an {fd, In, Out} port: output written at once to Out (docs/ports.md#drivers).
std::unique_ptr<PortDriver> fd_driver(int in, int out);
} // namespace erlang_aot::runtime::detail
