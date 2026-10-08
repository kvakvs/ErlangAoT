#pragma once
#include "../ports/io.hpp"
#include "../ports/port.hpp"
#include <chrono>
#include <condition_variable>
#include <deque>
#include <erlang_aot/abi/frames.hpp>
#include <erlang_aot/runtime/process_context.hpp>
#include <erlang_aot/runtime/runtime.hpp>
#include <map>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// The executor of one runtime (docs/processes.md#workers): a first-in, first-out queue of runnable processes, each run
// for a time slice of reductions by one of the scheduler workers. One mutex guards the queue and every process that is
// not running; a running process's heap, stack and mailbox belong to its worker alone.
namespace erlang_aot::runtime::detail {
// The call a process spawned by spawn/3 starts with: Module:Function(Arguments...).
struct InitialCall final {
    // Atoms naming the function the new process calls.
    Term module;
    Term function;
    // A proper list of the arguments.
    Term arguments;
};

// What a spawn does besides starting the process, before it can run: link it to the parent, or monitor it from the
// parent (the spawn then returns {Pid, Ref}).
struct SpawnOptions final {
    bool link = false;
    bool monitor = false;
};

// A request sent to a port as a message (docs/ports.md#builtins-and-port-messages): {From, close},
// {From, {command, Data}} or {From, {connect, Pid}}; anything else is malformed.
struct PortRequest final {
    enum class Kind : std::uint8_t { close, command, connect, malformed };
    Kind kind = Kind::malformed;
    // The pid the request names as its sender, which must be the port's connected process.
    Word from = 0;
    // The output of a command, its framing not yet applied.
    std::vector<std::byte> data;
    // The new connected process of a connect.
    Word owner = 0;

    // A request without data or a new owner.
    static PortRequest of(Kind kind, Word from) { return {kind, from, {}, 0}; }
};

// What port_info/1,2 report of an open port, copied out of the port table.
struct PortInfo final {
    std::string name;
    std::vector<Word> links;
    Word id = 0;
    Word connected = 0;
    std::size_t input = 0;
    std::size_t output = 0;
    std::optional<std::int64_t> os_pid;
    std::vector<Word> monitored_by;
    Word registered = 0;
};

// A signal from a port to a process that holds no heap term, so it can wait while its target runs on another worker:
// an exit signal or 'DOWN' with an atom reason, or a message of the port protocol or of its input.
struct PortEvent final {
    enum class Kind : std::uint8_t { exit, down, closed, connected, data, eof };
    Kind kind = Kind::exit;
    // The port word, and the reason atom word of an exit signal or a 'DOWN'.
    Word port = 0;
    Word reason = 0;
    // The monitor of a 'DOWN' and the registered name it was made with, or 0.
    std::optional<ReferenceIdentity> reference;
    Word name = 0;
    // The input of a data message, how it is framed, and whether it is a binary (else a byte list).
    std::vector<std::byte> bytes;
    PortInput::Kind input = PortInput::Kind::data;
    bool binary = false;

    // An exit signal with an atom reason.
    static PortEvent exit(Word port, Word reason) noexcept { return {Kind::exit, port, reason, std::nullopt, 0, {}}; }

    // A message {Port, closed}, {Port, connected} or {Port, eof}.
    static PortEvent message(Kind kind, Word port) noexcept { return {kind, port, 0, std::nullopt, 0, {}}; }

    // A 'DOWN' of `reference` with an atom reason.
    static PortEvent down(Word port, Word reason, const ReferenceIdentity &reference, Word name) noexcept {
        return {Kind::down, port, reason, reference, name, {}};
    }

    // A message {Port, {data, Data}} of one input unit (data, eol or noeol).
    static PortEvent data(Word port, PortInput unit, bool binary) noexcept {
        return {Kind::data, port, 0, std::nullopt, 0, std::move(unit.bytes), unit.kind, binary};
    }
};

// The outcome of port output: written, the port is not open, or the data does not fit the port's packet header.
enum class PortOutcome : std::uint8_t { done, not_open, too_long };

// The name of the only node, as node/0 returns it.
inline constexpr std::string_view LOCAL_NODE = "nonode@nohost";

// The pid word of a process.
Word pid_of(ProcessContext &process) noexcept;
// Whether an ended process ends the whole program: a halt or a failure outside Erlang, not an Erlang exception or an
// exit signal.
bool ends_program(ProcessContext &process) noexcept;

class Executor final {
  public:
    // Bind the executor to the runtime whose processes it runs.
    explicit Executor(Runtime::Impl &runtime) : runtime_(runtime) {}

    // The executor of a context's runtime.
    static Executor &of(ProcessContext &context) noexcept;

    // Queue `process` to start with a call of `function` on the arguments already in its registers; false after
    // recording the failure in its channel.
    bool start(ProcessContext &process, const abi::v1::FrameDescriptor &function) noexcept;
    // spawn(Fun) of `parent`: a new process calls Fun(). Its call is prepared in the new process, so a fun of
    // another arity crashes it, not the parent. The parent gets the new pid ({Pid, Ref} when monitoring), or the
    // failure to create it.
    TermResult<Term> spawn(ProcessContext &parent, const Term &fun, SpawnOptions options = {}) noexcept;
    // spawn(M, F, Args) of `parent`: a new process calls M:F(Args...), crashing with undef when nothing exports it.
    TermResult<Term> spawn(ProcessContext &parent, const InitialCall &call, SpawnOptions options = {}) noexcept;
    // Whether the process of a pid word this runtime issued has not ended.
    static bool alive(ProcessContext &context, Word pid) noexcept;
    // Deliver `message` of `sender` to the process of a pid word this runtime issued: copied into the receiver's
    // heap and appended to its signal inbox, waking it when it waits. A process that has ended gets nothing; the
    // error is a failed copy. Throws builtins::Blocked while the receiver runs on another worker.
    static TermResult<void> send(ProcessContext &sender, Word pid, const Term &message);

    // Links, monitors and names change under the lock at once, also while the other process runs elsewhere.
    // link(Pid) of the running `process` (docs/processes.md#links). For an ended process it sends itself
    // {'EXIT', Pid, noproc} when trapping exits; otherwise it returns false and the caller raises noproc.
    // Allocation failure throws.
    bool link(ProcessContext &process, Word pid);
    // unlink(Pid) of `process`: the link has no effect from now on.
    static void unlink(ProcessContext &process, Word pid) noexcept;
    // monitor(process, Item), or monitor(port, Item) with `port`, of the running `watcher`: a new reference, for the
    // pid or port, or for what is registered as `name` (an atom word; `pid` is then 0), whose 'DOWN' names
    // {Name, nonode@nohost}. For an ended, closed or unregistered target the watcher gets its 'DOWN' with noproc at
    // once; monitoring itself creates nothing. Allocation failure throws.
    Term monitor(ProcessContext &watcher, Word pid, Word name = 0, bool port = false);
    // demonitor(Ref) of `watcher`: whether the monitor was active; no 'DOWN' of it arrives afterwards.
    static bool demonitor(ProcessContext &watcher, const ReferenceIdentity &reference) noexcept;
    // register(Name, Pid) (docs/processes.md#registered-names): false when the name is taken, or the process has a
    // name or has ended. Allocation failure throws.
    bool register_name(ProcessContext &context, Word name, Word pid);
    // unregister(Name): false when no live process has the name.
    bool unregister(ProcessContext &context, Word name) noexcept;
    // The pid of the live process registered as `name` (an atom word), or 0.
    Word whereis(ProcessContext &context, Word name) const noexcept;
    // The registered names of live processes, in atom order. Allocation failure throws.
    std::vector<Word> registered(ProcessContext &context) const;
    // An exit signal of exit/2 or exit_signal/2 from the running `sender` to the process of a pid word, acted on at
    // once; `self_normal` is exit/2's quirk: reason normal sent to itself ends the sender. Allocation failure throws,
    // builtins::Blocked while the target runs on another worker.
    void exit(ProcessContext &sender, Word pid, const Term &reason, bool self_normal);

    // Ports (docs/ports.md). Open a port of `owner` running `driver`, linked to it; the port's word. Allocation failure
    // throws.
    Word open_port(ProcessContext &owner, std::unique_ptr<PortDriver> driver, PortOptions options,
                   std::string spelling);
    // port_close(Port) of `caller`: false when the port is not open.
    bool close_port(ProcessContext &caller, Word port);
    // port_command(Port, Data): write `data` with the port's framing; a failed write closes the port.
    PortOutcome command_port(ProcessContext &caller, Word port, std::vector<std::byte> data);
    // port_connect(Port, Pid): false when the port is not open or Pid is not a live process; links Pid.
    bool connect_port(Word port, const Term &owner_pid);
    // Port ! Request from `sender`; a malformed request, or one naming another process than the connected one, sends
    // the connected process an exit signal badsig. Nothing for a port that is not open.
    void port_request(ProcessContext &sender, Word port, const PortRequest &request);
    // The input of a port read by the I/O thread: data becomes messages to its connected process, end of input
    // {Port, eof} or a normal close, a read error a close with its reason. Nothing for a port that closed.
    void input(Word port, std::vector<PortInput> units, std::size_t read) noexcept;
    // What port_info/1,2 report; none for a port that is not open.
    std::optional<PortInfo> port_info(Word port) const;
    // The open ports, oldest first.
    std::vector<Word> ports() const;
    // Whether `port` is open and takes port_control/3.
    bool controllable(Word port) const;

    // Run queued processes on RuntimeOptions::schedulers workers (this thread and more threads) until `main` ends
    // (also by an exit signal) or another process halts or fails outside Erlang; once every worker stopped, return
    // the process whose outcome ends the program. Processes ending before it are released. Waiting processes whose
    // receive timeout expires are queued again; when every process waits without one, the program blocks forever,
    // as OTP's does.
    ProcessContext &run(ProcessContext &main) noexcept;
    // Release every process the executor started except the one run() returned, without running it further.
    void clear() noexcept;

  private:
    // Scheduling state of one process the executor started.
    struct Schedule final {
        // Runs on a worker: no other worker touches its heap, stack, mailbox or failure until its slice ends.
        bool running = false;
        // Has ended and waits among `blocked_on`'s blockers to be finished.
        bool ending = false;
        // Runnable but held: queued once its last hold is released.
        bool ready = false;
        // Holds of blockers woken to act on it before it runs again.
        std::size_t holds = 0;
        // The running process whose slice this one waits for, while it is among that process's blockers.
        ProcessContext *blocked_on = nullptr;
        // Blocked processes, and ended ones to finish, that wait for this running process's slice to end.
        std::vector<ProcessContext *> blockers;
        // Pids of the processes this one holds until its next slice ends.
        std::vector<Word> holding;
        // Port signals that wait for this running process's slice to end, oldest first.
        std::vector<PortEvent> events;
    };

    // How an exit signal was sent (docs/processes.md#exit-signals): by a link, by exit_signal/2 or exit/2 to another
    // process, or by exit/2 to the sender itself, where reason normal ends it.
    enum class SignalKind : std::uint8_t { link, exit, self_exit };

    // Whether `word` is a port word rather than a pid word.
    static bool is_port_word(Word word) noexcept;
    // The open port of a port word, or null.
    Port *open(Word port) const noexcept;
    // Close an open port with `reason`: unregister it, send exit signals to its links and 'DOWN' messages to its
    // monitors, release its driver. A reason other than an atom requires that no linked or monitoring process runs
    // elsewhere.
    void close(Port &port, const Term &reason);
    // An exit signal from `from` at an open port: a link's normal from another process than the connected one does
    // nothing; anything else closes the port, kill as killed.
    void port_exit(Port &port, Word from, const Term &reason, SignalKind kind);
    // Write `data` to an open port with its framing; a failed write closes the port with the driver's reason.
    PortOutcome write(ProcessContext &caller, Port &port, std::vector<std::byte> data);
    // Act on a well-formed request of the connected process; false when it is not one (badsig).
    bool serve(ProcessContext &sender, Port &port, const PortRequest &request);
    // Close the port or connect it to the request's new owner, then tell the old connected process so.
    void answer(ProcessContext &sender, Port &port, const PortRequest &request);
    // Send the port's connected process an exit signal badsig.
    void badsig(ProcessContext &sender, Port &port);
    // Send a linked process the exit signal of a closing port, at once or when its slice ends.
    void link_closed(ProcessContext &target, Word port, const Term &reason);
    // Send a monitoring process the 'DOWN' of a closing port, at once or when its slice ends.
    void monitor_closed(ProcessContext &target, Word port, const ReferenceIdentity &reference, Word name,
                        const Term &reason);
    // A process a linked pid or port leads to that runs on another worker, or null.
    ProcessContext *busy_link(ProcessContext &process, Word link) noexcept;
    // A process linked to or monitoring `port` that runs on another worker, or null.
    ProcessContext *busy_peer(const Port &port) noexcept;
    // Signal `target` from a port now, or when its slice ends while it runs elsewhere.
    void post(ProcessContext &target, PortEvent event);
    // Act on one input unit of an open port; false once the unit closed the port.
    bool input(Port &port, PortInput unit);
    // Act on the end of an open port's input or a read error: {Port, eof} with option eof, else a close.
    bool input_end(Port &port, const PortInput &unit);
    // The runtime's I/O service, started by the first port that reads input.
    IoService &io();
    // Act on a port signal at a process that does not run elsewhere.
    void apply(ProcessContext &target, const PortEvent &event);
    // link(Port), unlink(Port), monitor(port, Port), demonitor of a port monitor and exit/2 to a port of `process`.
    bool link_port(ProcessContext &process, Word port);
    void unlink_port(ProcessContext &process, Word port) noexcept;
    Term monitor_port(ProcessContext &watcher, Word port, Word name, const Term &reference);
    void exit_port(ProcessContext &sender, Word port, const Term &reason);
    // Drop the links and monitors an ended process had with ports, signalling the ports linked to it.
    void notify_ports(ProcessContext &process, const Term &reason, const std::vector<Word> &links);

    // The live process of a pid word of this executor's runtime, or null; the lock is held.
    ProcessContext *process(Word pid) const noexcept;
    // The live process of a pid word this runtime issued, or null once it ended; the lock is held.
    static ProcessContext *find(ProcessContext &context, Word pid) noexcept;

    // Restore the running process after a signal operation of a builtin, which may run outside run().
    class Running;

    // Run processes from the queue until the program ends; every worker thread runs this.
    void work() noexcept;
    // Wait until a process is queued or the earliest receive timeout; with neither, until another worker queues one.
    void idle(std::unique_lock<std::mutex> &lock);
    // Account for a process whose time slice ended: wake its blockers, then finish, block, park or queue it.
    void after(ProcessContext &process, bool ended);
    // Let each live blocker of `holder` hold it: `holder` is not queued again before their next slices end.
    void hold(const std::vector<ProcessContext *> &blockers, ProcessContext &holder);
    // Queue blockers woken by the end of a slice ahead of all others; finish the ended ones.
    void resume(const std::vector<ProcessContext *> &blockers);
    // Put a process that left its slice where it belongs: among the blockers of the busy process it waits for,
    // parked, or queued.
    void place(ProcessContext &process, ProcessContext *blocked_on);
    // Release the holds of `process`, queueing held processes whose last hold it was.
    void release(ProcessContext &process);
    // The scheduling state of a started process.
    Schedule &schedule(ProcessContext &process);
    // Whether `process` runs on a worker other than the calling thread.
    bool busy(ProcessContext *process) const noexcept;
    // Record that the running `process` waits for busy `target` and make its builtin run again: throws Blocked.
    [[noreturn]] void block(ProcessContext &process, ProcessContext &target);
    // A process an ended `process` must signal (a link or a monitor's watcher) that runs on another worker, or null.
    ProcessContext *busy_peer(ProcessContext &process) noexcept;
    // Create a process of the parent's runtime, let `prepare` load its first call into its registers (the frame,
    // or null after recording the error the process ends with) and queue it.
    template <typename Prepare>
    TermResult<Term> create(ProcessContext &parent, Prepare prepare, SpawnOptions options) noexcept;
    // Link or monitor a new process as `options` say and queue it; returns its pid, or {Pid, Ref} when monitored.
    // Allocation failure throws.
    TermResult<Term> adopt(ProcessContext &parent, ProcessContext &child, const Term &pid, SpawnOptions options);
    // Queue a runnable process, at the front for a woken blocker; a held one is only marked ready. Allocation
    // failure throws.
    void push(ProcessContext &process, bool front = false);
    // Keep a process that waits for a message out of the queue until a send wakes it. Allocation failure throws.
    void park(ProcessContext &process);
    // Run a parked receiver again after a message arrived for it.
    TermResult<void> wake(ProcessContext &receiver);
    // Queue the parked processes whose receive timeout has expired.
    void expire();
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
    // Take a process out of the run queue, the waiting processes, the timers and a blocker list; release its holds.
    void withdraw(ProcessContext &process);
    // Signal the links of an ended process with its exit reason, report it and release it. The main process and one
    // whose end ends the program are kept to end it; one with a busy peer waits for that peer's slice to end.
    void finish(ProcessContext &process);
    // Send the exit signals of an ended process's links and the 'DOWN' messages of its monitors, and drop the
    // monitors it held; false after recording why its exit reason cannot be built.
    bool notify(ProcessContext &process);
    // Signal the links and monitors of an ended process with its exit reason.
    void notify(ProcessContext &process, const Term &reason, const std::vector<Word> &links,
                const Signals::Monitors &watchers);
    // Keep an ended process that ends the program: the first one is the program's outcome; wake every worker.
    void stop(ProcessContext &process);
    // Finish the processes exit signals ended, oldest first.
    void drain();
    // Release an ended process and forget its schedule.
    void destroy(ProcessContext &process) noexcept;
    // The live process registered as `name`, or 0.
    Word lookup(ProcessContext &context, Word name) const noexcept;
    // End the program as a runtime failure of the main process when the executor itself runs out of memory; a main
    // process running elsewhere fails when its slice ends.
    void fail_program() noexcept;
    // Fail the main process as out of memory and end the program with it.
    void fail_main() noexcept;

    // The runtime whose processes and ports this executor runs.
    Runtime::Impl &runtime_;
    // Guards everything below and every process that is not running.
    mutable std::mutex mutex_;
    // Wakes idle workers: a process was queued, a timer added, or the program ended.
    std::condition_variable work_;
    // Scheduling state of every process started and not yet released.
    std::unordered_map<ProcessContext *, Schedule> schedules_;
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
    // The process this thread runs; an exit signal ending it records its exit and lets it unwind.
    static inline thread_local ProcessContext *running_ = nullptr;
    // The process whose outcome ends the program, once one ended; run() returns it.
    ProcessContext *finished_ = nullptr;
    // Registered names (atom words) and their pid or port words; a name is released when its owner ends.
    std::map<Word, Word> names_;
    // Open ports by number; a port leaves when it closes.
    std::map<Word, std::unique_ptr<Port>> ports_;
    // Reads port input on its own threads; declared last so it stops first, before the state its deliveries use.
    std::unique_ptr<IoService> io_;
    // Further ended processes that would have ended the program, released by clear().
    std::vector<ProcessContext *> stopped_;
    // The executor ran out of memory while the main process ran elsewhere; it fails when its slice ends.
    bool failed_ = false;
};
} // namespace erlang_aot::runtime::detail
