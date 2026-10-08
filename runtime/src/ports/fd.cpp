#include "io.hpp"
#include "port.hpp"
#include <cstdio>

// The fd driver (docs/ports.md#drivers): an {fd, In, Out} port writes its output to descriptor Out at once.
namespace clause::runtime::detail {
namespace {
class FdDriver final : public PortDriver {
  public:
    // Keep the descriptors; they belong to the program, not to the port, and stay open after it closes.
    explicit FdDriver(std::pair<int, int> descriptors) noexcept : in_(descriptors.first), out_(descriptors.second) {}

    std::optional<int> input() const noexcept override { return in_; }

    // Flush buffered standard output and error first, so bytes keep the order in which they were written.
    std::expected<void, DriverError> write(std::span<const std::byte> bytes) override {
        std::fflush(stdout);
        std::fflush(stderr);
        return write_all(out_, bytes);
    }

  private:
    // The input and output descriptors.
    int in_;
    int out_;
};
} // namespace

std::unique_ptr<PortDriver> fd_driver(int in, int out) { return std::make_unique<FdDriver>(std::pair{in, out}); }

std::optional<std::vector<std::byte>> framed(const PortOptions &options, std::vector<std::byte> bytes) {
    if (options.framing != Framing::packet) {
        return bytes;
    }
    const auto header = options.packet_bytes;
    if (header < sizeof(std::uint64_t) && bytes.size() >> (8 * header) != 0) {
        return std::nullopt;
    }
    std::vector<std::byte> result(header);
    for (std::size_t at = 0; at < header; ++at) {
        result[header - 1 - at] = static_cast<std::byte>((bytes.size() >> (8 * at)) & 0xff);
    }
    result.insert(result.end(), bytes.begin(), bytes.end());
    return result;
}
} // namespace clause::runtime::detail
