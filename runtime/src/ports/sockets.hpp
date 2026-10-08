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
