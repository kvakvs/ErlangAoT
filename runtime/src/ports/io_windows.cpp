#include "io_streams.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <thread>

// Asio has included <windows.h> with WIN32_LEAN_AND_MEAN and NOMINMAX.
#include <io.h>

// Port I/O on Windows (docs/ports.md#io-thread): the pipes of spawned programs are overlapped and served by the
// reactor's I/O completion port, program exits are waited for through it too. An fd port's input handle cannot be
// overlapped (a console or an inherited anonymous pipe), so it is read by a blocking thread, as libuv and ERTS do.
namespace clause::runtime::detail {
namespace {
// Bytes one read asks for.
constexpr std::size_t READ_BYTES = std::size_t{64} * 1024;

// The HANDLE of a native handle.
HANDLE handle_of(NativeHandle handle) noexcept { return std::bit_cast<HANDLE>(handle); }

// Reads an overlapped pipe end until end of input, an error or a stop, then closes it.
class PipeInput final : public Channel, public std::enable_shared_from_this<PipeInput> {
  public:
    PipeInput(IoService::Impl &service, Word port, Descriptor input)
        : service_(service), port_(port), stream_(service.context, handle_of(input.handle)), buffer_(READ_BYTES) {}

    // Start the first read.
    void start() { next(); }

    void stop() override {
        stopped_ = true;
        close();
    }

    void pause(bool paused) override {
        paused_ = paused;
        if (!paused_ && !reading_ && !stopped_ && stream_.is_open()) {
            next();
        }
    }

  private:
    // Read what the pipe has, up to the buffer size.
    void next() {
        reading_ = true;
        stream_.async_read_some(asio::buffer(buffer_),
                                [self = shared_from_this()](const boost::system::error_code &error, std::size_t bytes) {
                                    self->completed(error, bytes);
                                });
    }

    // Deliver what a read gave and read on; at end of input or an error deliver the last units and close.
    void completed(const boost::system::error_code &error, std::size_t bytes) {
        reading_ = false;
        if (stopped_) {
            return;
        }
        if (!error) {
            service_.deliver(port_, {PortInput::raw(std::span(buffer_).first(bytes))}, bytes);
            if (!paused_) {
                next();
            }
            return;
        }
        const bool ended = error == asio::error::eof || error == asio::error::broken_pipe;
        service_.deliver(port_,
                         ended ? std::vector{PortInput::of(PortInput::Kind::end)}
                               : std::vector{PortInput::of(PortInput::Kind::error, "eio")},
                         0);
        close();
    }

    // Close the pipe end, cancelling a pending read.
    void close() noexcept {
        boost::system::error_code ignored;
        stream_.close(ignored);
    }

    IoService::Impl &service_;
    Word port_;
    asio::windows::stream_handle stream_;
    std::vector<std::byte> buffer_;
    // Set when the port no longer wants input; while paused, no new read starts; while a read is pending.
    bool stopped_ = false;
    bool paused_ = false;
    bool reading_ = false;
};

// The state of a blocking reader thread, which may outlive its channel.
struct Reader final {
    Word port = 0;
    HANDLE handle = nullptr;
    // Set when the port no longer wants input; the thread then delivers nothing more.
    std::atomic<bool> stopped{false};
    // While paused the thread waits before its next read; `changed` wakes it.
    std::mutex mutex;
    std::condition_variable changed;
    bool paused = false;

    // Wait until not paused or stopped; false once stopped.
    bool wait_unpaused() {
        std::unique_lock lock(mutex);
        changed.wait(lock, [this] { return !paused || stopped; });
        return !stopped;
    }

    // Pause or resume reading.
    void pause(bool now) {
        const std::scoped_lock lock(mutex);
        paused = now;
        changed.notify_all();
    }
};

// The last units of a read that returned no bytes: the end of input, or an error.
std::vector<PortInput> last_units(BOOL ok) {
    const auto error = GetLastError();
    if (ok || error == ERROR_BROKEN_PIPE || error == ERROR_HANDLE_EOF) {
        return {PortInput::of(PortInput::Kind::end)};
    }
    return {PortInput::of(PortInput::Kind::error, "eio")};
}

// Read until end of input, an error or a stop; the end or error is the last unit. The handle is the program's own
// and stays open.
void read_loop(const std::shared_ptr<IoGate> &gate, const std::shared_ptr<Reader> &reader) {
    std::vector<std::byte> buffer(READ_BYTES);
    while (reader->wait_unpaused()) {
        DWORD read = 0;
        const BOOL ok = ReadFile(reader->handle, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr);
        if (reader->stopped) {
            return;
        }
        if (!ok || read == 0) {
            gate->deliver(reader->port, last_units(ok), 0);
            return;
        }
        gate->deliver(reader->port, {PortInput::raw(std::span(buffer).first(read))}, read);
    }
}

// Reads an fd port's handle on a blocking thread of its own; stopping cancels the read and lets the thread go (a
// read that cannot be cancelled, a console with no input, delivers nothing more).
class BlockingInput final : public Channel {
  public:
    BlockingInput(const IoService::Impl &service, Word port, Descriptor input)
        : reader_(std::make_shared<Reader>(port, handle_of(input.handle))) {
        thread_ = std::thread([gate = service.gate, reader = reader_] { read_loop(gate, reader); });
    }

    BlockingInput(const BlockingInput &) = delete;
    BlockingInput &operator=(const BlockingInput &) = delete;
    BlockingInput(BlockingInput &&) = delete;
    BlockingInput &operator=(BlockingInput &&) = delete;

    ~BlockingInput() override { release(); }

    void stop() override { release(); }

    void pause(bool paused) override { reader_->pause(paused); }

  private:
    // Stop the reader: cancel its blocking read and detach its thread, which keeps its own state alive.
    void release() noexcept {
        reader_->stopped = true;
        reader_->pause(false);
        if (thread_.joinable()) {
            CancelSynchronousIo(thread_.native_handle());
            thread_.detach();
        }
    }

    std::shared_ptr<Reader> reader_;
    std::thread thread_;
};

// Waits for a spawned program to exit and reports its exit code on the reactor's thread; owns the process handle.
// The system's wait thread pool waits (as Asio's object_handle does), so a program costs no thread of its own.
class ProcessWatch final : public Channel, public std::enable_shared_from_this<ProcessWatch> {
  public:
    ProcessWatch(IoService::Impl &service, Word port, Child child) noexcept
        : service_(service), port_(port), process_(std::bit_cast<HANDLE>(static_cast<std::intptr_t>(child.handle))) {}

    ProcessWatch(const ProcessWatch &) = delete;
    ProcessWatch &operator=(const ProcessWatch &) = delete;
    ProcessWatch(ProcessWatch &&) = delete;
    ProcessWatch &operator=(ProcessWatch &&) = delete;

    // Wait for a callback in progress to finish, so it never posts for a destroyed watch, then close the handle.
    ~ProcessWatch() override {
        unregister();
        CloseHandle(process_);
    }

    // Start waiting; the callback reaches this watch only while it lives.
    void start() {
        self_ = weak_from_this();
        if (!RegisterWaitForSingleObject(&wait_, process_, &ProcessWatch::exited, this, INFINITE, WT_EXECUTEONLYONCE)) {
            wait_ = nullptr;
        }
    }

    void stop() override {
        stopped_ = true;
        unregister();
    }

  private:
    // The program exited (on a wait thread of the pool): report on the reactor's thread.
    static void CALLBACK exited(void *context, BOOLEAN) noexcept {
        const auto *watch = static_cast<const ProcessWatch *>(context);
        try {
            asio::post(watch->service_.context, [self = watch->self_] {
                if (const auto live = self.lock()) {
                    live->report();
                }
            });
        } catch (...) {
            // Only exhausted memory fails posting; as in ERTS, the program cannot go on without memory.
            std::terminate();
        }
    }

    // Deliver the exit code unless the port was forgotten.
    void report() {
        if (stopped_) {
            return;
        }
        DWORD code = 0;
        GetExitCodeProcess(process_, &code);
        service_.deliver(port_, {PortInput{.kind = PortInput::Kind::status, .bytes = {}, .reason = {}, .status = code}},
                         0);
    }

    // Stop waiting; blocks until a callback in progress has posted.
    void unregister() noexcept {
        if (const auto wait = std::exchange(wait_, nullptr)) {
            UnregisterWaitEx(wait, INVALID_HANDLE_VALUE);
        }
    }

    IoService::Impl &service_;
    Word port_;
    HANDLE process_;
    // The registered wait, until the port no longer wants the status.
    HANDLE wait_ = nullptr;
    // This watch, for the callback to post without keeping it alive.
    std::weak_ptr<ProcessWatch> self_;
    // Set when the port no longer wants the status.
    bool stopped_ = false;
};
} // namespace

NativeHandle native_descriptor(int fd) noexcept { return _get_osfhandle(fd); }

PipeStream pipe_stream(asio::io_context &context, NativeHandle handle) { return {context, handle_of(handle)}; }

std::shared_ptr<Channel> start_input(IoService::Impl &service, Word port, Descriptor input) {
    if (!input.overlapped) {
        return std::make_shared<BlockingInput>(service, port, input);
    }
    auto channel = std::make_shared<PipeInput>(service, port, input);
    channel->start();
    return channel;
}

std::shared_ptr<Channel> start_child(IoService::Impl &service, Word port, Child child) {
    auto channel = std::make_shared<ProcessWatch>(service, port, child);
    channel->start();
    return channel;
}

std::string write_reason(const boost::system::error_code &error) {
    const bool closed = error == asio::error::broken_pipe ||
                        (error.category() == boost::system::system_category() && error.value() == ERROR_NO_DATA);
    return closed ? "epipe" : "eio";
}

void prepare_io() noexcept {}

std::expected<void, DriverError> write_all(int fd, std::span<const std::byte> bytes) {
    const auto handle = handle_of(_get_osfhandle(fd));
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
} // namespace clause::runtime::detail
