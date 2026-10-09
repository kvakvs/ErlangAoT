#pragma once
#include <atomic>
#include <chrono>
#include <clause/abi/modules.hpp>
#include <clause/runtime/base_types.hpp>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// Opt-in profiling (docs/profiling.md): entries (reductions) and self time of every Erlang function, per process
// and for the whole program, attributed where control moves between frames. Builtins and runtime services count
// as time of the function that called them.
namespace clause::runtime::detail {
// What one function cost: how often it was entered and the time its own code (and the services it called) ran.
struct FunctionCost final {
    std::uint64_t entries = 0;
    std::uint64_t nanoseconds = 0;
};

using FunctionCosts = std::unordered_map<const abi::v1::FrameDescriptor *, FunctionCost>;

// What the executor did in one run: time slices run and clock readings of its timer thread.
struct SchedulerCounts final {
    std::uint64_t slices = 0;
    std::uint64_t clock_reads = 0;
};

// The profile of one process, kept by its stack while it runs; only that process's worker touches it.
class ProcessProfile final {
  public:
    // Count an entry of `function`, which runs from now on.
    void enter(const abi::v1::FrameDescriptor &function) noexcept;
    // Charge the time so far to the running function; `function` runs from now on (null: no clock until resumed).
    void run(const abi::v1::FrameDescriptor *function) noexcept;

    const FunctionCosts &functions() const noexcept { return functions_; }

    // Samples dropped because recording them ran out of memory.
    std::uint64_t lost() const noexcept { return lost_; }

  private:
    using Clock = std::chrono::steady_clock;
    // Costs by function descriptor; growth failures drop a sample instead of failing the program.
    FunctionCosts functions_;
    // The function running since `since_`; null between time slices.
    const abi::v1::FrameDescriptor *running_ = nullptr;
    Clock::time_point since_;
    std::uint64_t lost_ = 0;
};

// The program's profile: the costs of every ended process, merged by the runtime under a lock.
class RuntimeProfile final {
  public:
    // Merge the profile of process `pid` when its context is destroyed.
    void add(Word pid, const ProcessProfile &profile) noexcept;
    // Record the executor's time slices and timer-thread clock readings of the run (descriptive).
    void scheduling(const SchedulerCounts &counts) noexcept;
    // Functions by self time, then processes by time, as text (docs/profiling.md#report).
    std::string report() const;

  private:
    // One ended process: its pid number, totals and its most expensive function.
    struct ProcessRecord final {
        Word pid = 0;
        FunctionCost total;
        const abi::v1::FrameDescriptor *top = nullptr;
    };

    // Guards the merged costs: processes end on any worker.
    mutable std::mutex mutex_;
    FunctionCosts functions_;
    std::vector<ProcessRecord> processes_;
    // Samples and processes lost to memory exhaustion; atomic, since a failed merge counts outside the lock.
    std::atomic<std::uint64_t> lost_ = 0;
    // Time slices and timer clock readings of the last run.
    SchedulerCounts scheduling_;
};
} // namespace clause::runtime::detail
