#include "reactor.hpp"
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <exception>

namespace clause::runtime::detail {
Reactor::Reactor() : context_(std::make_unique<boost::asio::io_context>(1)) {
    thread_ = std::thread([context = context_.get()] {
        const auto guard = boost::asio::make_work_guard(*context);
        context->run();
    });
}

Reactor::~Reactor() { stop(); }

void Reactor::stop() noexcept {
    if (!thread_.joinable()) {
        return;
    }
    try {
        context_->stop();
        thread_.join();
    } catch (...) {
        // A failing join would leave the I/O thread running handlers on destroyed state.
        std::terminate();
    }
}
} // namespace clause::runtime::detail
