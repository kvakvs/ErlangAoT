#pragma once
#include "port.hpp"
#include "reactor.hpp"
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

// The port I/O of a runtime (docs/ports.md#io-thread): reads the input of ports and hands it, framed, to the
// executor, writes queued port output and reports the exit status of spawned programs, all on the reactor's thread.
namespace clause::runtime::detail {
// One unit of port input after framing: data (a stream chunk, a packet or a whole line), a line part (eol, noeol),
// the end of input, a read or write error with its POSIX reason, or a spawned program's exit status.
struct PortInput final {
    enum class Kind : std::uint8_t { data, eol, noeol, end, error, status };
    Kind kind = Kind::data;
    std::vector<std::byte> bytes;
    std::string reason;
    std::int64_t status = 0;
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

// Blocking whole writes to a C runtime descriptor, as the fd driver writes its output.
std::expected<void, DriverError> write_all(int fd, std::span<const std::byte> bytes);

// A spawned program the I/O service waits for: a process handle on Windows, a pid elsewhere.
struct Child final {
    std::int64_t handle = 0;
};

// Lets the I/O thread (and Windows blocking readers) hand input to the executor until the service stops; stopping
// waits for a delivery in progress. Readers share it, so it outlives a service whose readers were let go.
struct IoGate final {
    // Receives the input of a port: the units completed and the bytes read.
    using Deliver = std::function<void(Word port, std::vector<PortInput> units, std::size_t read)>;

    // Hand units over unless the service stopped.
    void deliver(Word port, std::vector<PortInput> units, std::size_t read);
    // Deliver nothing more; waits for a delivery in progress.
    void close();

    std::mutex mutex;
    bool open = true;
    Deliver receiver;
};

// The port I/O of the reactor: every method only posts work to the reactor's thread, so the executor may call it
// under its lock; all I/O state lives on that thread.
class IoService final {
  public:
    using Deliver = IoGate::Deliver;

    IoService(Reactor &reactor, Deliver deliver);
    IoService(const IoService &) = delete;
    IoService &operator=(const IoService &) = delete;
    IoService(IoService &&) = delete;
    IoService &operator=(IoService &&) = delete;
    // Stop delivering. The reactor must have stopped: the remaining I/O objects are destroyed here.
    ~IoService();

    // Read `input` for `port`, framed as `options` says; an owned handle is closed when reading ends.
    void read_descriptor(Word port, Descriptor input, const PortOptions &options);
    // Write the port's queued output to `output`.
    void write_descriptor(Word port, Descriptor output);
    // Queue output of `port`.
    void send(Word port, std::vector<std::byte> bytes);
    // Report the exit status of a spawned program of `port` when it exits.
    void watch_child(Word port, Child child);
    // Stop delivering input and errors of `port`; its queued output is still written, then its handle closed, and
    // its program is still reaped.
    void forget(Word port);
    // Deliver nothing more (before the reactor stops); waits for a delivery in progress.
    void close();

    class Impl;

  private:
    // The state the reactor's thread works on; handlers reach it only while the reactor runs.
    std::unique_ptr<Impl> impl_;
};
} // namespace clause::runtime::detail
