#include "executor.hpp"
#include "../process/exits.hpp"
#include "../process/identities.hpp"
#include "../runtime_state.hpp"
#include "../terms/funs.hpp"
#include <chrono>
#include <erlang_aot/abi/equality.hpp>
#include <new>
#include <thread>

namespace erlang_aot::runtime::detail {
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
    process.generated_calls().enter();
    if (!process.stack().start(function)) {
        return false;
    }
    try {
        queue_.push_back(&process);
        return true;
    } catch (const std::bad_alloc &) {
        process.generated_calls().fail_service(Status::out_of_memory);
        return false;
    }
}

template <typename Prepare>
TermResult<Term> Executor::create(ProcessContext &parent, Prepare prepare, bool link) noexcept {
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
    if (!pid) {
        runtime.destroy_context(&child);
        return pid;
    }
    try {
        if (link) {
            parent.signals().link(pid->word());
            child.signals().link(pid_of(parent));
        }
        queue_.push_back(&child);
    } catch (const std::bad_alloc &) {
        parent.signals().unlink(pid->word());
        runtime.destroy_context(&child);
        return std::unexpected(TermError::out_of_memory);
    }
    return pid;
}

TermResult<Term> Executor::spawn(ProcessContext &parent, const Term &fun, bool link) noexcept {
    return create(
        parent,
        [&](ProcessContext &child) -> TermResult<const void *> {
            const auto copy = fun.copy_to(child.heap());
            if (!copy) {
                return std::unexpected(copy.error());
            }
            return apply_list_service(child, copy->word(), abi::v1::empty_list, child.stack().registers());
        },
        link);
}

TermResult<Term> Executor::spawn(ProcessContext &parent, const InitialCall &call, bool link) noexcept {
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
        link);
}

ProcessContext *Executor::find(ProcessContext &context, Word pid) noexcept {
    const auto &processes = context.runtime().impl_->processes;
    const auto found = processes.find(pid_number(pid));
    return found == processes.end() ? nullptr : found->second;
}

bool Executor::alive(ProcessContext &context, Word pid) noexcept { return find(context, pid) != nullptr; }

TermResult<void> Executor::send(ProcessContext &sender, Word pid, const Term &message) {
    auto *found = find(sender, pid);
    if (!found) {
        return {};
    }
    auto &receiver = *found;
    const auto copy = message.copy_to(receiver.heap());
    if (!copy) {
        return std::unexpected(copy.error());
    }
    receiver.mailbox().deliver(copy->word());
    return of(sender).wake(receiver);
}

TermResult<void> Executor::wake(ProcessContext &receiver) {
    if (!receiver.stack().waiting()) {
        return {};
    }
    try {
        queue_.push_back(&receiver);
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

void Executor::expire() noexcept {
    const auto now = std::chrono::steady_clock::now();
    while (!timers_.empty() && timers_.begin()->first <= now) {
        auto &process = *timers_.begin()->second;
        if (!requeue(process)) {
            return;
        }
        timers_.erase(timers_.begin());
        parked_.erase(&process);
        process.stack().wake();
    }
}

void Executor::idle() noexcept {
    if (timers_.empty()) {
        // Every process waits for a message no running process can send.
        for (;;) {
            std::this_thread::sleep_for(std::chrono::hours(1));
        }
    }
    std::this_thread::sleep_until(timers_.begin()->first);
    expire();
}

bool Executor::park(ProcessContext &process) noexcept {
    try {
        parked_.insert(&process);
        if (const auto deadline = process.mailbox().deadline()) {
            timers_.emplace(*deadline, &process);
        }
        return true;
    } catch (const std::bad_alloc &) {
        process.generated_calls().fail_service(Status::out_of_memory);
        return false;
    }
}

bool Executor::requeue(ProcessContext &process) noexcept {
    try {
        queue_.push_back(&process);
        return true;
    } catch (const std::bad_alloc &) {
        process.generated_calls().fail_service(Status::out_of_memory);
        return false;
    }
}

void Executor::slice() noexcept {
    expire();
    while (queue_.empty()) {
        idle();
    }
    auto &process = *queue_.front();
    queue_.pop_front();
    running_ = &process;
    const bool ended = process.stack().run(SLICE_REDUCTIONS);
    running_ = nullptr;
    try {
        if (ended || !(process.stack().waiting() ? park(process) : requeue(process))) {
            finish(process);
        }
        drain();
    } catch (const std::bad_alloc &) {
        fail_program();
    }
}

ProcessContext &Executor::run(ProcessContext &main) noexcept {
    main_ = &main;
    finished_ = nullptr;
    while (!finished_) {
        slice();
    }
    return *finished_;
}

void Executor::clear() noexcept {
    for (auto *process : stopped_) {
        process->runtime().destroy_context(process);
    }
    stopped_.clear();
    for (auto *process : ending_) {
        process->runtime().destroy_context(process);
    }
    ending_.clear();
    main_ = finished_ = nullptr;
    while (!queue_.empty()) {
        auto &process = *queue_.back();
        queue_.pop_back();
        process.runtime().destroy_context(&process);
    }
    timers_.clear();
    while (!parked_.empty()) {
        auto *process = *parked_.begin();
        parked_.erase(parked_.begin());
        process->runtime().destroy_context(process);
    }
}
} // namespace erlang_aot::runtime::detail
