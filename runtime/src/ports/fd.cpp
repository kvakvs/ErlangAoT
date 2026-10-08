#include "port.hpp"
#include <algorithm>
#include <bit>
#include <cstdio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <io.h>
#include <windows.h>
#else
#include <cerrno>
#include <unistd.h>
#endif

// The fd driver (docs/ports.md#drivers): an {fd, In, Out} port writes its output to descriptor Out at once.
namespace erlang_aot::runtime::detail {
namespace {
#ifdef _WIN32
// Write every byte to the descriptor's Windows handle; the reason of a failure, as OTP's fd driver reports it.
std::expected<void, DriverError> write_all(int fd, std::span<const std::byte> bytes) {
    const auto handle = std::bit_cast<HANDLE>(_get_osfhandle(fd));
    if (handle == INVALID_HANDLE_VALUE) {
        return std::unexpected(DriverError{"ebadf"});
    }
    while (!bytes.empty()) {
        DWORD written = 0;
        const auto size = static_cast<DWORD>(std::min<std::size_t>(bytes.size(), 1U << 30));
        if (!WriteFile(handle, bytes.data(), size, &written, nullptr)) {
            return std::unexpected(DriverError{GetLastError() == ERROR_NO_DATA ? "epipe" : "eio"});
        }
        bytes = bytes.subspan(written);
    }
    return {};
}
#else
// Write every byte to the descriptor, retrying interrupted writes.
std::expected<void, DriverError> write_all(int fd, std::span<const std::byte> bytes) {
    while (!bytes.empty()) {
        const auto written = ::write(fd, bytes.data(), bytes.size());
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written < 0) {
            return std::unexpected(DriverError{errno == EPIPE ? "epipe" : (errno == EBADF ? "ebadf" : "eio")});
        }
        bytes = bytes.subspan(static_cast<std::size_t>(written));
    }
    return {};
}
#endif

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
} // namespace erlang_aot::runtime::detail
