#pragma once
#include "value.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

// The options of a port and the work its task does (docs/ports.md#port-tasks): input the I/O thread read, framed by
// the port's options on a scheduler worker, and events drivers report.
namespace clause::runtime::detail {
class PortDriver;

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

// One unit of port input: raw bytes as read (data, before framing) or after framing (a stream chunk, a packet or a
// whole line), a line part (eol, noeol), the end of input, a read or write error with its POSIX reason, or a
// spawned program's exit status.
struct PortInput final {
    enum class Kind : std::uint8_t { data, eol, noeol, end, error, status };
    Kind kind = Kind::data;
    std::vector<std::byte> bytes;
    std::string reason;
    std::int64_t status = 0;

    // Raw input bytes.
    static PortInput raw(std::span<const std::byte> bytes) {
        return {.kind = Kind::data, .bytes = {bytes.begin(), bytes.end()}, .reason = {}, .status = 0};
    }

    // A unit without bytes: the end of input, or an error with its reason.
    static PortInput of(Kind kind, std::string reason = {}) {
        return {.kind = kind, .bytes = {}, .reason = std::move(reason), .status = 0};
    }
};

// Splits a port's input into the units its framing asks for ({packet, N}, {line, L} or stream).
class InputDecoder final {
  public:
    explicit InputDecoder(const PortOptions &options) noexcept
        : framing_(options.framing), packet_bytes_(options.packet_bytes), line_length_(options.line_length) {}

    // The complete units in `bytes` read after earlier input; the rest waits for more.
    std::vector<PortInput> feed(std::span<const std::byte> bytes);
    // The units at end of input: an unterminated line as noeol, then end.
    std::vector<PortInput> finish();

  private:
    // Append the packets complete in pending_.
    void packets(std::vector<PortInput> &units);
    // Append the lines complete in pending_, and parts of lines longer than the line length.
    void lines(std::vector<PortInput> &units);

    Framing framing_;
    std::size_t packet_bytes_;
    std::size_t line_length_;
    // Bytes read but not yet a complete unit.
    std::vector<std::byte> pending_;
};

// Something a socket reports from the I/O thread.
struct SocketEvent final {
    enum class Kind : std::uint8_t { message, accepted };
    Kind kind = Kind::message;
    // The process the message goes to; 0 for the port's connected process.
    Word target = 0;
    // The message ({tcp, S, Data}, {clause_socket, S, Reply}, ...); for accepted, the reply without the new
    // socket, which the executor adds once it has a port.
    PortValue value;
    // A connection a listening socket accepted for `target`, to become a new port it is connected to.
    std::shared_ptr<PortDriver> driver;
};

// One item of a port's task: input the I/O thread handed over, or a socket's event.
using PortWork = std::variant<PortInput, SocketEvent>;
} // namespace clause::runtime::detail
