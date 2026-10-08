// Plan 11 step 57A prototype (docs/ports.md#io-thread): one I/O thread waits for pipe input with the platform
// mechanism the runtime will use (an I/O completion port on Windows, poll() elsewhere), queues each read as an
// event under the scheduler's mutex and wakes an idle scheduler thread through its condition variable. Shutdown
// wakes the I/O thread without input. Prints one line per event and the wakeup latency; exits non-zero on failure.
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#endif

namespace {
using Clock = std::chrono::steady_clock;

// The scheduler side: events queued by the I/O thread and the condition variable idle workers sleep on.
struct Scheduler {
    std::mutex mutex;
    std::condition_variable work;
    std::deque<std::pair<std::string, Clock::time_point>> events;
    bool closed = false;

    // Called by the I/O thread: queue an event and wake one idle worker.
    void post(std::string data) {
        {
            const std::scoped_lock lock(mutex);
            events.emplace_back(std::move(data), Clock::now());
        }
        work.notify_one();
    }

    // Called by the I/O thread when its source ended.
    void close() {
        {
            const std::scoped_lock lock(mutex);
            closed = true;
        }
        work.notify_one();
    }
};

#ifdef _WIN32
// A named pipe pair: the read end is overlapped and bound to the completion port.
struct Pipe {
    HANDLE read = INVALID_HANDLE_VALUE;
    HANDLE write = INVALID_HANDLE_VALUE;
};

Pipe make_pipe() {
    const auto name = "\\\\.\\pipe\\erlang_aot_poller_" + std::to_string(GetCurrentProcessId());
    Pipe pipe;
    pipe.read =
        CreateNamedPipeA(name.c_str(), PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                         PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, nullptr);
    pipe.write = CreateFileA(name.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe.read == INVALID_HANDLE_VALUE || pipe.write == INVALID_HANDLE_VALUE) {
        std::fprintf(stderr, "pipe: %lu\n", GetLastError());
        std::exit(2);
    }
    return pipe;
}

constexpr ULONG_PTR PIPE_KEY = 1;
constexpr ULONG_PTR STOP_KEY = 2;

// The I/O thread: one overlapped read at a time; a completion with STOP_KEY ends it.
void io_thread(HANDLE port, HANDLE read, Scheduler &scheduler) {
    char buffer[256];
    OVERLAPPED overlapped{};
    const auto start_read = [&] {
        if (!ReadFile(read, buffer, sizeof(buffer), nullptr, &overlapped) && GetLastError() != ERROR_IO_PENDING) {
            scheduler.close();
            return false;
        }
        return true;
    };
    if (!start_read()) {
        return;
    }
    for (;;) {
        DWORD bytes = 0;
        ULONG_PTR key = 0;
        OVERLAPPED *done = nullptr;
        const BOOL ok = GetQueuedCompletionStatus(port, &bytes, &key, &done, INFINITE);
        if (key == STOP_KEY) {
            CancelIoEx(read, &overlapped);
            return;
        }
        if (!ok || bytes == 0) {
            scheduler.close();
            return;
        }
        scheduler.post(std::string(buffer, bytes));
        if (!start_read()) {
            return;
        }
    }
}
#else
// The I/O thread: poll() on the nonblocking read end and on a wakeup pipe that stops it.
void io_thread(int read_fd, int wake_fd, Scheduler &scheduler) {
    for (;;) {
        pollfd fds[2] = {{read_fd, POLLIN, 0}, {wake_fd, POLLIN, 0}};
        if (poll(fds, 2, -1) < 0 && errno != EINTR) {
            scheduler.close();
            return;
        }
        if (fds[1].revents != 0) {
            return;
        }
        if (fds[0].revents == 0) {
            continue;
        }
        char buffer[256];
        const auto bytes = read(read_fd, buffer, sizeof(buffer));
        if (bytes <= 0 && !(bytes < 0 && errno == EAGAIN)) {
            scheduler.close();
            return;
        }
        if (bytes > 0) {
            scheduler.post(std::string(buffer, static_cast<std::size_t>(bytes)));
        }
    }
}
#endif

// Write three messages with pauses, as an external program would.
template <typename Write> void writer(Write write) {
    for (const char *text : {"one", "two", "three"}) {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        write(text);
    }
}

// The scheduler thread sleeps until events arrive; returns how many it took.
int schedule(Scheduler &scheduler, int expected) {
    int seen = 0;
    std::unique_lock lock(scheduler.mutex);
    while (seen < expected) {
        scheduler.work.wait(lock, [&] { return !scheduler.events.empty() || scheduler.closed; });
        while (!scheduler.events.empty()) {
            const auto [data, posted] = scheduler.events.front();
            scheduler.events.pop_front();
            const auto latency = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - posted);
            std::printf("event %s, wakeup %lld us\n", data.c_str(), static_cast<long long>(latency.count()));
            ++seen;
        }
        if (scheduler.closed) {
            break;
        }
    }
    return seen;
}
} // namespace

int main() {
    Scheduler scheduler;
#ifdef _WIN32
    const auto pipe = make_pipe();
    HANDLE port = CreateIoCompletionPort(pipe.read, nullptr, PIPE_KEY, 1);
    std::thread io([&] { io_thread(port, pipe.read, scheduler); });
    std::thread source([&] {
        writer([&](const char *text) {
            DWORD written = 0;
            WriteFile(pipe.write, text, static_cast<DWORD>(std::char_traits<char>::length(text)), &written, nullptr);
        });
    });
    const int seen = schedule(scheduler, 3);
    source.join();
    PostQueuedCompletionStatus(port, 0, STOP_KEY, nullptr);
    io.join();
    CloseHandle(pipe.write);
    CloseHandle(pipe.read);
    CloseHandle(port);
#else
    int data[2];
    int wake[2];
    if (pipe(data) != 0 || pipe(wake) != 0) {
        return 2;
    }
    fcntl(data[0], F_SETFL, fcntl(data[0], F_GETFL) | O_NONBLOCK);
    std::thread io([&] { io_thread(data[0], wake[0], scheduler); });
    std::thread source(
        [&] { writer([&](const char *text) { (void)!write(data[1], text, std::char_traits<char>::length(text)); }); });
    const int seen = schedule(scheduler, 3);
    source.join();
    (void)!write(wake[1], "x", 1);
    io.join();
#endif
    std::printf("%s: %d events, I/O thread stopped\n", seen == 3 ? "ok" : "FAIL", seen);
    return seen == 3 ? 0 : 1;
}
