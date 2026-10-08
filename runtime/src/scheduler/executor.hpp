#pragma once
#include <chrono>
#include <deque>
#include <erlang_aot/abi/frames.hpp>
#include <erlang_aot/runtime/process_context.hpp>
#include <map>
#include <unordered_set>
#include <vector>

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

// The pid word of a process.
Word pid_of(ProcessContext &process) noexcept;
// Whether an ended process ends the whole program: a halt or a failure outside Erlang, not an Erlang exception or an
// exit signal.
bool ends_program(ProcessContext &process) noexcept;

class Executor final {
  public:
    // The executor of a context's runtime.
    static Executor &of(ProcessContext &context) noexcept;

    // Queue `process` to start with a call of `function` on the arguments already in its registers; false after
    // recording the failure in its channel.
    bool start(ProcessContext &process, const abi::v1::FrameDescriptor &function) noexcept;
    // spawn(Fun) of `parent`: a new process calls Fun(). Its call is prepared in the new process, so a fun of
    // another arity crashes it, not the parent. The parent gets the new pid, or the failure to create it. With
    // `link` (spawn_link/1) both are linked before the new process runs.
    TermResult<Term> spawn(ProcessContext &parent, const Term &fun, bool link = false) noexcept;
    // spawn(M, F, Args) of `parent`: a new process calls M:F(Args...), crashing with undef when nothing exports it.
    TermResult<Term> spawn(ProcessContext &parent, const InitialCall &call, bool link = false) noexcept;
    // Whether the process of a pid word this runtime issued has not ended.
    static bool alive(ProcessContext &context, Word pid) noexcept;
    // Deliver `message` of `sender` to the process of a pid word this runtime issued: copied into the receiver's
    // heap and appended to its signal inbox, waking it when it waits. A process that has ended gets nothing; the
    // error is a failed copy.
    static TermResult<void> send(ProcessContext &sender, Word pid, const Term &message);

    // link(Pid) of the running `process` (docs/processes.md#links). For an ended process it sends itself
    // {'EXIT', Pid, noproc} when trapping exits; otherwise it returns false and the caller raises noproc.
    // Allocation failure throws.
    bool link(ProcessContext &process, Word pid);
    // unlink(Pid) of `process`: the link has no effect from now on.
    static void unlink(ProcessContext &process, Word pid) noexcept;
    // monitor(process, Pid) of the running `watcher`: a new reference. For an ended process the watcher gets
    // {'DOWN', Ref, process, Pid, noproc} at once; monitoring itself creates nothing. Allocation failure throws.
    Term monitor(ProcessContext &watcher, Word pid);
    // demonitor(Ref) of `watcher`: whether the monitor was active; no 'DOWN' of it arrives afterwards.
    static bool demonitor(ProcessContext &watcher, const ReferenceIdentity &reference) noexcept;
    // An exit signal of exit/2 or exit_signal/2 from the running `sender` to the process of a pid word, acted on at
    // once; `self_normal` is exit/2's quirk: reason normal sent to itself ends the sender. Allocation failure throws.
    void exit(ProcessContext &sender, Word pid, const Term &reason, bool self_normal);

    // Run queued processes until `main` ends (also by an exit signal) or another process halts or fails outside
    // Erlang; return the process whose outcome ends the program. Processes ending before it are released. Waiting
    // processes whose receive timeout expires are queued again; when every process waits without one, the program
    // blocks forever, as OTP's does.
    ProcessContext &run(ProcessContext &main) noexcept;
    // Release every queued, waiting and ended process except the one run() returned, without running it further.
    void clear() noexcept;

  private:
    // The live process of a pid word this runtime issued, or null once it ended.
    static ProcessContext *find(ProcessContext &context, Word pid) noexcept;

    // How an exit signal was sent (docs/processes.md#exit-signals): by a link, by exit_signal/2 or exit/2 to another
    // process, or by exit/2 to the sender itself, where reason normal ends it.
    enum class SignalKind : std::uint8_t { link, exit, self_exit };

    // Restore the running process after a signal operation of a builtin, which may run outside run().
    class Running;

    // Run the first queued process for one time slice, then finish the processes that ended.
    void slice() noexcept;
    // Create a process of the parent's runtime, let `prepare` load its first call into its registers (the frame,
    // or null after recording the error the process ends with) and queue it.
    template <typename Prepare> TermResult<Term> create(ProcessContext &parent, Prepare prepare, bool link) noexcept;
    // Put a process that yielded back at the end of the queue; false after recording the allocation failure.
    bool requeue(ProcessContext &process) noexcept;
    // Keep a process that waits for a message out of the queue until a send wakes it; false after recording the
    // allocation failure.
    bool park(ProcessContext &process) noexcept;
    // Run a parked receiver again after a message arrived for it.
    TermResult<void> wake(ProcessContext &receiver);
    // Queue the parked processes whose receive timeout has expired.
    void expire() noexcept;
    // Wait until the earliest receive timeout when no process can run; with none, wait forever.
    void idle() noexcept;
    // Forget a parked process's timer.
    void cancel(ProcessContext &process) noexcept;

    // Act on an exit signal from `from` at `target`: end it, turn the signal into a message when it traps exits, or
    // drop it.
    void signal(ProcessContext &target, Word from, const Term &reason, SignalKind kind);
    // Send `process` a message built in its heap, waking it when it waits; a failed build ends it as a runtime
    // failure.
    void deliver(ProcessContext &process, const TermResult<Term> &message);
    // End `target` with `reason` copied into its heap: the running process unwinds past every catch; any other
    // leaves the queue and is finished by drain().
    void end(ProcessContext &target, const Term &reason);
    // Let a process whose failure is recorded end: the running one unwinds, any other is queued for drain().
    void retire(ProcessContext &process);
    // Take a process out of the run queue, the waiting processes and the timers.
    void withdraw(ProcessContext &process) noexcept;
    // Signal the links of an ended process with its exit reason, report it and release it. The main process and one
    // whose end ends the program are kept to end it.
    void finish(ProcessContext &process);
    // Send the exit signals of an ended process's links and the 'DOWN' messages of its monitors, and drop the
    // monitors it held; false after recording why its exit reason cannot be built.
    bool notify(ProcessContext &process);
    // Signal the links and monitors of an ended process with its exit reason.
    void notify(ProcessContext &process, const Term &reason, const std::vector<Word> &links,
                const Signals::Monitors &watchers);
    // Keep an ended process that ends the program: the first one is the program's outcome.
    void stop(ProcessContext &process);
    // Finish the processes exit signals ended, oldest first.
    void drain();
    // End the program as a runtime failure of the main process when the executor itself runs out of memory.
    void fail_program() noexcept;

    // Runnable processes in the order they run; a running process is in none of them.
    std::deque<ProcessContext *> queue_;
    // Processes waiting for a message.
    std::unordered_set<ProcessContext *> parked_;
    // Parked processes whose receive has a finite timeout, by its deadline.
    std::multimap<std::chrono::steady_clock::time_point, ProcessContext *> timers_;
    // Processes ended by exit signals that drain() has not finished yet.
    std::deque<ProcessContext *> ending_;
    // The main process during run(): its end ends the program.
    ProcessContext *main_ = nullptr;
    // The process running Erlang code; an exit signal ending it records its exit and lets it unwind.
    ProcessContext *running_ = nullptr;
    // The process whose outcome ends the program, once one ended; run() returns it.
    ProcessContext *finished_ = nullptr;
    // Further ended processes that would have ended the program, released by clear().
    std::vector<ProcessContext *> stopped_;
};
} // namespace erlang_aot::runtime::detail
