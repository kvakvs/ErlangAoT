#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <iostream>
#include <map>
#include <mutex>
#include <ports/io.hpp>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <tlhelp32.h>
#else
#include <filesystem>
#include <sys/resource.h>
#include <unistd.h>
#endif

// The port I/O of one runtime (docs/ports.md#io-thread) serves thousands of pipes from the reactor's one thread:
// while every pipe is open the program has no more threads than before, and each pipe's output arrives at its
// reader, followed by the end of input. A program cannot count its threads, so this is a runtime test.
namespace {
using namespace clause::runtime;
using detail::Descriptor;
using detail::IoService;
using detail::NativeHandle;
using detail::PortInput;
using detail::Reactor;

// Keep every check active in optimized builds.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Pipes open at once: thousands, within the descriptor limit of the host.
constexpr std::size_t WANTED_PIPES = 2'000;
// Threads the host may add meanwhile on its own (thread pool and loader workers).
constexpr std::size_t SYSTEM_THREADS = 16;

// A pipe: the end the service writes and the end it reads, both owned by the service.
struct Pipe final {
    NativeHandle write = -1;
    NativeHandle read = -1;
};

#if defined(_WIN32)
// The threads of this program.
std::size_t thread_count() {
    const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 entry{};
    entry.dwSize = sizeof(THREADENTRY32);
    std::size_t count = 0;
    for (auto found = Thread32First(snapshot, &entry); found; found = Thread32Next(snapshot, &entry)) {
        count += entry.th32OwnerProcessID == GetCurrentProcessId() ? 1 : 0;
    }
    CloseHandle(snapshot);
    return count;
}

// How many pipes to open.
std::size_t pipe_count() { return WANTED_PIPES; }

// A named pipe with both ends overlapped, as the runtime makes them for spawned programs.
Pipe make_pipe(std::size_t index) {
    const auto name =
        L"\\\\.\\pipe\\clause-port-io-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(index);
    const auto server = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED,
                                         PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, nullptr);
    const auto client =
        CreateFileW(name.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    require(server != INVALID_HANDLE_VALUE && client != INVALID_HANDLE_VALUE, "pipe creation failed");
    return {reinterpret_cast<NativeHandle>(server), reinterpret_cast<NativeHandle>(client)};
}
#else
// The threads of this program; 0 where they cannot be listed (macOS), which skips the check.
std::size_t thread_count() {
    std::error_code error;
    std::size_t count = 0;
    for (std::filesystem::directory_iterator task("/proc/self/task", error), end; !error && task != end; ++task) {
        ++count;
    }
    return count;
}

// How many pipes to open: two descriptors each, within the raised descriptor limit.
std::size_t pipe_count() {
    rlimit limit{};
    getrlimit(RLIMIT_NOFILE, &limit);
    limit.rlim_cur = limit.rlim_max;
    setrlimit(RLIMIT_NOFILE, &limit);
    getrlimit(RLIMIT_NOFILE, &limit);
    return std::min<std::size_t>(WANTED_PIPES, (limit.rlim_cur - 64) / 2);
}

Pipe make_pipe(std::size_t) {
    int ends[2] = {-1, -1};
    require(pipe(ends) == 0, "pipe creation failed");
    return {ends[1], ends[0]};
}
#endif

// What arrived for each reading port: its bytes, and whether its input ended.
struct Received final {
    std::mutex mutex;
    std::condition_variable changed;
    std::map<Word, std::string> bytes;
    std::size_t ended = 0;
};

// The text the writing port of pipe `index` sends.
std::string text_of(std::size_t index) { return "pipe " + std::to_string(index) + "\n"; }

void serve_many_pipes() {
    const auto pipes = pipe_count();
    Received received;
    Reactor reactor;
    IoService service(reactor, [&](Word port, std::vector<PortInput> units, std::size_t) {
        const std::scoped_lock lock(received.mutex);
        for (const auto &unit : units) {
            if (unit.kind == PortInput::Kind::data) {
                received.bytes[port].append(reinterpret_cast<const char *>(unit.bytes.data()), unit.bytes.size());
            } else {
                received.ended += unit.kind == PortInput::Kind::end ? 1 : 0;
            }
        }
        received.changed.notify_all();
    });
    // Ports 2i write pipe i, ports 2i + 1 read it.
    const auto before = thread_count();
    for (std::size_t index = 0; index < pipes; ++index) {
        const auto pipe = make_pipe(index);
        service.write_descriptor(2 * index, Descriptor{.handle = pipe.write, .owned = true, .overlapped = true});
        service.read_descriptor(2 * index + 1, Descriptor{.handle = pipe.read, .owned = true, .overlapped = true});
    }
    const auto during = thread_count();
    // The system starts and retires a few pool threads of its own; a thread per port would add thousands.
    require(before == 0 || during <= before + SYSTEM_THREADS, "pipe ports started threads of their own");
    for (std::size_t index = 0; index < pipes; ++index) {
        const auto text = text_of(index);
        const auto *begin = reinterpret_cast<const std::byte *>(text.data());
        service.send(2 * index, {begin, begin + text.size()});
        // A forgotten writer still writes what is queued, then closes its end: the reader sees the end of input.
        service.forget(2 * index);
    }
    std::unique_lock lock(received.mutex);
    const auto done =
        received.changed.wait_for(lock, std::chrono::seconds(60), [&] { return received.ended == pipes; });
    lock.unlock();
    reactor.stop();
    require(done, "not every pipe's input ended");
    for (std::size_t index = 0; index < pipes; ++index) {
        require(received.bytes[2 * index + 1] == text_of(index), "a pipe delivered other bytes");
    }
    std::cout << pipes << " pipes, " << before << " threads before, " << during << " while open\n";
}
} // namespace

int main() {
    try {
        serve_many_pipes();
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "port_io: " << error.what() << '\n';
        return 1;
    }
}
