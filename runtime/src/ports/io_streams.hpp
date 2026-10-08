#pragma once
#include "io.hpp"
#include <boost/asio.hpp>
#include <map>

// The I/O service's state on the reactor's thread, private to io.cpp and the platform files io_windows.cpp and
// io_posix.cpp: the channels of each port and the platform operations they are built from.
namespace clause::runtime::detail {
namespace asio = boost::asio;

// One I/O object of a port on the reactor's thread: an input reader, an output writer or a program watcher. Its
// handlers keep it alive while they are pending.
class Channel {
  public:
    Channel() = default;
    Channel(const Channel &) = delete;
    Channel &operator=(const Channel &) = delete;
    Channel(Channel &&) = delete;
    Channel &operator=(Channel &&) = delete;
    virtual ~Channel() = default;

    // The port no longer wants reports: an input stops reading, an output still writes what is queued, a watcher
    // stops reporting (a POSIX program is still reaped).
    virtual void stop() = 0;
};

class IoService::Impl final {
  public:
    Impl(asio::io_context &io, Deliver deliver);

    // Hand units of `port` to the executor unless the service stopped.
    void deliver(Word port, std::vector<PortInput> units, std::size_t read) const {
        gate->deliver(port, std::move(units), read);
    }

    // The reactor's io_context, which every channel uses.
    asio::io_context &context;
    // Shared with Windows blocking readers, which may outlive the service.
    std::shared_ptr<IoGate> gate;

    // The channels of one port.
    struct Channels final {
        std::shared_ptr<Channel> input;
        std::shared_ptr<Channel> output;
        std::shared_ptr<Channel> child;
    };

    // The channels of each port not yet forgotten.
    std::map<Word, Channels> ports;
    // Platform state shared by every port (the POSIX reaper of programs), created on first use.
    std::shared_ptr<Channel> shared;
};

// The asynchronous stream type of the pipes the runtime creates for spawned programs.
#if defined(_WIN32)
using PipeStream = asio::windows::stream_handle;
#else
using PipeStream = asio::posix::stream_descriptor;
#endif

// Platform parts, run on the reactor's thread. The stream of a pipe end the runtime created, owning it.
PipeStream pipe_stream(asio::io_context &context, NativeHandle handle);
// Start reading `input` for `port`, framed by `decoder`.
std::shared_ptr<Channel> start_input(IoService::Impl &service, Word port, Descriptor input, InputDecoder decoder);
// Start waiting for the exit of the spawned program `child` of `port`.
std::shared_ptr<Channel> start_child(IoService::Impl &service, Word port, Child child);
// The POSIX reason a failed pipe write closes its port with.
std::string write_reason(const boost::system::error_code &error);
// Prepare the process for port I/O once (POSIX: writes to a closed pipe fail instead of raising SIGPIPE).
void prepare_io() noexcept;
} // namespace clause::runtime::detail
