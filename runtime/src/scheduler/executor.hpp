#pragma once
#include <deque>
#include <erlang_aot/abi/frames.hpp>
#include <erlang_aot/runtime/process_context.hpp>
#include <unordered_set>

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
    // Deliver `message` of `sender` to the process of a pid word this runtime issued: copied into the receiver's
    // heap and appended to its signal inbox, waking it when it waits. A process that has ended gets nothing; the
    // error is a failed copy.
    static TermResult<void> send(ProcessContext &sender, Word pid, const Term &message);

    // Run queued processes until `main` ends or another process halts or fails outside Erlang; return the process
    // whose outcome ends the program. Processes ending before it are released. When every process waits for a
    // message that nothing can send, the program blocks forever, as OTP's does.
    ProcessContext &run(ProcessContext &main) noexcept;
    // Release every queued and waiting process without running it further.
    void clear() noexcept;

  private:
    // Run the first queued process for one time slice; the process whose end ends the program, or null.
    ProcessContext *slice(ProcessContext &main) noexcept;
    // Create a process of the parent's runtime, let `prepare` load its first call into its registers (the frame,
    // or null after recording the error the process ends with) and queue it.
    template <typename Prepare> TermResult<Term> create(ProcessContext &parent, Prepare prepare) noexcept;
    // Put a process that yielded back at the end of the queue; false after recording the allocation failure.
    bool requeue(ProcessContext &process) noexcept;
    // Keep a process that waits for a message out of the queue until a send wakes it; false after recording the
    // allocation failure.
    bool park(ProcessContext &process) noexcept;
    // Run a parked receiver again after a message arrived for it.
    TermResult<void> wake(ProcessContext &receiver);

    // Runnable processes in the order they run; a running process is in none of them.
    std::deque<ProcessContext *> queue_;
    // Processes waiting for a message.
    std::unordered_set<ProcessContext *> parked_;
};
} // namespace erlang_aot::runtime::detail
