#pragma once
#include "port.hpp"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

// The I/O service of a runtime (docs/ports.md#io-thread): it reads the input of ports and hands it, framed, to the
// executor, writes queued port output, and reports the exit status of spawned programs.
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

// Platform primitives the service builds on: blocking whole writes, closing a descriptor, and waiting for a
// spawned program (a process handle on Windows, a pid elsewhere) to exit, reaping it; its status as OTP reports
// it (the exit code, or 128 plus the signal that ended it).
std::expected<void, DriverError> write_all(int fd, std::span<const std::byte> bytes);
void close_descriptor(int fd) noexcept;
std::int64_t wait_child(std::int64_t child) noexcept;

// A descriptor the I/O service reads or writes, and whether it closes it when done.
struct Descriptor final {
    int fd = -1;
    bool owned = false;
};

// A spawned program the I/O service waits for: a process handle on Windows, a pid elsewhere.
struct Child final {
    std::int64_t handle = 0;
};

// Lets I/O threads hand input to the executor until the service stops; stopping waits for a delivery in progress.
// I/O threads share it, so it outlives a service whose threads were let go.
struct IoGate final {
    // Receives the input of a port: the units completed and the bytes read.
    using Deliver = std::function<void(Word port, std::vector<PortInput> units, std::size_t read)>;

    // Hand units over unless the service stopped.
    void deliver(Word port, std::vector<PortInput> units, std::size_t read);

    std::mutex mutex;
    bool open = true;
    Deliver receiver;
};

// A writer thread of one port: writes queued output in order, then closes an owned descriptor once closed.
struct Writer final {
    Word port = 0;
    int fd = -1;
    bool owned = false;
    std::mutex mutex;
    std::condition_variable ready;
    std::deque<std::vector<std::byte>> queue;
    // Set when the port closed: the queue is still written, then the thread ends.
    bool closing = false;
    // Set when the port no longer wants error reports.
    std::atomic<bool> stopped{false};
};

// Reads port input on threads of its own and hands every unit to the executor, in order per port; writes queued
// output; reports exit statuses. The readers are platform specific: a poll() thread on POSIX hosts, a reader thread
// per input on Windows.
class IoService {
  public:
    using Deliver = IoGate::Deliver;

    explicit IoService(Deliver deliver);
    IoService(const IoService &) = delete;
    IoService &operator=(const IoService &) = delete;
    IoService(IoService &&) = delete;
    IoService &operator=(IoService &&) = delete;
    // Stop delivering; writers finish their queues and readers and watchers are let go.
    virtual ~IoService();

    // Read descriptor `fd` for `port`, framed as `options` says; an owned descriptor is closed when reading ends.
    virtual void read_descriptor(Word port, Descriptor input, const PortOptions &options) = 0;
    // Write the port's queued output to `output` on a writer thread.
    void write_descriptor(Word port, Descriptor output);
    // Queue output of `port`.
    void send(Word port, std::vector<std::byte> bytes);
    // Report the exit status of a spawned program of `port` when it exits.
    void watch_child(Word port, Child child);
    // Stop delivering input and errors of `port`; its writer still writes what is queued, its program is reaped.
    virtual void forget(Word port);

  protected:
    // Shared with the I/O threads, which may outlive the service.
    std::shared_ptr<IoGate> gate_;

  private:
    // Guards writers_ and watchers_.
    std::mutex mutex_;
    std::map<Word, std::shared_ptr<Writer>> writers_;
    // Whether each port's program watcher may still report.
    std::map<Word, std::shared_ptr<std::atomic<bool>>> watchers_;
};

// The I/O service of this platform.
std::unique_ptr<IoService> make_io_service(IoService::Deliver deliver);
} // namespace clause::runtime::detail
