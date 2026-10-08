#include "io.hpp"
#include <atomic>
#include <bit>
#include <map>
#include <mutex>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <io.h>
#include <windows.h>

// The I/O service on Windows (docs/ports.md#io-thread): each input descriptor gets a reader thread that blocks in
// ReadFile, as console and anonymous-pipe handles cannot be overlapped, and hands framed input to the executor.
namespace erlang_aot::runtime::detail {
namespace {
// Lets readers deliver until the service stops; stopping waits for a delivery in progress.
struct Gate final {
    std::mutex mutex;
    bool open = true;
    IoService::Deliver deliver;
};

// Bytes one read asks for.
constexpr std::size_t READ_BYTES = std::size_t{64} * 1024;

// One input being read.
struct Reader final {
    Word port = 0;
    HANDLE handle = INVALID_HANDLE_VALUE;
    InputDecoder decoder;
    // Set when the port no longer wants input; the thread then delivers nothing more.
    std::atomic<bool> stopped{false};
    std::thread thread;
};

// Hand `read` bytes and the `units` they completed to the executor unless the port or the service stopped.
void deliver(Gate &gate, Reader &reader, std::size_t read, std::vector<PortInput> units) {
    if ((units.empty() && read == 0) || reader.stopped) {
        return;
    }
    const std::scoped_lock lock(gate.mutex);
    if (gate.open) {
        gate.deliver(reader.port, std::move(units), read);
    }
}

// Read until end of input, an error or a stop; the end or error is the last unit.
void read_loop(const std::shared_ptr<Gate> &gate, const std::shared_ptr<Reader> &reader) {
    std::vector<std::byte> buffer(READ_BYTES);
    for (;;) {
        DWORD read = 0;
        const BOOL ok = ReadFile(reader->handle, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr);
        if (!ok || read == 0) {
            const auto error = GetLastError();
            const bool end = read == 0 && (ok || error == ERROR_BROKEN_PIPE || error == ERROR_HANDLE_EOF);
            auto units = end ? reader->decoder.finish()
                             : std::vector{PortInput{.kind = PortInput::Kind::error, .bytes = {}, .reason = "eio"}};
            deliver(*gate, *reader, 0, std::move(units));
            return;
        }
        deliver(*gate, *reader, read, reader->decoder.feed(std::span(buffer).first(read)));
    }
}

class WindowsIo final : public IoService {
  public:
    explicit WindowsIo(Deliver deliver) : gate_(std::make_shared<Gate>()) { gate_->deliver = std::move(deliver); }

    WindowsIo(const WindowsIo &) = delete;
    WindowsIo &operator=(const WindowsIo &) = delete;
    WindowsIo(WindowsIo &&) = delete;
    WindowsIo &operator=(WindowsIo &&) = delete;

    // Close the gate, then let every reader go: a reader blocked in a read that cannot be cancelled (a console
    // with no input) is detached and delivers nothing more.
    ~WindowsIo() override {
        {
            const std::scoped_lock lock(gate_->mutex);
            gate_->open = false;
        }
        const std::scoped_lock lock(mutex_);
        for (auto &[port, reader] : readers_) {
            release(*reader);
        }
    }

    void read_descriptor(Word port, int fd, const PortOptions &options) override {
        auto reader = std::make_shared<Reader>(port, std::bit_cast<HANDLE>(_get_osfhandle(fd)), InputDecoder(options));
        reader->thread = std::thread([gate = gate_, reader] { read_loop(gate, reader); });
        const std::scoped_lock lock(mutex_);
        readers_.insert_or_assign(port, std::move(reader));
    }

    void forget(Word port) override {
        std::shared_ptr<Reader> reader;
        {
            const std::scoped_lock lock(mutex_);
            const auto found = readers_.find(port);
            if (found == readers_.end()) {
                return;
            }
            reader = std::move(found->second);
            readers_.erase(found);
        }
        release(*reader);
    }

  private:
    // Stop a reader: cancel its blocking read and detach its thread, which keeps its own state alive.
    static void release(Reader &reader) noexcept {
        reader.stopped = true;
        if (reader.thread.joinable()) {
            CancelSynchronousIo(reader.thread.native_handle());
            reader.thread.detach();
        }
    }

    // Shared with the reader threads, which may outlive the service.
    std::shared_ptr<Gate> gate_;
    // Guards readers_.
    std::mutex mutex_;
    std::map<Word, std::shared_ptr<Reader>> readers_;
};
} // namespace

std::unique_ptr<IoService> make_io_service(IoService::Deliver deliver) {
    return std::make_unique<WindowsIo>(std::move(deliver));
}
} // namespace erlang_aot::runtime::detail
