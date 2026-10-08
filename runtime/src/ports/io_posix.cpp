#include "io.hpp"
#include <array>
#include <cerrno>
#include <csignal>

#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

// The I/O service on Linux and macOS (docs/ports.md#io-thread): one thread polls every input descriptor and a wakeup
// pipe, reads what is ready and hands framed input to the executor.
namespace clause::runtime::detail {
namespace {
// One input being read.
struct Source final {
    Word port = 0;
    int fd = -1;
    bool owned = false;
    InputDecoder decoder;
};

class PosixIo final : public IoService {
  public:
    explicit PosixIo(Deliver deliver) : IoService(std::move(deliver)) {
        // A write to a pipe whose reader ended fails with EPIPE instead of ending the program, as in OTP.
        static_cast<void>(std::signal(SIGPIPE, SIG_IGN));
        if (pipe(wake_.data()) != 0) {
            throw std::bad_alloc();
        }
        fcntl(wake_[0], F_SETFL, fcntl(wake_[0], F_GETFL) | O_NONBLOCK);
        thread_ = std::thread([this] { loop(); });
    }

    PosixIo(const PosixIo &) = delete;
    PosixIo &operator=(const PosixIo &) = delete;
    PosixIo(PosixIo &&) = delete;
    PosixIo &operator=(PosixIo &&) = delete;

    // Stop the poll thread; owned descriptors still read are closed, the program's own stay open.
    ~PosixIo() override {
        {
            const std::scoped_lock lock(mutex_);
            stopping_ = true;
        }
        wake();
        thread_.join();
        for (const auto &[port, source] : sources_) {
            retire(*source);
        }
        close_retired();
        close(wake_[0]);
        close(wake_[1]);
    }

    void read_descriptor(Word port, Descriptor input, const PortOptions &options) override {
        {
            const std::scoped_lock lock(mutex_);
            sources_.insert_or_assign(port,
                                      std::make_shared<Source>(port, input.fd, input.owned, InputDecoder(options)));
        }
        wake();
    }

    void forget(Word port) override {
        IoService::forget(port);
        {
            const std::scoped_lock lock(mutex_);
            const auto found = sources_.find(port);
            if (found == sources_.end()) {
                return;
            }
            retire(*found->second);
            sources_.erase(found);
        }
        wake();
    }

  private:
    // Make poll() return so the thread sees changed sources or the stop.
    void wake() noexcept { static_cast<void>(!write(wake_[1], "x", 1)); }

    // Queue an owned descriptor to be closed by the poll thread, after poll() no longer uses it.
    void retire(const Source &source) {
        if (source.owned) {
            retired_.push_back(source.fd);
        }
    }

    // Close the retired descriptors; the lock is held or the thread has stopped.
    void close_retired() noexcept {
        for (const auto fd : std::exchange(retired_, {})) {
            close_descriptor(fd);
        }
    }

    // Poll the sources until stopped, delivering what each read gives.
    void loop() {
        for (;;) {
            std::vector<pollfd> fds{{wake_[0], POLLIN, 0}};
            std::vector<std::shared_ptr<Source>> polled;
            {
                const std::scoped_lock lock(mutex_);
                close_retired();
                if (stopping_) {
                    return;
                }
                for (const auto &[port, source] : sources_) {
                    fds.push_back({source->fd, POLLIN, 0});
                    polled.push_back(source);
                }
            }
            if (poll(fds.data(), fds.size(), -1) < 0 && errno != EINTR) {
                return;
            }
            drain_wake(fds[0]);
            for (std::size_t index = 0; index < polled.size(); ++index) {
                if (fds[index + 1].revents != 0) {
                    read_ready(*polled[index]);
                }
            }
        }
    }

    // Empty the wakeup pipe.
    void drain_wake(const pollfd &wake_fd) const noexcept {
        std::array<char, 64> bytes{};
        while (wake_fd.revents != 0 && read(wake_[0], bytes.data(), bytes.size()) > 0) {
        }
    }

    // Read once from a ready source and deliver its units; at end or error the source is forgotten.
    void read_ready(Source &source) {
        std::array<std::byte, std::size_t{64} * 1024> buffer{};
        const auto bytes = read(source.fd, buffer.data(), buffer.size());
        if (bytes < 0 && (errno == EINTR || errno == EAGAIN)) {
            return;
        }
        if (bytes <= 0) {
            auto units =
                bytes == 0
                    ? source.decoder.finish()
                    : std::vector{PortInput{.kind = PortInput::Kind::error, .bytes = {}, .reason = "eio", .status = 0}};
            deliver(source, std::move(units), 0);
            end(source.port);
            return;
        }
        const auto read_bytes = static_cast<std::size_t>(bytes);
        deliver(source, source.decoder.feed(std::span(buffer).first(read_bytes)), read_bytes);
    }

    // Stop polling a source whose input ended.
    void end(Word port) {
        const std::scoped_lock lock(mutex_);
        const auto found = sources_.find(port);
        if (found != sources_.end()) {
            retire(*found->second);
            sources_.erase(found);
        }
    }

    // Hand `read` bytes and their units to the executor unless the source was forgotten meanwhile.
    void deliver(const Source &source, std::vector<PortInput> units, std::size_t read) {
        {
            const std::scoped_lock lock(mutex_);
            const auto found = sources_.find(source.port);
            const bool current = found != sources_.end() && found->second.get() == &source;
            if (stopping_ || !current || (units.empty() && read == 0)) {
                return;
            }
        }
        gate_->deliver(source.port, std::move(units), read);
    }

    // The wakeup pipe: [0] polled, [1] written.
    std::array<int, 2> wake_{-1, -1};
    // Guards sources_, retired_ and stopping_.
    std::mutex mutex_;
    std::map<Word, std::shared_ptr<Source>> sources_;
    // Owned descriptors no longer read, closed by the poll thread.
    std::vector<int> retired_;
    bool stopping_ = false;
    std::thread thread_;
};
} // namespace

std::expected<void, DriverError> write_all(int fd, std::span<const std::byte> bytes) {
    while (!bytes.empty()) {
        const auto written = write(fd, bytes.data(), bytes.size());
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

void close_descriptor(int fd) noexcept { close(fd); }

std::int64_t wait_child(std::int64_t child) noexcept {
    int status = 0;
    while (waitpid(static_cast<pid_t>(child), &status, 0) < 0 && errno == EINTR) {
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 0;
}

std::unique_ptr<IoService> make_io_service(IoService::Deliver deliver) {
    return std::make_unique<PosixIo>(std::move(deliver));
}
} // namespace clause::runtime::detail
