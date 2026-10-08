#include "io.hpp"
#include <algorithm>

// Input framing of ports (docs/ports.md#data-modes-and-options).
namespace erlang_aot::runtime::detail {
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
} // namespace erlang_aot::runtime::detail

namespace erlang_aot::runtime::detail {
namespace {
// Write a port's queue until the port closed and the queue is empty; a failed write reports the error once and
// drops the rest. An owned descriptor is closed at the end.
void write_loop(const std::shared_ptr<IoGate> &gate, const std::shared_ptr<Writer> &writer) {
    bool failed = false;
    for (;;) {
        std::vector<std::byte> bytes;
        {
            std::unique_lock lock(writer->mutex);
            writer->ready.wait(lock, [&] { return !writer->queue.empty() || writer->closing; });
            if (writer->queue.empty()) {
                break;
            }
            bytes = std::move(writer->queue.front());
            writer->queue.pop_front();
        }
        if (failed) {
            continue;
        }
        if (const auto written = write_all(writer->fd, bytes); !written && !writer->stopped) {
            failed = true;
            gate->deliver(
                writer->port,
                {PortInput{.kind = PortInput::Kind::error, .bytes = {}, .reason = written.error().reason, .status = 0}},
                0);
        }
    }
    if (writer->owned) {
        close_descriptor(writer->fd);
    }
}
} // namespace

void IoGate::deliver(Word port, std::vector<PortInput> units, std::size_t read) {
    const std::scoped_lock lock(mutex);
    if (open) {
        receiver(port, std::move(units), read);
    }
}

IoService::IoService(Deliver deliver) : gate_(std::make_shared<IoGate>()) { gate_->receiver = std::move(deliver); }

IoService::~IoService() {
    {
        const std::scoped_lock lock(gate_->mutex);
        gate_->open = false;
    }
    const std::scoped_lock lock(mutex_);
    for (auto &[port, writer] : writers_) {
        const std::scoped_lock writing(writer->mutex);
        writer->closing = true;
        writer->ready.notify_one();
    }
}

void IoService::write_descriptor(Word port, Descriptor output) {
    auto writer = std::make_shared<Writer>();
    writer->port = port;
    writer->fd = output.fd;
    writer->owned = output.owned;
    std::thread([gate = gate_, writer] { write_loop(gate, writer); }).detach();
    const std::scoped_lock lock(mutex_);
    writers_.insert_or_assign(port, std::move(writer));
}

void IoService::send(Word port, std::vector<std::byte> bytes) {
    std::shared_ptr<Writer> writer;
    {
        const std::scoped_lock lock(mutex_);
        const auto found = writers_.find(port);
        if (found == writers_.end()) {
            return;
        }
        writer = found->second;
    }
    const std::scoped_lock lock(writer->mutex);
    writer->queue.push_back(std::move(bytes));
    writer->ready.notify_one();
}

void IoService::watch_child(Word port, Child child) {
    auto reporting = std::make_shared<std::atomic<bool>>(true);
    std::thread([gate = gate_, port, child, reporting] {
        const auto status = wait_child(child.handle);
        if (*reporting) {
            gate->deliver(port,
                          {PortInput{.kind = PortInput::Kind::status, .bytes = {}, .reason = {}, .status = status}}, 0);
        }
    }).detach();
    const std::scoped_lock lock(mutex_);
    watchers_.insert_or_assign(port, std::move(reporting));
}

void IoService::forget(Word port) {
    const std::scoped_lock lock(mutex_);
    if (const auto writer = writers_.find(port); writer != writers_.end()) {
        writer->second->stopped = true;
        const std::scoped_lock writing(writer->second->mutex);
        writer->second->closing = true;
        writer->second->ready.notify_one();
        writers_.erase(writer);
    }
    if (const auto watcher = watchers_.find(port); watcher != watchers_.end()) {
        *watcher->second = false;
        watchers_.erase(watcher);
    }
}
} // namespace erlang_aot::runtime::detail
