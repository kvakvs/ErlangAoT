// Step-17 decision prototype, rejected alternative: every Erlang function is an LLVM coroutine (clang lowers
// C++20 coroutines through the LLVM coro intrinsics). Calls await the callee with symmetric transfer.
#include <algorithm>
#include <chrono>
#include <coroutine>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <new>
#include <utility>

namespace {
using Word = std::uintptr_t;
constexpr Word BOOM = 0xB00;

// Coroutine frame accounting: every activation is a separate allocation whose layout LLVM chooses.
std::size_t allocations = 0;
std::size_t live_frames = 0;
std::size_t peak_frames = 0;
std::size_t frame_bytes = 0;

struct Process {
    long reductions = 0;
    std::coroutine_handle<> resume;
    bool failed = false;
    Word reason = 0;
    std::size_t yields = 0;
    std::uintptr_t deepest = UINTPTR_MAX;
};

class Task {
  public:
    struct promise_type;
    using Handle = std::coroutine_handle<promise_type>;

    struct Final {
        bool await_ready() noexcept { return false; }

        // Return to the awaiting caller by tail transfer, or to the scheduler for the root task.
        std::coroutine_handle<> await_suspend(Handle self) noexcept {
            auto caller = self.promise().caller;
            return caller ? caller : std::noop_coroutine();
        }

        void await_resume() noexcept {}
    };

    struct promise_type {
        Word value = 0;
        std::coroutine_handle<> caller;

        static void *operator new(std::size_t bytes) {
            ++allocations;
            peak_frames = std::max(peak_frames, ++live_frames);
            frame_bytes = bytes;
            return ::operator new(bytes);
        }

        static void operator delete(void *frame) noexcept {
            --live_frames;
            ::operator delete(frame);
        }

        Task get_return_object() { return Task{Handle::from_promise(*this)}; }

        std::suspend_always initial_suspend() noexcept { return {}; }

        Final final_suspend() noexcept { return {}; }

        void return_value(Word result) { value = result; }

        void unhandled_exception() { std::terminate(); }
    };

    explicit Task(Handle handle) : handle_(handle) {}

    Task(Task &&other) noexcept : handle_(std::exchange(other.handle_, {})) {}

    Task(const Task &) = delete;
    Task &operator=(const Task &) = delete;
    Task &operator=(Task &&) = delete;

    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }

    bool await_ready() noexcept { return false; }

    // Start the callee by symmetric transfer; it resumes the caller from its final suspend.
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) noexcept {
        handle_.promise().caller = caller;
        return handle_;
    }

    Word await_resume() noexcept { return handle_.promise().value; }

    Handle handle() const noexcept { return handle_; }

  private:
    // The callee's frame, destroyed with the awaiting expression.
    Handle handle_;
};

// Count one reduction; at zero suspend the whole chain back to the scheduler.
struct Tick {
    Process &process;

    bool await_ready() noexcept { return --process.reductions >= 0; }

    void await_suspend(std::coroutine_handle<> self) noexcept {
        process.resume = self;
        ++process.yields;
    }

    void await_resume() noexcept {}
};

void probe(Process &process) {
    volatile char marker = 0;
    process.deepest = std::min(process.deepest, reinterpret_cast<std::uintptr_t>(&marker));
}

// sum(0) -> 0; sum(N) -> N + sum(N - 1).
Task sum(Process &process, Word n) {
    co_await Tick{process};
    if (n == 0) {
        probe(process);
        co_return 0;
    }
    Word result = co_await sum(process, n - 1);
    co_return process.failed ? 0 : result + n;
}

// loop(0, Acc) -> Acc; loop(N, Acc) -> loop(N - 1, Acc + 1).   No tail call: every level keeps its frame.
Task loop(Process &process, Word n, Word acc) {
    co_await Tick{process};
    if (n == 0) {
        co_return acc;
    }
    co_return co_await loop(process, n - 1, acc + 1);
}

// fail(0) -> error(boom); fail(N) -> 1 + fail(N - 1).
Task fail(Process &process, Word n) {
    co_await Tick{process};
    if (n == 0) {
        process.failed = true;
        process.reason = BOOM;
        co_return 0;
    }
    Word result = co_await fail(process, n - 1);
    co_return process.failed ? 0 : result + 1;
}

// catcher(N) -> try fail(N) catch error:R -> R end.
Task catcher(Process &process, Word n) {
    Word result = co_await fail(process, n);
    if (process.failed) {
        process.failed = false;
        co_return process.reason;
    }
    co_return result;
}

// Run a root task in slices of 4000 reductions until it completes.
bool scenario(const char *name, Process &process, Task task, Word expected) {
    allocations = peak_frames = 0;
    volatile char marker = 0;
    auto base = reinterpret_cast<std::uintptr_t>(&marker);
    auto started = std::chrono::steady_clock::now();
    process.resume = task.handle();
    while (!task.handle().done()) {
        process.reductions = 4000;
        process.resume.resume();
    }
    auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    Word value = task.handle().promise().value;
    long long native = process.deepest == UINTPTR_MAX ? 0 : static_cast<long long>(base - process.deepest);
    bool ok = value == expected;
    std::printf("%-10s %s value=%llu yields=%zu frames=%zu peak_frames=%zu frame_bytes=%zu native_bytes=%lld "
                "ms=%.1f\n",
                name, ok ? "ok  " : "FAIL", static_cast<unsigned long long>(value), process.yields, allocations,
                peak_frames, frame_bytes, native, elapsed);
    return ok;
}
} // namespace

int main() {
    std::printf("model: LLVM coroutines (C++20), word %zu bits\n", sizeof(Word) * 8);
    Process first;
    bool ok = scenario("recursion", first, sum(first, 1'000'000), 500'000'500'000ULL);
    Process second;
    ok = scenario("tail", second, loop(second, 1'000'000, 0), 1'000'000) && ok;
    Process third;
    ok = scenario("caught", third, catcher(third, 100'000), BOOM) && ok;
    return ok ? 0 : 1;
}
