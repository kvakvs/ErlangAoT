#pragma once
#include <memory>
#include <thread>

namespace boost::asio {
class io_context;
} // namespace boost::asio

// The I/O thread of a runtime (docs/ports.md#io-thread): one Boost.Asio io_context (an I/O completion port on
// Windows, epoll on Linux, kqueue on macOS) that serves every port's pipes, descriptors, program exits and sockets.
namespace clause::runtime::detail {
class Reactor final {
  public:
    // Start the I/O thread; it runs until stop().
    Reactor();
    Reactor(const Reactor &) = delete;
    Reactor &operator=(const Reactor &) = delete;
    Reactor(Reactor &&) = delete;
    Reactor &operator=(Reactor &&) = delete;
    // Stop if still running, then destroy the io_context with its pending handlers. Every I/O object the services
    // keep outside handlers must be gone by now.
    ~Reactor();

    // The io_context that I/O objects and posted work use.
    boost::asio::io_context &context() noexcept { return *context_; }

    // Stop running handlers and join the thread; handlers not run yet stay pending until destruction. Called without
    // the executor's lock, as deliveries in progress take it.
    void stop() noexcept;

  private:
    // Owned through a pointer so this header needs no Asio.
    std::unique_ptr<boost::asio::io_context> context_;
    // Runs the io_context.
    std::thread thread_;
};
} // namespace clause::runtime::detail
