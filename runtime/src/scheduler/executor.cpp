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

// Wait forever: every process waits for a message no running process can send.
[[noreturn]] void block_forever() noexcept {
    for (;;) {
        std::this_thread::sleep_for(std::chrono::hours(1));
    }
}

// Whether an ended process ends the whole program: a halt or a failure outside Erlang, not an Erlang exception.
bool ends_program(ProcessContext &process) {
    const auto &failure = process.generated_calls().failure();
    return failure && failure->code != CallError::erlang_exception;
}
} // namespace

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

template <typename Prepare> TermResult<Term> Executor::create(ProcessContext &parent, Prepare prepare) noexcept {
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
    try {
        queue_.push_back(&child);
    } catch (const std::bad_alloc &) {
        runtime.destroy_context(&child);
        return std::unexpected(TermError::out_of_memory);
    }
    return TermFactory(parent).pid(child.identity());
}

TermResult<Term> Executor::spawn(ProcessContext &parent, const Term &fun) noexcept {
    return create(parent, [&](ProcessContext &child) -> TermResult<const void *> {
        const auto copy = fun.copy_to(child.heap());
        if (!copy) {
            return std::unexpected(copy.error());
        }
        return apply_list_service(child, copy->word(), abi::v1::empty_list, child.stack().registers());
    });
}

TermResult<Term> Executor::spawn(ProcessContext &parent, const InitialCall &call) noexcept {
    return create(parent, [&](ProcessContext &child) -> TermResult<const void *> {
        const auto copy = call.arguments.copy_to(child.heap());
        if (!copy) {
            return std::unexpected(copy.error());
        }
        return call_list_service(child, call.module.word(), call.function.word(), copy->word(),
                                 child.stack().registers());
    });
}

bool Executor::alive(ProcessContext &context, Word pid) noexcept {
    return context.runtime().impl_->processes.contains(pid_number(pid));
}

TermResult<void> Executor::send(ProcessContext &sender, Word pid, const Term &message) {
    const auto &processes = sender.runtime().impl_->processes;
    const auto found = processes.find(pid_number(pid));
    if (found == processes.end()) {
        return {};
    }
    auto &receiver = *found->second;
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
    return {};
}

bool Executor::park(ProcessContext &process) noexcept {
    try {
        parked_.insert(&process);
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

ProcessContext *Executor::slice(ProcessContext &main) noexcept {
    if (queue_.empty()) {
        block_forever();
    }
    auto &process = *queue_.front();
    queue_.pop_front();
    if (!process.stack().run(SLICE_REDUCTIONS)) {
        const bool kept = process.stack().waiting() ? park(process) : requeue(process);
        return kept ? nullptr : &process;
    }
    if (&process == &main || ends_program(process)) {
        return &process;
    }
    report_exit(process);
    process.runtime().destroy_context(&process);
    return nullptr;
}

ProcessContext &Executor::run(ProcessContext &main) noexcept {
    for (;;) {
        if (auto *ended = slice(main)) {
            return *ended;
        }
    }
}

void Executor::clear() noexcept {
    while (!queue_.empty()) {
        auto &process = *queue_.back();
        queue_.pop_back();
        process.runtime().destroy_context(&process);
    }
    while (!parked_.empty()) {
        auto *process = *parked_.begin();
        parked_.erase(parked_.begin());
        process->runtime().destroy_context(process);
    }
}
} // namespace erlang_aot::runtime::detail
