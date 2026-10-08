#include "io.hpp"
#include <algorithm>
#include <bit>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <io.h>
#include <windows.h>

// The I/O service on Windows (docs/ports.md#io-thread): each input descriptor gets a reader thread that blocks in
// ReadFile, as console and anonymous-pipe handles cannot be overlapped, and hands framed input to the executor.
namespace erlang_aot::runtime::detail {
namespace {
// Bytes one read asks for.
constexpr std::size_t READ_BYTES = std::size_t{64} * 1024;

// The Windows handle behind a C runtime descriptor.
HANDLE handle_of(int fd) noexcept { return std::bit_cast<HANDLE>(_get_osfhandle(fd)); }

// One input being read.
struct Reader final {
    Word port = 0;
    int fd = -1;
    bool owned = false;
    InputDecoder decoder;
    // Set when the port no longer wants input; the thread then delivers nothing more.
    std::atomic<bool> stopped{false};
};

// The last units of a read that returned no bytes: the end of input, or an error.
std::vector<PortInput> last_units(Reader &reader, BOOL ok) {
    const auto error = GetLastError();
    if (ok || error == ERROR_BROKEN_PIPE || error == ERROR_HANDLE_EOF) {
        return reader.decoder.finish();
    }
    return {PortInput{.kind = PortInput::Kind::error, .bytes = {}, .reason = "eio", .status = 0}};
}

// Read until end of input, an error or a stop; the end or error is the last unit. An owned descriptor is closed.
void read_loop(const std::shared_ptr<IoGate> &gate, const std::shared_ptr<Reader> &reader) {
    std::vector<std::byte> buffer(READ_BYTES);
    for (;;) {
        DWORD read = 0;
        const BOOL ok =
            ReadFile(handle_of(reader->fd), buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr);
        if (reader->stopped) {
            break;
        }
        if (!ok || read == 0) {
            gate->deliver(reader->port, last_units(*reader, ok), 0);
            break;
        }
        gate->deliver(reader->port, reader->decoder.feed(std::span(buffer).first(read)), read);
    }
    if (reader->owned) {
        close_descriptor(reader->fd);
    }
}

class WindowsIo final : public IoService {
  public:
    using IoService::IoService;

    WindowsIo(const WindowsIo &) = delete;
    WindowsIo &operator=(const WindowsIo &) = delete;
    WindowsIo(WindowsIo &&) = delete;
    WindowsIo &operator=(WindowsIo &&) = delete;

    // Let every reader go: a reader blocked in a read that cannot be cancelled (a console with no input) delivers
    // nothing more.
    ~WindowsIo() override {
        const std::scoped_lock lock(mutex_);
        for (auto &[port, entry] : readers_) {
            release(entry);
        }
    }

    void read_descriptor(Word port, Descriptor input, const PortOptions &options) override {
        auto reader = std::make_shared<Reader>(port, input.fd, input.owned, InputDecoder(options));
        std::thread thread([gate = gate_, reader] { read_loop(gate, reader); });
        const std::scoped_lock lock(mutex_);
        readers_.insert_or_assign(port, Entry{std::move(reader), std::move(thread)});
    }

    void forget(Word port) override {
        IoService::forget(port);
        const std::scoped_lock lock(mutex_);
        const auto found = readers_.find(port);
        if (found != readers_.end()) {
            release(found->second);
            readers_.erase(found);
        }
    }

  private:
    // A reader and its thread.
    struct Entry {
        std::shared_ptr<Reader> reader;
        std::thread thread;
    };

    // Stop a reader: cancel its blocking read and detach its thread, which keeps its own state alive.
    static void release(Entry &entry) noexcept {
        entry.reader->stopped = true;
        if (entry.thread.joinable()) {
            CancelSynchronousIo(entry.thread.native_handle());
            entry.thread.detach();
        }
    }

    // Guards readers_.
    std::mutex mutex_;
    std::map<Word, Entry> readers_;
};
} // namespace

std::expected<void, DriverError> write_all(int fd, std::span<const std::byte> bytes) {
    const auto handle = handle_of(fd);
    if (handle == INVALID_HANDLE_VALUE) {
        return std::unexpected(DriverError{"ebadf"});
    }
    while (!bytes.empty()) {
        DWORD written = 0;
        const auto size = static_cast<DWORD>(std::min<std::size_t>(bytes.size(), std::size_t{1} << 30));
        if (!WriteFile(handle, bytes.data(), size, &written, nullptr)) {
            const auto error = GetLastError();
            return std::unexpected(DriverError{error == ERROR_NO_DATA || error == ERROR_BROKEN_PIPE ? "epipe" : "eio"});
        }
        bytes = bytes.subspan(written);
    }
    return {};
}

void close_descriptor(int fd) noexcept { _close(fd); }

std::int64_t wait_child(std::int64_t child) noexcept {
    const auto handle = std::bit_cast<HANDLE>(static_cast<std::intptr_t>(child));
    WaitForSingleObject(handle, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(handle, &code);
    CloseHandle(handle);
    return code;
}

std::unique_ptr<IoService> make_io_service(IoService::Deliver deliver) {
    return std::make_unique<WindowsIo>(std::move(deliver));
}
} // namespace erlang_aot::runtime::detail
