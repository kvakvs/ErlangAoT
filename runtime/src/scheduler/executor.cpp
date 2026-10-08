#include "executor.hpp"
#include "../process/exits.hpp"
#include "../process/identities.hpp"
#include "../runtime_state.hpp"
#include "../terms/funs.hpp"
#include <erlang_aot/abi/equality.hpp>
#include <new>

namespace erlang_aot::runtime::detail {
namespace {
using abi::v1::FrameDescriptor;
using abi::v1::Status;

// The term error of a failed context creation.
TermError creation_error(Status status) {
    return status == Status::out_of_memory ? TermError::out_of_memory : TermError::resource_limit;
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

bool Executor::requeue(ProcessContext &process) noexcept {
    try {
        queue_.push_back(&process);
        return true;
    } catch (const std::bad_alloc &) {
        process.generated_calls().fail_service(Status::out_of_memory);
        return false;
    }
}

ProcessContext &Executor::run(ProcessContext &main) noexcept {
    while (!queue_.empty()) {
        auto &process = *queue_.front();
        queue_.pop_front();
        if (!process.stack().run(SLICE_REDUCTIONS)) {
            if (!requeue(process)) {
                return process;
            }
            continue;
        }
        if (&process == &main || ends_program(process)) {
            return process;
        }
        report_exit(process);
        process.runtime().destroy_context(&process);
    }
    return main;
}

void Executor::clear() noexcept {
    while (!queue_.empty()) {
        auto &process = *queue_.back();
        queue_.pop_back();
        process.runtime().destroy_context(&process);
    }
}
} // namespace erlang_aot::runtime::detail
