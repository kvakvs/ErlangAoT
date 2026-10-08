#include "io_streams.hpp"
#include <algorithm>
#include <deque>

// Input framing of ports (docs/ports.md#data-modes-and-options).
namespace clause::runtime::detail {
std::vector<PortInput> InputDecoder::feed(std::span<const std::byte> bytes) {
    std::vector<PortInput> units;
    if (framing_ == Framing::stream) {
        if (!bytes.empty()) {
            units.push_back(
                {.kind = PortInput::Kind::data, .bytes = {bytes.begin(), bytes.end()}, .reason = {}, .status = 0});
        }
        return units;
    }
    pending_.insert(pending_.end(), bytes.begin(), bytes.end());
    if (framing_ == Framing::packet) {
        packets(units);
    } else {
        lines(units);
    }
    return units;
}

std::vector<PortInput> InputDecoder::finish() {
    std::vector<PortInput> units;
    if (framing_ == Framing::line && !pending_.empty()) {
        units.push_back(
            {.kind = PortInput::Kind::noeol, .bytes = std::exchange(pending_, {}), .reason = {}, .status = 0});
    }
    units.push_back({.kind = PortInput::Kind::end, .bytes = {}, .reason = {}, .status = 0});
    return units;
}

void InputDecoder::packets(std::vector<PortInput> &units) {
    std::size_t at = 0;
    while (pending_.size() - at >= packet_bytes_) {
        std::size_t size = 0;
        for (std::size_t index = 0; index < packet_bytes_; ++index) {
            size = (size << 8) | std::to_integer<std::size_t>(pending_[at + index]);
        }
        if (pending_.size() - at - packet_bytes_ < size) {
            break;
        }
        const auto *begin = pending_.data() + at + packet_bytes_;
        units.push_back({.kind = PortInput::Kind::data, .bytes = {begin, begin + size}, .reason = {}, .status = 0});
        at += packet_bytes_ + size;
    }
    pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(at));
}

void InputDecoder::lines(std::vector<PortInput> &units) {
    for (;;) {
        const auto newline = std::ranges::find(pending_, std::byte{'\n'});
        const auto length = static_cast<std::size_t>(newline - pending_.begin());
        if (length > line_length_ || (newline == pending_.end() && pending_.size() > line_length_)) {
            // A line longer than L arrives in parts of L bytes.
            const auto end = pending_.begin() + static_cast<std::ptrdiff_t>(line_length_);
            units.push_back(
                {.kind = PortInput::Kind::noeol, .bytes = {pending_.begin(), end}, .reason = {}, .status = 0});
            pending_.erase(pending_.begin(), end);
            continue;
        }
        if (newline == pending_.end()) {
            return;
        }
        units.push_back(
            {.kind = PortInput::Kind::eol, .bytes = {pending_.begin(), newline}, .reason = {}, .status = 0});
        pending_.erase(pending_.begin(), newline + 1);
    }
}
} // namespace clause::runtime::detail

namespace clause::runtime::detail {
namespace {
// Writes a port's queued output to a pipe in order; once stopped it writes what is queued, then closes the pipe. A
// failed write reports the error once and drops the rest.
class Output final : public Channel, public std::enable_shared_from_this<Output> {
  public:
    Output(IoService::Impl &service, Word port, PipeStream stream)
        : service_(service), port_(port), stream_(std::move(stream)) {}

    // Queue `bytes` after the output before them.
    void send(std::vector<std::byte> bytes) {
        if (failed_) {
            return;
        }
        queue_.push_back(std::move(bytes));
        if (!writing_) {
            next();
        }
    }

    void stop() override {
        stopped_ = true;
        if (!writing_) {
            next();
        }
    }

  private:
    // Start writing the oldest queued output; close the pipe once stopped with nothing queued.
    void next() {
        if (queue_.empty()) {
            if (stopped_) {
                boost::system::error_code ignored;
                stream_.close(ignored);
            }
            return;
        }
        writing_ = true;
        asio::async_write(
            stream_, asio::buffer(queue_.front()),
            [self = shared_from_this()](const boost::system::error_code &error, std::size_t) { self->written(error); });
    }

    // A write completed: report a failure once, then go on with the queue.
    void written(const boost::system::error_code &error) {
        writing_ = false;
        queue_.pop_front();
        if (error) {
            if (!stopped_) {
                service_.deliver(
                    port_,
                    {PortInput{
                        .kind = PortInput::Kind::error, .bytes = {}, .reason = write_reason(error), .status = 0}},
                    0);
            }
            failed_ = true;
            queue_.clear();
        }
        next();
    }

    IoService::Impl &service_;
    Word port_;
    PipeStream stream_;
    // Output not written yet, oldest first; the front is being written while writing_ is set.
    std::deque<std::vector<std::byte>> queue_;
    bool writing_ = false;
    // Set when the port was forgotten, and after a failed write.
    bool stopped_ = false;
    bool failed_ = false;
};
} // namespace

void IoGate::deliver(Word port, std::vector<PortInput> units, std::size_t read) {
    const std::scoped_lock lock(mutex);
    if (open) {
        receiver(port, std::move(units), read);
    }
}

void IoGate::close() {
    const std::scoped_lock lock(mutex);
    open = false;
}

IoService::Impl::Impl(asio::io_context &io, Deliver deliver) : context(io), gate(std::make_shared<IoGate>()) {
    gate->receiver = std::move(deliver);
}

IoService::IoService(Reactor &reactor, Deliver deliver)
    : impl_(std::make_unique<Impl>(reactor.context(), std::move(deliver))) {
    prepare_io();
}

IoService::~IoService() {
    impl_->gate->close();
    // The reactor has stopped, so no handler runs while the channels go.
    impl_->ports.clear();
    impl_->shared.reset();
}

void IoService::close() { impl_->gate->close(); }

void IoService::read_descriptor(Word port, Descriptor input, const PortOptions &options) {
    asio::post(impl_->context, [impl = impl_.get(), port, input, decoder = InputDecoder(options)]() mutable {
        impl->ports[port].input = start_input(*impl, port, input, std::move(decoder));
    });
}

void IoService::write_descriptor(Word port, Descriptor output) {
    asio::post(impl_->context, [impl = impl_.get(), port, output] {
        impl->ports[port].output = std::make_shared<Output>(*impl, port, pipe_stream(impl->context, output.handle));
    });
}

void IoService::send(Word port, std::vector<std::byte> bytes) {
    asio::post(impl_->context, [impl = impl_.get(), port, bytes = std::move(bytes)]() mutable {
        const auto found = impl->ports.find(port);
        if (found != impl->ports.end() && found->second.output) {
            std::static_pointer_cast<Output>(found->second.output)->send(std::move(bytes));
        }
    });
}

void IoService::watch_child(Word port, Child child) {
    asio::post(impl_->context,
               [impl = impl_.get(), port, child] { impl->ports[port].child = start_child(*impl, port, child); });
}

void IoService::forget(Word port) {
    asio::post(impl_->context, [impl = impl_.get(), port] {
        const auto found = impl->ports.find(port);
        if (found == impl->ports.end()) {
            return;
        }
        for (const auto &channel : {found->second.input, found->second.output, found->second.child}) {
            if (channel) {
                channel->stop();
            }
        }
        impl->ports.erase(found);
    });
}
} // namespace clause::runtime::detail
