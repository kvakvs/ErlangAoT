#include "io_streams.hpp"
#include <array>
#include <cerrno>
#include <csignal>

#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

// Port I/O on Linux and macOS (docs/ports.md#io-thread): the reactor (epoll, kqueue) waits until a descriptor can be
// read, then one read() takes what is there, so the program's own descriptors keep their blocking mode. A regular
// file cannot be waited for and is read at once. Programs are reaped on SIGCHLD.
namespace clause::runtime::detail {
namespace {
// Bytes one read asks for.
constexpr std::size_t READ_BYTES = std::size_t{64} * 1024;

// Reads one descriptor until end of input, an error or a stop; an owned descriptor is then closed, the program's own
// are only let go.
class DescriptorInput final : public Channel, public std::enable_shared_from_this<DescriptorInput> {
  public:
    DescriptorInput(IoService::Impl &service, Word port, Descriptor input, InputDecoder decoder)
        : service_(service), port_(port), stream_(service.context, static_cast<int>(input.handle)), owned_(input.owned),
          decoder_(std::move(decoder)), buffer_(READ_BYTES) {}

    // Start waiting for the first input.
    void start() { wait(); }

    void stop() override {
        stopped_ = true;
        let_go();
    }

  private:
    // Wait until the descriptor has input or an end.
    void wait() {
        stream_.async_wait(asio::posix::descriptor_base::wait_read,
                           [self = shared_from_this()](const boost::system::error_code &error) { self->ready(error); });
    }

    // The descriptor is readable, or cannot be waited for (a regular file, read at once).
    void ready(const boost::system::error_code &error) {
        if (stopped_) {
            return;
        }
        if (error && error != asio::error::operation_not_supported) {
            end({PortInput{.kind = PortInput::Kind::error, .bytes = {}, .reason = "eio", .status = 0}});
            return;
        }
        read_once(error == asio::error::operation_not_supported);
    }

    // Read what is there and deliver it, then wait again (or, for a file, read again after other work).
    void read_once(bool file) {
        const auto bytes = ::read(stream_.native_handle(), buffer_.data(), buffer_.size());
        if (bytes < 0 && (errno == EINTR || errno == EAGAIN)) {
            wait();
            return;
        }
        if (bytes <= 0) {
            end(bytes == 0 ? decoder_.finish()
                           : std::vector{
                                 PortInput{.kind = PortInput::Kind::error, .bytes = {}, .reason = "eio", .status = 0}});
            return;
        }
        const auto count = static_cast<std::size_t>(bytes);
        service_.deliver(port_, decoder_.feed(std::span(buffer_).first(count)), count);
        if (file) {
            asio::post(service_.context,
                       [self = shared_from_this()] { self->ready(asio::error::operation_not_supported); });
        } else {
            wait();
        }
    }

    // Deliver the last units, then let the descriptor go.
    void end(std::vector<PortInput> units) {
        service_.deliver(port_, std::move(units), 0);
        let_go();
    }

    // Close an owned descriptor; release the program's own without closing it.
    void let_go() noexcept {
        boost::system::error_code ignored;
        if (owned_) {
            stream_.close(ignored);
        } else if (stream_.is_open()) {
            static_cast<void>(stream_.release());
        }
    }

    IoService::Impl &service_;
    Word port_;
    asio::posix::stream_descriptor stream_;
    bool owned_;
    InputDecoder decoder_;
    std::vector<std::byte> buffer_;
    // Set when the port no longer wants input.
    bool stopped_ = false;
};

// Reaps the spawned programs of all ports on SIGCHLD and reports the status of those whose port still wants it.
class Reaper final : public Channel, public std::enable_shared_from_this<Reaper> {
  public:
    explicit Reaper(asio::io_context &context) : signals_(context, SIGCHLD) {}

    // Watch `pid` for `port`; it may have exited already.
    void add(IoService::Impl &service, pid_t pid, Word port) {
        service_ = &service;
        children_.insert_or_assign(pid, Watched{.port = port, .reporting = true});
        if (!waiting_) {
            waiting_ = true;
            wait();
        }
        reap();
    }

    // Stop reporting the program of `pid`; it is still reaped.
    void forget(pid_t pid) noexcept {
        if (const auto found = children_.find(pid); found != children_.end()) {
            found->second.reporting = false;
        }
    }

    void stop() override {}

  private:
    // A watched program: its port and whether that port still wants the status.
    struct Watched final {
        Word port = 0;
        bool reporting = true;
    };

    // Wait for the next SIGCHLD.
    void wait() {
        signals_.async_wait([self = shared_from_this()](const boost::system::error_code &error, int) {
            if (!error) {
                self->reap();
                self->wait();
            }
        });
    }

    // Reap every watched program that exited.
    void reap() {
        for (auto at = children_.begin(); at != children_.end();) {
            int status = 0;
            if (waitpid(at->first, &status, WNOHANG) <= 0) {
                ++at;
                continue;
            }
            if (at->second.reporting) {
                service_->deliver(
                    at->second.port,
                    {PortInput{
                        .kind = PortInput::Kind::status, .bytes = {}, .reason = {}, .status = exit_status(status)}},
                    0);
            }
            at = children_.erase(at);
        }
    }

    // The status as OTP reports it: the exit code, or 128 plus the signal that ended the program.
    static std::int64_t exit_status(int status) noexcept {
        if (WIFSIGNALED(status)) {
            return 128 + WTERMSIG(status);
        }
        return WIFEXITED(status) ? WEXITSTATUS(status) : 0;
    }

    asio::signal_set signals_;
    IoService::Impl *service_ = nullptr;
    std::map<pid_t, Watched> children_;
    bool waiting_ = false;
};

// A port's watch of its program in the shared reaper.
class ChildWatch final : public Channel {
  public:
    ChildWatch(std::shared_ptr<Reaper> reaper, pid_t pid) noexcept : reaper_(std::move(reaper)), pid_(pid) {}

    void stop() override { reaper_->forget(pid_); }

  private:
    std::shared_ptr<Reaper> reaper_;
    pid_t pid_;
};
} // namespace

NativeHandle native_descriptor(int fd) noexcept { return fd; }

PipeStream pipe_stream(asio::io_context &context, NativeHandle handle) { return {context, static_cast<int>(handle)}; }

std::shared_ptr<Channel> start_input(IoService::Impl &service, Word port, Descriptor input, InputDecoder decoder) {
    auto channel = std::make_shared<DescriptorInput>(service, port, input, std::move(decoder));
    channel->start();
    return channel;
}

std::shared_ptr<Channel> start_child(IoService::Impl &service, Word port, Child child) {
    if (!service.shared) {
        service.shared = std::make_shared<Reaper>(service.context);
    }
    auto reaper = std::static_pointer_cast<Reaper>(service.shared);
    const auto pid = static_cast<pid_t>(child.handle);
    reaper->add(service, pid, port);
    return std::make_shared<ChildWatch>(std::move(reaper), pid);
}

std::string write_reason(const boost::system::error_code &error) {
    return error == asio::error::broken_pipe ? "epipe" : "eio";
}

void prepare_io() noexcept {
    // A write to a pipe whose reader ended fails with EPIPE instead of ending the program, as in OTP.
    static_cast<void>(std::signal(SIGPIPE, SIG_IGN));
}

std::expected<void, DriverError> write_all(int fd, std::span<const std::byte> bytes) {
    while (!bytes.empty()) {
        const auto written = write(fd, bytes.data(), bytes.size());
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written < 0 && errno == EAGAIN) {
            // Another holder of the descriptor made it non-blocking: wait until it takes more.
            pollfd ready{fd, POLLOUT, 0};
            static_cast<void>(poll(&ready, 1, -1));
            continue;
        }
        if (written < 0) {
            return std::unexpected(DriverError{errno == EPIPE ? "epipe" : (errno == EBADF ? "ebadf" : "eio")});
        }
        bytes = bytes.subspan(static_cast<std::size_t>(written));
    }
    return {};
}
} // namespace clause::runtime::detail
