#include "io.hpp"
#include <array>
#include <cerrno>
#include <map>
#include <mutex>
#include <thread>

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

// The I/O service on Linux and macOS (docs/ports.md#io-thread): one thread polls every input descriptor and a wakeup
// pipe, reads what is ready and hands framed input to the executor.
namespace erlang_aot::runtime::detail {
namespace {
// One input being read.
struct Source final {
    Word port = 0;
    int fd = -1;
    InputDecoder decoder;
};

class PosixIo final : public IoService {
  public:
    explicit PosixIo(Deliver deliver) : deliver_(std::move(deliver)) {
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

    // Stop the thread after any delivery in progress; descriptors the program owns stay open.
    ~PosixIo() override {
        {
            const std::scoped_lock lock(mutex_);
            stopping_ = true;
        }
        wake();
        thread_.join();
        close(wake_[0]);
        close(wake_[1]);
    }

    void read_descriptor(Word port, int fd, const PortOptions &options) override {
        {
            const std::scoped_lock lock(mutex_);
            sources_.insert_or_assign(port, std::make_shared<Source>(port, fd, InputDecoder(options)));
        }
        wake();
    }

    void forget(Word port) override {
        {
            const std::scoped_lock lock(mutex_);
            sources_.erase(port);
        }
        wake();
    }

  private:
    // Make poll() return so the thread sees changed sources or the stop.
    void wake() noexcept { static_cast<void>(!write(wake_[1], "x", 1)); }

    // Poll the sources until stopped, delivering what each read gives.
    void loop() {
        for (;;) {
            std::vector<pollfd> fds{{wake_[0], POLLIN, 0}};
            std::vector<std::shared_ptr<Source>> polled;
            {
                const std::scoped_lock lock(mutex_);
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
        std::array<std::byte, 64 * 1024> buffer{};
        const auto bytes = read(source.fd, buffer.data(), buffer.size());
        if (bytes < 0 && (errno == EINTR || errno == EAGAIN)) {
            return;
        }
        if (bytes <= 0) {
            auto units = bytes == 0
                             ? source.decoder.finish()
                             : std::vector{PortInput{.kind = PortInput::Kind::error, .bytes = {}, .reason = "eio"}};
            deliver(source, 0, std::move(units));
            forget(source.port);
            return;
        }
        const auto read_bytes = static_cast<std::size_t>(bytes);
        deliver(source, read_bytes, source.decoder.feed(std::span(buffer).first(read_bytes)));
    }

    // Hand `read` bytes and their units to the executor unless the source was forgotten meanwhile.
    void deliver(const Source &source, std::size_t read, std::vector<PortInput> units) {
        {
            const std::scoped_lock lock(mutex_);
            const auto found = sources_.find(source.port);
            const bool current = found != sources_.end() && found->second.get() == &source;
            if (stopping_ || !current || (units.empty() && read == 0)) {
                return;
            }
        }
        deliver_(source.port, std::move(units), read);
    }

    Deliver deliver_;
    // The wakeup pipe: [0] polled, [1] written.
    std::array<int, 2> wake_{-1, -1};
    // Guards sources_ and stopping_.
    std::mutex mutex_;
    std::map<Word, std::shared_ptr<Source>> sources_;
    bool stopping_ = false;
    std::thread thread_;
};
} // namespace

std::unique_ptr<IoService> make_io_service(IoService::Deliver deliver) {
    return std::make_unique<PosixIo>(std::move(deliver));
}
} // namespace erlang_aot::runtime::detail
