#pragma once
#include <deque>
#include <erlang_aot/abi/frames.hpp>
#include <erlang_aot/runtime/process_context.hpp>

// The cooperative executor of one runtime (docs/processes.md): a first-in, first-out queue of runnable processes,
// each run for a time slice of reductions on the thread that started the program.
namespace erlang_aot::runtime::detail {
// The call a process spawned by spawn/3 starts with: Module:Function(Arguments...).
struct InitialCall final {
    // Atoms naming the function the new process calls.
    Term module;
    Term function;
    // A proper list of the arguments.
    Term arguments;
};

class Executor final {
  public:
    // The executor of a context's runtime.
    static Executor &of(ProcessContext &context) noexcept;

    // Queue `process` to start with a call of `function` on the arguments already in its registers; false after
    // recording the failure in its channel.
    bool start(ProcessContext &process, const abi::v1::FrameDescriptor &function) noexcept;
    // spawn(Fun) of `parent`: a new process calls Fun(). Its call is prepared in the new process, so a fun of
    // another arity crashes it, not the parent. The parent gets the new pid, or the failure to create it.
    TermResult<Term> spawn(ProcessContext &parent, const Term &fun) noexcept;
    // spawn(M, F, Args) of `parent`: a new process calls M:F(Args...), crashing with undef when nothing exports it.
    TermResult<Term> spawn(ProcessContext &parent, const InitialCall &call) noexcept;
    // Whether the process of a pid word this runtime issued has not ended.
    static bool alive(ProcessContext &context, Word pid) noexcept;

    // Run queued processes until `main` ends or another process halts or fails outside Erlang; return the process
    // whose outcome ends the program. Processes ending before it are released.
    ProcessContext &run(ProcessContext &main) noexcept;
    // Release every queued process without running it further.
    void clear() noexcept;

  private:
    // Create a process of the parent's runtime, let `prepare` load its first call into its registers (the frame,
    // or null after recording the error the process ends with) and queue it.
    template <typename Prepare> TermResult<Term> create(ProcessContext &parent, Prepare prepare) noexcept;
    // Put a process that yielded back at the end of the queue; false after recording the allocation failure.
    bool requeue(ProcessContext &process) noexcept;

    // Runnable processes in the order they run; a running process is in none of them.
    std::deque<ProcessContext *> queue_;
};
} // namespace erlang_aot::runtime::detail
