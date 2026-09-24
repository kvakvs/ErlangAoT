#include <bit>
#include <erlang_aot/runtime/code_server.hpp>
#include <erlang_aot/runtime/runtime.hpp>
#include <erlang_aot/runtime/scheduler.hpp>
#include <iostream>
#include <stdexcept>

using namespace erlang_aot::runtime;
using erlang_aot::abi::v1::Status;

namespace {
// Keep lifecycle assertions active in Release as well as sanitizer builds.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Build independent owners without hiding startup failures inside later pointer accesses.
std::unique_ptr<Runtime> start_runtime() {
    auto started = Runtime::start();
    require(started.has_value(), "runtime startup failed");
    return std::move(*started);
}

// Context creation alone must not fabricate an executing or registered process.
ProcessContext &create_context(Runtime &runtime) {
    auto created = runtime.create_context();
    require(created.has_value(), "context creation failed");
    return **created;
}

// Explicitly attach one live context to the service before exercising its lifecycle.
ProcessContext &register_context(Runtime &runtime) {
    auto &context = create_context(runtime);
    require(runtime.scheduler()->register_process(context).has_value(), "registration failed");
    return context;
}

// Registration is runtime-local and once-only, including after explicit removal.
void check_registration() {
    auto runtime = start_runtime();
    auto other = start_runtime();
    auto &context = create_context(*runtime);
    auto &foreign = create_context(*other);
    auto &scheduler = *runtime->scheduler();
    require(scheduler.process_count() == 0, "context implicitly registered");
    require(scheduler.begin_dispatch(context.identity()) == std::unexpected(SchedulerError::unknown_process),
            "unknown dispatch");
    require(scheduler.finish_dispatch(context.identity(), {}) == std::unexpected(SchedulerError::unknown_process),
            "unknown return");
    require(scheduler.set_suspended(context.identity(), true) == std::unexpected(SchedulerError::unknown_process),
            "unknown control");
    require(scheduler.inspect(context.identity()) == std::unexpected(SchedulerError::unknown_process),
            "unknown lookup");
    require(scheduler.register_process(foreign) == std::unexpected(SchedulerError::wrong_owner),
            "foreign registration");
    require(scheduler.register_process(context).has_value(), "registration failed");
    require(scheduler.register_process(context) == std::unexpected(SchedulerError::already_registered),
            "duplicate accepted");
    require(scheduler.remove_process(context.identity()).has_value(), "removal failed");
    require(scheduler.register_process(context) == std::unexpected(SchedulerError::already_registered),
            "identity revived");
    require(scheduler.remove_process(context.identity()) == std::unexpected(SchedulerError::unknown_process),
            "stale removal");
    require(scheduler.process_count() == 0 && runtime->context_count() == 1, "removal destroyed context");
}

// Registry growth and erase must preserve unrelated records and never move the runtime's context owners.
void check_growth() {
    auto runtime = start_runtime();
    auto &first = register_context(*runtime);
    auto &scheduler = *runtime->scheduler();
    require(scheduler.set_suspended(first.identity(), true).has_value(), "initial suspend failed");
    ProcessContext *last = nullptr;
    for (unsigned index = 0; index < 32; ++index) {
        last = &register_context(*runtime);
    }
    require(scheduler.process_count() == 33 && scheduler.inspect(first.identity())->suspended, "growth lost state");
    require(runtime->destroy_context(&first) == Status::ok, "first removal failed");
    require(scheduler.process_count() == 32 && runtime->context_count() == 32, "erase changed unrelated ownership");
    require(scheduler.begin_dispatch(last->identity()).has_value(), "erase lost final entry");
    require(scheduler.finish_dispatch(last->identity(), {}).has_value(), "last return failed");
}

// Every identity-based operation rejects foreign identities without altering either runtime.
void check_isolation() {
    auto first = start_runtime();
    auto second = start_runtime();
    const auto local = register_context(*first).identity();
    const auto foreign = register_context(*second).identity();
    auto &scheduler = *first->scheduler();
    const auto wrong_owner = std::unexpected(SchedulerError::wrong_owner);
    require(scheduler.inspect(foreign) == wrong_owner, "foreign inspection");
    require(scheduler.remove_process(foreign) == wrong_owner, "foreign removal");
    require(scheduler.begin_dispatch(foreign) == wrong_owner, "foreign dispatch");
    require(scheduler.finish_dispatch(foreign, {}) == wrong_owner, "foreign return");
    require(scheduler.set_suspended(foreign, true) == wrong_owner, "foreign control");
    scheduler.request_shutdown();
    require(second->scheduler()->begin_dispatch(foreign).has_value(), "shutdown crossed runtime boundary");
    require(scheduler.inspect(local)->state == ProcessState::runnable, "foreign operation changed local state");
    require(scheduler.process_count() == 1 && second->scheduler()->process_count() == 1, "registry ownership changed");
}

// Invalid return data preserves an in-flight record so a valid return can still complete it.
void check_bad_returns(SchedulerService &scheduler, ProcessIdentity process) {
    require(scheduler.finish_dispatch(process, {std::bit_cast<StepDisposition>(std::uint8_t{255})}) ==
                std::unexpected(SchedulerError::invalid_argument),
            "invalid disposition accepted");
    require(
        scheduler.finish_dispatch(process, {StepDisposition::exited, std::bit_cast<ExitReason>(std::uint8_t{255})}) ==
            std::unexpected(SchedulerError::invalid_argument),
        "invalid exit reason accepted");
    require(scheduler.inspect(process)->state == ProcessState::running, "invalid return changed state");
    require(scheduler.finish_dispatch(process, {}).has_value(), "valid yield failed");
    const auto yielded = scheduler.inspect(process);
    require(yielded->state == ProcessState::runnable && !yielded->exit_reason, "yield retained terminal metadata");
}

// Running records cannot be suspended, removed or have their context destroyed before a return boundary.
void check_dispatch() {
    auto runtime = start_runtime();
    auto &context = register_context(*runtime);
    auto &scheduler = *runtime->scheduler();
    const auto process = context.identity();
    const auto invalid = std::unexpected(SchedulerError::invalid_transition);
    require(scheduler.finish_dispatch(process, {}) == invalid, "return without dispatch accepted");
    require(scheduler.set_suspended(process, true).has_value(), "suspend failed");
    require(scheduler.set_suspended(process, true).has_value(), "suspend not idempotent");
    require(scheduler.begin_dispatch(process) == invalid, "suspended dispatch accepted");
    require(scheduler.set_suspended(process, false).has_value(), "resume failed");
    require(scheduler.begin_dispatch(process).has_value(), "dispatch boundary failed");
    require(scheduler.begin_dispatch(process) == invalid, "double dispatch accepted");
    require(scheduler.set_suspended(process, true) == invalid, "running suspension applied eagerly");
    require(scheduler.remove_process(process) == invalid, "running record removed");
    require(runtime->destroy_context(&context) == Status::busy, "running context destroyed");
    check_bad_returns(scheduler, process);
}

// Resume clears explicit suspension only; a waiting process still needs future signal handling to wake.
void check_waiting() {
    auto runtime = start_runtime();
    auto &context = register_context(*runtime);
    auto &scheduler = *runtime->scheduler();
    const auto process = context.identity();
    require(scheduler.begin_dispatch(process).has_value(), "dispatch failed");
    require(scheduler.finish_dispatch(process, {StepDisposition::waiting}).has_value(), "wait failed");
    require(scheduler.set_suspended(process, true).has_value(), "waiting suspension failed");
    require(scheduler.inspect(process)->suspended, "suspension lost");
    require(scheduler.set_suspended(process, false).has_value(), "waiting resume failed");
    require(scheduler.set_suspended(process, false).has_value(), "resume not idempotent");
    const auto waiting = scheduler.inspect(process);
    require(waiting->state == ProcessState::waiting && !waiting->suspended, "resume spuriously woke waiter");
    require(scheduler.begin_dispatch(process) == std::unexpected(SchedulerError::invalid_transition),
            "waiting dispatch");
    require(runtime->destroy_context(&context) == Status::ok, "waiting context cleanup failed");
    require(scheduler.process_count() == 0, "context destruction retained registration");
}

// Terminal state preserves its reason and cannot return, resume or dispatch again.
void check_exit() {
    auto runtime = start_runtime();
    auto &context = register_context(*runtime);
    auto &scheduler = *runtime->scheduler();
    const auto process = context.identity();
    require(scheduler.begin_dispatch(process).has_value(), "dispatch failed");
    require(scheduler.finish_dispatch(process, {StepDisposition::exited, ExitReason::normal}).has_value(),
            "exit failed");
    const auto exited = scheduler.inspect(process);
    require(exited->state == ProcessState::exited && exited->exit_reason == ExitReason::normal, "exit metadata lost");
    const auto invalid = std::unexpected(SchedulerError::invalid_transition);
    require(scheduler.begin_dispatch(process) == invalid, "exited dispatch");
    require(scheduler.finish_dispatch(process, {}) == invalid, "exited return");
    require(scheduler.set_suspended(process, false) == invalid, "exited resume");
    require(runtime->destroy_context(&context) == Status::ok, "exit cleanup failed");
    require(scheduler.inspect(process) == std::unexpected(SchedulerError::unknown_process), "stale identity resolved");
}

// Closing admission preserves in-flight state until its return; runtime shutdown still requires no contexts.
void check_shutdown() {
    auto runtime = start_runtime();
    auto &context = register_context(*runtime);
    auto &unregistered = create_context(*runtime);
    auto &scheduler = *runtime->scheduler();
    const auto process = context.identity();
    require(runtime->shutdown() == Status::busy && !scheduler.stopping(), "busy shutdown closed admission");
    require(scheduler.begin_dispatch(process).has_value(), "dispatch failed");
    scheduler.request_shutdown();
    scheduler.request_shutdown();
    require(scheduler.stopping(), "shutdown not recorded");
    require(scheduler.register_process(unregistered) == std::unexpected(SchedulerError::stopped),
            "closed registration");
    require(scheduler.begin_dispatch(process) == std::unexpected(SchedulerError::stopped), "closed dispatch");
    require(scheduler.set_suspended(process, false) == std::unexpected(SchedulerError::stopped), "closed control");
    require(scheduler.finish_dispatch(process, {StepDisposition::exited, ExitReason::runtime_shutdown}).has_value(),
            "in-flight shutdown return rejected");
    require(runtime->destroy_context(&context) == Status::ok, "closed cleanup failed");
    require(runtime->destroy_context(&unregistered) == Status::ok, "unregistered cleanup failed");
    require(runtime->shutdown() == Status::ok && runtime->scheduler() == nullptr, "stopped service exposed");
}

// Observe destruction while the stopped registry still exists but all context tokens are invalid.
struct CleanupProbe final {
    // Borrow only through runtime-owned registry destruction, never through a surviving loaded-module handle.
    SchedulerService &scheduler;
    // Retain liveness metadata without retaining process storage.
    std::shared_ptr<const ContextLifetime> token;
    // Report teardown ordering after Runtime has been destroyed.
    bool &observed;

    // Record the required registry-before-context-before-code teardown order without throwing.
    ~CleanupProbe() { observed = scheduler.stopping() && scheduler.process_count() == 0 && !token->alive(); }
};

// RAII fallback retires even an unfinished bookkeeping dispatch before releasing contexts and native captures.
void check_ordered_teardown() {
    bool observed = false;
    auto runtime = start_runtime();
    auto &context = register_context(*runtime);
    auto &scheduler = *runtime->scheduler();
    auto probe = std::shared_ptr<CleanupProbe>(new CleanupProbe{scheduler, context.lifetime().lock(), observed});
    auto registry = std::make_unique<ModuleRegistry>();
    require(
        registry->add("probe", 0, [probe](ProcessContext &, std::span<const Term>) { return CallResult<Term>(Term{}); })
            .has_value(),
        "cleanup probe registration failed");
    require(runtime->code_server()->load({"probe", CodeImage::linked(), std::move(registry)}).has_value(),
            "probe load failed");
    probe.reset();
    require(scheduler.begin_dispatch(context.identity()).has_value(), "dispatch failed");
    runtime.reset();
    require(observed, "registry/context/code destruction order violated");
}
} // namespace

// Test lifecycle bookkeeping without workers, continuations, messages or timing assumptions.
int main() {
    try {
        check_registration();
        check_growth();
        check_isolation();
        check_dispatch();
        check_waiting();
        check_exit();
        check_shutdown();
        check_ordered_teardown();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
