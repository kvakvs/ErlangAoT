#pragma once
#include "port.hpp"
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

// The I/O thread of a runtime (docs/ports.md#io-thread): it reads the input of ports and hands it, framed, to the
// executor, which delivers it as messages to the ports' connected processes.
namespace erlang_aot::runtime::detail {
// One unit of port input after framing: data (a stream chunk, a packet or a whole line), a line part (eol, noeol),
// the end of input, or a read error with its POSIX reason.
struct PortInput final {
    enum class Kind : std::uint8_t { data, eol, noeol, end, error };
    Kind kind = Kind::data;
    std::vector<std::byte> bytes;
    std::string reason;
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

// Reads port input on threads of its own and hands every unit to `deliver`, in order per port. Platform
// specific: a poll() thread on POSIX hosts, a reader thread per input on Windows.
class IoService {
  public:
    // Receives the input of a port: the units completed and the bytes read; called on an I/O thread, never while
    // the service holds its own lock.
    using Deliver = std::function<void(Word port, std::vector<PortInput> units, std::size_t read)>;

    IoService() = default;
    IoService(const IoService &) = delete;
    IoService &operator=(const IoService &) = delete;
    IoService(IoService &&) = delete;
    IoService &operator=(IoService &&) = delete;
    // Stop reading and join the I/O threads; descriptors the program owns stay open.
    virtual ~IoService() = default;

    // Read descriptor `fd` (owned by the program) for `port`, framed as `options` says.
    virtual void read_descriptor(Word port, int fd, const PortOptions &options) = 0;
    // Stop delivering input of `port`; units already handed over may still arrive.
    virtual void forget(Word port) = 0;
};

// The I/O service of this platform.
std::unique_ptr<IoService> make_io_service(IoService::Deliver deliver);
} // namespace erlang_aot::runtime::detail
