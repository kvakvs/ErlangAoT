#pragma once
#include "process_context.hpp"
#include "process_state.hpp"
#include <optional>

namespace erlang_aot::runtime {
// Report host lifecycle errors separately from Erlang exits and future asynchronous control replies.
enum class SchedulerError : std::uint8_t {
    wrong_owner,
    unknown_process,
    already_registered,
    invalid_argument,
    invalid_transition,
    stopped,
    resource_limit
};
template <typename Value> using SchedulerResult = std::expected<Value, SchedulerError>;

// Observe lifecycle bookkeeping without implying worker placement, queue eligibility or code execution.
struct ProcessLifecycle final {
    // Start runnable; only an explicit dispatch boundary can record running or a cooperative return.
    ProcessState state = ProcessState::runnable;
    // Preserve explicit suspension independently of waiting/runnable state.
    bool suspended = false;
    // Retain a reported terminal reason until registration removal.
    std::optional<ExitReason> exit_reason;
};

// Own one runtime's serialized lifecycle registry; no workers, queues, signals or continuations run here.
class SchedulerService final {
  public:
    // Release bookkeeping after runtime teardown has retired registrations and invalidated contexts.
    ~SchedulerService();
    SchedulerService(const SchedulerService &) = delete;
    SchedulerService &operator=(const SchedulerService &) = delete;

    // Register a live context from this runtime once; failed publication leaves it eligible to retry.
    SchedulerResult<void> register_process(ProcessContext &context) noexcept;
    // Retire a non-running registration without destroying its context; the identity cannot be registered again.
    SchedulerResult<void> remove_process(ProcessIdentity process) noexcept;
    // Observe registered state even during shutdown; unknown/foreign identities never alias another process.
    SchedulerResult<ProcessLifecycle> inspect(ProcessIdentity process) const noexcept;
    // Count registered identities, including waiting, suspended and reported-exited entries pending removal.
    std::size_t process_count() const noexcept;

    // Record a future executor's dispatch boundary; this does not select work or invoke ProcessCode.
    SchedulerResult<void> begin_dispatch(ProcessIdentity process) noexcept;
    // Record a running process's cooperative return, including draining an in-flight boundary after shutdown.
    SchedulerResult<void> finish_dispatch(ProcessIdentity process, StepResult result) noexcept;
    // Set an idempotent control gate only outside dispatch; resume never changes a waiting state.
    SchedulerResult<void> set_suspended(ProcessIdentity process, bool suspended) noexcept;

    // Close registration and new dispatch/control admission; inspection, returns and removal remain available.
    void request_shutdown() noexcept;
    // Observe admission closure without claiming that any worker was joined or context destroyed.
    bool stopping() const noexcept;

  private:
    friend class Runtime;
    // Bind to the runtime identity before any host can register a context.
    explicit SchedulerService(std::uint64_t runtime_identity);
    // Retire all bookkeeping before runtime-owned contexts are destroyed; no executing code exists yet.
    void clear() noexcept;
    // Hide registry storage and its admission flag from the project service API.
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace erlang_aot::runtime
