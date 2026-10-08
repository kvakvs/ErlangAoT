#pragma once
#include "port.hpp"
#include "value.hpp"
#include <functional>
#include <memory>
#include <thread>

// TCP and UDP sockets as ports (docs/ports.md#sockets) on Boost.Asio: one io_context runs on the runtime's socket
// thread (an I/O completion port on Windows, epoll on Linux, kqueue on macOS). The library modules gen_tcp,
// gen_udp and inet drive a socket port through port_control/3 operations (SocketOperation in sockets.cpp).
namespace erlang_aot::runtime::detail {
// Something a socket reports to the executor from the socket thread.
struct SocketEvent final {
    enum class Kind : std::uint8_t { message, accepted };
    Kind kind = Kind::message;
    // The process the message goes to; 0 for the port's connected process.
    Word target = 0;
    // The message ({tcp, S, Data}, {erlang_aot_socket, S, Reply}, ...); for accepted, the reply without the new
    // socket, which the executor adds once it has a port.
    PortValue value;
    // A connection a listening socket accepted for `target`, to become a new port it is connected to.
    std::shared_ptr<PortDriver> driver;
};

// The socket thread and its io_context.
class SocketService final {
  public:
    // Receives a socket's events on the socket thread; the executor takes its lock.
    using Deliver = std::function<void(Word port, SocketEvent event)>;

    explicit SocketService(Deliver deliver);
    SocketService(const SocketService &) = delete;
    SocketService &operator=(const SocketService &) = delete;
    SocketService(SocketService &&) = delete;
    SocketService &operator=(SocketService &&) = delete;
    // Stop delivering, stop the io_context and join its thread; called without the executor lock.
    ~SocketService();

    // A new socket port's driver: {spawn_driver, "tcp_inet"} or {spawn_driver, "udp_inet"}.
    std::unique_ptr<PortDriver> driver(bool udp);

    class Impl;

  private:
    // Shared with the sockets, whose handlers may outlive the service.
    std::shared_ptr<Impl> impl_;
    // Runs the io_context.
    std::thread thread_;
};
} // namespace erlang_aot::runtime::detail
