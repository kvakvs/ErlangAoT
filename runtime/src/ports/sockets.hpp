#pragma once
#include "port.hpp"
#include "reactor.hpp"
#include "value.hpp"
#include <functional>
#include <memory>

// TCP and UDP sockets as ports (docs/ports.md#sockets) on Boost.Asio, served by the reactor's I/O thread (an I/O
// completion port on Windows, epoll on Linux, kqueue on macOS). The library modules gen_tcp,
// gen_udp and inet drive a socket port through port_control/3 operations (SocketOperation in sockets.cpp).
namespace clause::runtime::detail {
// Something a socket reports to the executor from the I/O thread.
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

// The sockets of a runtime on the reactor's thread.
class SocketService final {
  public:
    // Receives a socket's events on the reactor's thread; the executor takes its lock.
    using Deliver = std::function<void(Word port, SocketEvent event)>;

    SocketService(Reactor &reactor, Deliver deliver);
    SocketService(const SocketService &) = delete;
    SocketService &operator=(const SocketService &) = delete;
    SocketService(SocketService &&) = delete;
    SocketService &operator=(SocketService &&) = delete;
    // Stop delivering; waits for a delivery in progress, so it is called without the executor lock. Sockets close
    // with their ports and their handlers, before the reactor's io_context goes.
    ~SocketService();

    // A new socket port's driver: {spawn_driver, "tcp_inet"} or {spawn_driver, "udp_inet"}.
    std::unique_ptr<PortDriver> driver(bool udp);

    class Impl;

  private:
    // Shared with the sockets, whose handlers may outlive the service.
    std::shared_ptr<Impl> impl_;
};
} // namespace clause::runtime::detail
