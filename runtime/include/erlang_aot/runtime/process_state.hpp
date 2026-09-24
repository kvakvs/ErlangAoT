#pragma once
#include <cstdint>

namespace erlang_aot::runtime {
// Preserve the proposed five scheduler classes; these are not OS priorities.
enum class ProcessPriority : std::uint8_t { idle, low, normal, high, realtime };
// Suspension is orthogonal to execution state; clearing it never wakes a waiter.
enum class ProcessState : std::uint8_t { runnable, running, waiting, exited };
// Reserve the alternate backend without implementing per-process native threads.
enum class ProcessBackend : std::uint8_t { cooperative, os_thread_placeholder };
// Carry a runtime exit category; arbitrary Erlang reason terms remain a later extension.
enum class ExitReason : std::uint8_t { normal, requested, killed, code_failure, heap_limit, runtime_shutdown };
// A future dispatch returns at a compiler-inserted cooperative safe point or completion.
enum class StepDisposition : std::uint8_t { yielded, waiting, exited };

struct StepResult final {
    // Tell the future scheduler whether to enqueue, park or reap a continuation.
    StepDisposition disposition = StepDisposition::yielded;
    // Supply the terminal reason only when disposition is exited.
    ExitReason reason = ExitReason::normal;
};
} // namespace erlang_aot::runtime
