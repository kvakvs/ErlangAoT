#pragma once
#include "port.hpp"
#include "reactor.hpp"
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

// The port I/O of a runtime (docs/ports.md#io-thread): reads the input of ports and hands it, as read, to the
// executor, writes queued port output and reports the exit status of spawned programs, all on the reactor's thread.
namespace clause::runtime::detail {
// Blocking whole writes to a C runtime descriptor, as the fd driver writes its output.
std::expected<void, DriverError> write_all(int fd, std::span<const std::byte> bytes);

// A spawned program the I/O service waits for: a process handle on Windows, a pid elsewhere.
struct Child final {
    std::int64_t handle = 0;
};

// Lets the I/O thread (and Windows blocking readers) hand input to the executor until the service stops; stopping
// waits for a delivery in progress. Readers share it, so it outlives a service whose readers were let go.
struct IoGate final {
    // Receives the input of a port: raw bytes, the end, an error or a status, and the bytes read.
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

    // Read `input` for `port`; an owned handle is closed when reading ends.
    void read_descriptor(Word port, Descriptor input);
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
