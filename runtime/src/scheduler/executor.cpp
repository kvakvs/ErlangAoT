#include "executor.hpp"
#include "../builtins/support.hpp"
#include "../process/exits.hpp"
#include "../process/identities.hpp"
#include "../runtime_state.hpp"
#include "../terms/funs.hpp"
#include <array>
#include <chrono>
#include <clause/abi/equality.hpp>
#include <exception>
#include <new>
#include <optional>
#include <thread>

namespace clause::runtime::detail {
namespace {
using abi::v1::FrameDescriptor;
using abi::v1::Status;

// The term error of a failed context creation.
TermError creation_error(Status status) {
    return status == Status::out_of_memory ? TermError::out_of_memory : TermError::resource_limit;
}
} // namespace

Word pid_of(ProcessContext &process) noexcept {
    return TermFactory(process).pid(process.identity()).transform(&Term::word).value_or(0);
}

bool ends_program(ProcessContext &process) noexcept {
    const auto &failure = process.generated_calls().failure();
    return failure && failure->code != CallError::erlang_exception && failure->code != CallError::exited;
}

Executor &Executor::of(ProcessContext &context) noexcept { return context.runtime().impl_->executor; }

bool Executor::start(ProcessContext &process, const FrameDescriptor &function) noexcept {
    const std::scoped_lock lock(mutex_);
    process.generated_calls().enter();
    if (!process.stack().start(function)) {
        return false;
    }
    try {
        schedules_[&process] = {};
        push(process);
        return true;
    } catch (const std::bad_alloc &) {
        schedules_.erase(&process);
        process.generated_calls().fail_service(Status::out_of_memory);
        return false;
    }
}

template <typename Prepare>
TermResult<Term> Executor::create(ProcessContext &parent, Prepare prepare, SpawnOptions options) noexcept {
    const std::scoped_lock lock(mutex_);
    auto &runtime = parent.runtime();
    const auto created = runtime.create_context();
    if (!created) {
        return std::unexpected(creation_error(created.error()));
    }
    auto &child = **created;
    child.generated_calls().enter();
    const auto frame = prepare(child);
    if (!frame) {
        runtime.destroy_context(&child);
        return std::unexpected(frame.error());
    }
    if (*frame) {
        child.stack().start(*static_cast<const FrameDescriptor *>(*frame));
    }
    const auto pid = TermFactory(parent).pid(child.identity());
    try {
        auto result = pid ? adopt(parent, child, *pid, options) : pid;
        if (!result) {
            destroy(child);
        }
        return result;
    } catch (const std::bad_alloc &) {
        destroy(child);
        return std::unexpected(TermError::out_of_memory);
    }
}

TermResult<Term> Executor::adopt(ProcessContext &parent, ProcessContext &child, const Term &pid, SpawnOptions options) {
    auto result = TermResult<Term>(pid);
    std::optional<ReferenceIdentity> monitor;
    if (options.monitor) {
        TermFactory factory(parent);
        const auto reference = factory.make_reference();
        const auto identity = reference.and_then([](const Term &term) { return term.reference_value(); });
        if (!identity) {
            return std::unexpected(identity.error());
        }
        result = factory.tuple(std::array{pid, *reference});
        monitor = *identity;
    }
    if (!result) {
        return result;
    }
    try {
        schedules_[&child] = {};
        if (options.link) {
            parent.signals().link(pid.word());
            child.signals().link(pid_of(parent));
        }
        if (monitor) {
            parent.signals().monitor(*monitor, pid.word());
            child.signals().watch(*monitor, {.pid = pid_of(parent)});
        }
        push(child);
    } catch (...) {
        parent.signals().unlink(pid.word());
        if (monitor) {
            parent.signals().demonitor(*monitor);
        }
        throw;
    }
    return result;
}

TermResult<Term> Executor::spawn(ProcessContext &parent, const Term &fun, SpawnOptions options) noexcept {
    return create(
        parent,
        [&](ProcessContext &child) -> TermResult<const void *> {
            const auto copy = fun.copy_to(child.heap());
            if (!copy) {
                return std::unexpected(copy.error());
            }
            return apply_list_service(child, copy->word(), abi::v1::empty_list, child.stack().registers());
        },
        options);
}

TermResult<Term> Executor::spawn(ProcessContext &parent, const InitialCall &call, SpawnOptions options) noexcept {
    return create(
        parent,
        [&](ProcessContext &child) -> TermResult<const void *> {
            const auto copy = call.arguments.copy_to(child.heap());
            if (!copy) {
                return std::unexpected(copy.error());
            }
            return call_list_service(child, call.module.word(), call.function.word(), copy->word(),
                                     child.stack().registers());
        },
        options);
}

ProcessContext *Executor::find(ProcessContext &context, Word pid) noexcept { return of(context).process(pid); }

ProcessContext *Executor::process(Word pid) const noexcept {
    if (TermTag{pid}.get_kind() != TermKind::local_pid) {
        return nullptr;
    }
    const auto &processes = runtime_.processes;
    const auto found = processes.find(pid_number(pid));
    return found == processes.end() ? nullptr : found->second;
}

bool Executor::alive(ProcessContext &context, Word pid) noexcept {
    const std::scoped_lock lock(of(context).mutex_);
    return find(context, pid) != nullptr;
}

TermResult<void> Executor::send(ProcessContext &sender, Word pid, const Term &message) {
    auto &executor = of(sender);
    const std::scoped_lock lock(executor.mutex_);
    auto *found = find(sender, pid);
    if (!found) {
        return {};
    }
    auto &receiver = *found;
    if (executor.busy(&receiver)) {
        executor.block(sender, receiver);
    }
    const auto copy = message.copy_to(receiver.heap());
    if (!copy) {
        return std::unexpected(copy.error());
    }
    receiver.mailbox().deliver(copy->word());
    return executor.wake(receiver);
}

Executor::Schedule &Executor::schedule(ProcessContext &process) { return schedules_[&process]; }

bool Executor::busy(ProcessContext *process) const noexcept {
    const auto found = schedules_.find(process);
    return process != running_ && found != schedules_.end() && found->second.running;
}

void Executor::block(ProcessContext &process, ProcessContext &target) {
    schedule(process).blocked_on = &target;
    throw builtins::Blocked{};
}

void Executor::push(ProcessContext &process, bool front) {
    auto &state = schedule(process);
    if (state.holds > 0) {
        state.ready = true;
        return;
    }
    if (front) {
        queue_.push_front(&process);
    } else {
        queue_.push_back(&process);
    }
    work_.notify_one();
}

TermResult<void> Executor::wake(ProcessContext &receiver) {
    if (!receiver.stack().waiting()) {
        return {};
    }
    try {
        push(receiver);
    } catch (const std::bad_alloc &) {
        return std::unexpected(TermError::out_of_memory);
    }
    receiver.stack().wake();
    parked_.erase(&receiver);
    cancel(receiver);
    return {};
}

void Executor::cancel(ProcessContext &process) noexcept {
    const auto deadline = process.mailbox().deadline();
    if (!deadline) {
        return;
    }
    for (auto [at, end] = timers_.equal_range(*deadline); at != end; ++at) {
        if (at->second == &process) {
            timers_.erase(at);
            return;
        }
    }
}

void Executor::expire() {
    const auto now = std::chrono::steady_clock::now();
    while (!timers_.empty() && timers_.begin()->first <= now) {
        auto &process = *timers_.begin()->second;
        timers_.erase(timers_.begin());
        parked_.erase(&process);
        process.stack().wake();
        push(process);
    }
}

void Executor::idle(std::unique_lock<std::mutex> &lock) {
    if (timers_.empty()) {
        // Every process waits for a message; only a worker still running a process can queue one.
        work_.wait(lock);
        return;
    }
    const auto deadline = timers_.begin()->first;
    work_.wait_until(lock, deadline);
}

void Executor::park(ProcessContext &process) {
    parked_.insert(&process);
    if (const auto deadline = process.mailbox().deadline()) {
        timers_.emplace(*deadline, &process);
        // An idle worker may sleep until a later deadline.
        work_.notify_all();
    }
}

void Executor::work() noexcept {
    std::unique_lock lock(mutex_);
    while (!finished_) {
        try {
            expire();
            if (const auto port = next_port()) {
                run_port(*port);
                continue;
            }
            if (queue_.empty()) {
                idle(lock);
                continue;
            }
            auto &process = *queue_.front();
            queue_.pop_front();
            schedule(process).running = true;
            lock.unlock();
            running_ = &process;
            const bool ended = process.stack().run(SLICE_REDUCTIONS);
            running_ = nullptr;
            lock.lock();
            after(process, ended);
        } catch (const std::bad_alloc &) {
            fail_program();
        } catch (...) {
            // A failing mutex or condition variable leaves the executor without a consistent state.
            std::terminate();
        }
    }
    work_.notify_all();
}

void Executor::after(ProcessContext &process, bool ended) {
    auto &state = schedule(process);
    state.running = false;
    const auto blockers = std::exchange(state.blockers, {});
    auto *blocked_on = std::exchange(state.blocked_on, nullptr);
    release(process);
    // The process is placed before its blockers act: a message they deliver must find it parked, not mid-way.
    if (failed_ && &process == main_) {
        fail_main();
    } else if (ended) {
        finish(process);
    } else {
        hold(blockers, process);
        place(process, blocked_on);
        for (const auto &event : std::exchange(state.events, {})) {
            apply(process, event);
        }
    }
    resume(blockers);
    drain();
}

void Executor::hold(const std::vector<ProcessContext *> &blockers, ProcessContext &holder) {
    for (auto *blocker : blockers) {
        auto &state = schedule(*blocker);
        if (!state.ending) {
            state.holding.push_back(pid_of(holder));
            ++schedule(holder).holds;
        }
    }
}

void Executor::resume(const std::vector<ProcessContext *> &blockers) {
    // Pushing to the front in reverse keeps the blockers in the order they blocked.
    for (auto at = blockers.rbegin(); at != blockers.rend(); ++at) {
        auto &blocker = **at;
        auto &state = schedule(blocker);
        state.blocked_on = nullptr;
        if (std::exchange(state.ending, false)) {
            finish(blocker);
        } else {
            push(blocker, true);
        }
    }
}

void Executor::place(ProcessContext &process, ProcessContext *blocked_on) {
    // The process waited for may have ended since; its address is then only compared, never used.
    if (blocked_on && busy(blocked_on)) {
        schedule(*blocked_on).blockers.push_back(&process);
        schedule(process).blocked_on = blocked_on;
    } else if (process.stack().waiting()) {
        park(process);
    } else {
        push(process);
    }
}

void Executor::release(ProcessContext &process) {
    for (const auto pid : std::exchange(schedule(process).holding, {})) {
        auto *held = find(process, pid);
        if (!held) {
            continue;
        }
        auto &state = schedule(*held);
        if (state.holds == 0) {
            continue;
        }
        --state.holds;
        if (state.holds == 0 && std::exchange(state.ready, false)) {
            push(*held);
        }
    }
}

ProcessContext &Executor::run(ProcessContext &main) noexcept {
    {
        const std::scoped_lock lock(mutex_);
        main_ = &main;
        finished_ = nullptr;
        failed_ = false;
    }
    std::vector<std::thread> helpers;
    const auto workers = main.runtime().impl_->options.schedulers;
    for (std::size_t count = 1; count < workers; ++count) {
        try {
            helpers.emplace_back([this] { work(); });
        } catch (const std::exception &) {
            // Run on the workers started so far.
            break;
        }
    }
    work();
    for (auto &helper : helpers) {
        helper.join();
    }
    return *finished_;
}

void Executor::destroy(ProcessContext &process) noexcept {
    schedules_.erase(&process);
    process.runtime().destroy_context(&process);
}

Executor::~Executor() {
    if (reactor_) {
        reactor_->stop();
    }
}

void Executor::clear() noexcept {
    // The I/O thread and the Windows blocking readers stop outside the lock: a delivery in progress waits for it.
    // Workers have stopped, so nothing else creates or replaces the services meanwhile.
    Reactor *reactor = nullptr;
    IoService *io = nullptr;
    {
        const std::scoped_lock lock(mutex_);
        reactor = reactor_.get();
        io = io_.get();
    }
    if (reactor) {
        reactor->stop();
    }
    if (io) {
        io->close();
    }
    const std::scoped_lock lock(mutex_);
    for (const auto &[process, state] : schedules_) {
        if (process != finished_) {
            process->runtime().destroy_context(process);
        }
    }
    schedules_.clear();
    queue_.clear();
    port_queue_.clear();
    parked_.clear();
    timers_.clear();
    ending_.clear();
    stopped_.clear();
    names_.clear();
    // Drivers go before the services, and the reactor's io_context last.
    ports_.clear();
    io_.reset();
    sockets_.reset();
    reactor_.reset();
    main_ = finished_ = nullptr;
    failed_ = false;
}
} // namespace clause::runtime::detail
