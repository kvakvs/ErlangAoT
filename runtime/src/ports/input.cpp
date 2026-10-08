#include "input.hpp"
#include <algorithm>

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
