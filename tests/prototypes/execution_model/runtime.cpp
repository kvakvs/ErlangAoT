// Step-17 decision prototype: runtime services, a round-robin scheduler and the measured scenarios for the
// chosen model (flat process stack, continuation indices, tail transfers).
#include "model.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace prototype {
namespace {
Result exit_body(Process *process);

// Bottom frame of every process: continuation 1 is a normal return, 2 an uncaught exception.
const Descriptor EXIT{nullptr, exit_body, "exit", 0, 0};

Result exit_body(Process *process) {
    process->exited = true;
    process->crashed = frame(process)->resume == 2;
    SUSPEND();
}

// Make room for `words` more stack words, moving the whole stack when it is full.
void reserve(Process *process, size_t words) {
    if (process->top + words <= process->capacity) {
        return;
    }
    size_t capacity = std::max(process->capacity * 2, process->top + words);
    auto *moved = static_cast<Word *>(std::realloc(process->stack, capacity * sizeof(Word)));
    if (moved == nullptr) {
        std::abort();
    }
    process->moves += moved != process->stack ? 1 : 0;
    process->stack = moved;
    process->capacity = capacity;
}

// Push a zeroed frame for `function`; returns its slots.
Word *push(Process *process, const Descriptor *function) {
    size_t words = HEADER_WORDS + function->slots;
    reserve(process, words);
    Word *start = process->stack + process->top;
    std::memset(start, 0, words * sizeof(Word));
    auto *header = reinterpret_cast<Frame *>(start);
    header->previous = process->frame;
    header->function = function;
    process->frame = process->top;
    process->top += words;
    process->peak = std::max(process->peak, process->top);
    return start + HEADER_WORDS;
}
} // namespace

Code call(Process *process, const Descriptor *function) {
    if (tick(process, function->entry)) {
        return park;
    }
    Word *roots = push(process, function);
    std::copy_n(process->x, function->arity, roots);
    return function->body;
}

Code leave(Process *process) {
    process->top = process->frame;
    process->frame = frame(process)->previous;
    return caller(process);
}

Code raise(Process *process, Word reason) {
    process->failed = true;
    process->reason = reason;
    size_t named = 0;
    for (size_t at = process->frame; named < TRACE_DEPTH;) {
        const auto *header = reinterpret_cast<const Frame *>(process->stack + at);
        if (header->function == &EXIT) {
            break;
        }
        process->trace[named++] = header->function->name;
        at = header->previous;
    }
    while (frame(process)->handler == 0) {
        leave(process);
    }
    frame(process)->resume = frame(process)->handler;
    return caller(process);
}

Word exception(Process *process) {
    process->failed = false;
    return process->reason;
}

bool tick(Process *process, Code entry) {
    if (--process->reductions >= 0) {
        return false;
    }
    process->resume_at = entry;
    ++process->yields;
    return true;
}

Result park(Process *) { SUSPEND(); }

void probe(Process *process) {
    volatile char marker = 0;
    process->deepest = std::min(process->deepest, reinterpret_cast<uintptr_t>(&marker));
}
} // namespace prototype

namespace {
using namespace prototype;

struct Scheduler {
    // Native stack address of the scheduler loop, the base every probe is measured from.
    uintptr_t base = 0;
    long budget = 4000;
};

// Start a process at `entry` with up to two arguments above a bottom exit frame.
void spawn(Process &process, Code entry, Word first, Word second) {
    process = Process{};
    process.deepest = UINTPTR_MAX;
    // Offset 0 is an unused word, so no real frame starts there; the bottom exit frame follows it.
    process.stack = static_cast<Word *>(std::calloc(1, sizeof(Word)));
    process.capacity = 1;
    process.top = 1;
    push(&process, &EXIT);
    frame(&process)->resume = 1;
    frame(&process)->handler = 2;
    process.x[0] = first;
    process.x[1] = second;
    process.resume_at = entry;
}

// Run one time slice: resume the process and return when it suspends or exits.
void run_slice(Scheduler &scheduler, Process &process) {
    volatile char marker = 0;
    scheduler.base = reinterpret_cast<uintptr_t>(&marker);
    process.reductions = scheduler.budget;
    Code code = process.resume_at;
#if defined(PROTOTYPE_TRAMPOLINE)
    while (code != nullptr) {
        code = code(&process).next;
    }
#else
    code(&process);
#endif
}

// Run processes round-robin until all have exited; return the number of slices.
size_t run_all(Scheduler &scheduler, Process *processes, size_t count) {
    size_t slices = 0;
    for (size_t done = 0; done < count;) {
        done = 0;
        for (size_t at = 0; at < count; ++at) {
            if (!processes[at].exited) {
                run_slice(scheduler, processes[at]);
                ++slices;
            }
            done += processes[at].exited ? 1 : 0;
        }
    }
    return slices;
}

// Run one scenario process, print its outcome and measurements and check the expected result.
bool scenario(const char *name, Code entry, Word first, Word second, Word expected, bool crash) {
    Scheduler scheduler;
    Process process{};
    spawn(process, entry, first, second);
    auto started = std::chrono::steady_clock::now();
    size_t slices = run_all(scheduler, &process, 1);
    auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    Word value = crash ? process.reason : process.x[0];
    bool ok = process.crashed == crash && value == expected;
    long long native = process.deepest == UINTPTR_MAX ? 0 : static_cast<long long>(scheduler.base - process.deepest);
    std::printf("%-10s %s value=%llu slices=%zu stack_moves=%zu peak_words=%zu native_bytes=%lld ms=%.1f\n", name,
                ok ? "ok  " : "FAIL", static_cast<unsigned long long>(value), slices, process.moves, process.peak,
                native, elapsed);
    if (crash) {
        std::printf("           trace:");
        for (const char *frame_name : process.trace) {
            std::printf(" %s", frame_name != nullptr ? frame_name : "-");
        }
        std::printf("\n");
    }
    std::free(process.stack);
    return ok;
}

// Two processes share one scheduler; both must finish with interleaved slices.
bool interleave() {
    Scheduler scheduler;
    Process processes[2]{};
    spawn(processes[0], sum, 1'000'000, 0);
    spawn(processes[1], loop, 3'000'000, 0);
    size_t slices = run_all(scheduler, processes, 2);
    bool ok = processes[0].x[0] == 500'000'500'000ULL && processes[1].x[0] == 3'000'000 && !processes[0].crashed &&
              !processes[1].crashed;
    std::printf("%-10s %s slices=%zu yields=%zu+%zu\n", "interleave", ok ? "ok  " : "FAIL", slices, processes[0].yields,
                processes[1].yields);
    std::free(processes[0].stack);
    std::free(processes[1].stack);
    return ok;
}
} // namespace

int main() {
#if defined(PROTOTYPE_TRAMPOLINE)
    std::printf("model: explicit frames, trampoline transfers, word %zu bits\n", sizeof(Word) * 8);
#else
    std::printf("model: explicit frames, musttail transfers, word %zu bits\n", sizeof(Word) * 8);
#endif
    bool ok = scenario("return", answer, 0, 0, 42, false);
    ok = scenario("recursion", sum, 1'000'000, 0, 500'000'500'000ULL, false) && ok;
    ok = scenario("tail", loop, 10'000'000, 0, 10'000'000, false) && ok;
    ok = scenario("caught", catcher, 100'000, 0, BOOM, false) && ok;
    ok = scenario("uncaught", fail, 100'000, 0, BOOM, true) && ok;
    ok = interleave() && ok;
    return ok ? 0 : 1;
}
