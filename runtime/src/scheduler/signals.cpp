#include "../process/exits.hpp"
#include "../terms/service_errors.hpp"
#include "executor.hpp"
#include <algorithm>
#include <array>
#include <erlang_aot/runtime/runtime.hpp>
#include <new>
#include <utility>

// Links and exit signals (docs/processes.md#links). Only the running process sends signals, so every other target
// is queued or waiting: the executor acts on a signal at once, and a process it ends is finished before the
// sending builtin returns.
namespace erlang_aot::runtime::detail {
namespace {
using abi::v1::ErrorReason;

// Whether `term` is the atom spelled `name`.
bool is_atom(const Term &term, std::string_view name) {
    return term.is_atom() && term.atom_spelling().value_or("") == name;
}

// The atom `name`; a full atom table fails like exhausted memory.
Term atom(ProcessContext &process, std::string_view name) {
    auto atom = TermFactory(process).atom(name);
    if (!atom) {
        throw std::bad_alloc();
    }
    return *atom;
}

// {'EXIT', From, Reason} built in the heap of `process`.
TermResult<Term> exit_message(ProcessContext &process, Word from, const Term &reason) {
    TermFactory factory(process);
    const auto tag = factory.atom("EXIT");
    const auto pid = Term::from_word(from, process);
    const auto copy = reason.copy_to(process.heap());
    if (!tag || !pid || !copy) {
        return !tag ? tag : (!pid ? pid : copy);
    }
    return factory.tuple(std::array{*tag, *pid, *copy});
}
} // namespace

class Executor::Running final {
  public:
    // Make `process` the running one until the scope ends.
    Running(Executor &executor, ProcessContext &process) noexcept
        : executor_(executor), saved_(std::exchange(executor.running_, &process)) {}

    ~Running() { executor_.running_ = saved_; }

    Running(const Running &) = delete;
    Running &operator=(const Running &) = delete;

  private:
    // The executor whose running process this scope set.
    Executor &executor_;
    // The running process before the scope: itself under run(), none for a host invocation.
    ProcessContext *saved_;
};

bool Executor::link(ProcessContext &process, Word pid) {
    const auto self = pid_of(process);
    if (pid == self) {
        return true;
    }
    auto *target = find(process, pid);
    if (!target) {
        if (!process.signals().trap_exit()) {
            return false;
        }
        const Running running(*this, process);
        deliver_exit(process, pid, atom(process, "noproc"));
        return true;
    }
    process.signals().link(pid);
    target->signals().link(self);
    return true;
}

void Executor::unlink(ProcessContext &process, Word pid) noexcept {
    process.signals().unlink(pid);
    if (auto *target = find(process, pid)) {
        target->signals().unlink(pid_of(process));
    }
}

void Executor::exit(ProcessContext &sender, Word pid, const Term &reason, bool self_normal) {
    auto *target = find(sender, pid);
    if (!target) {
        return;
    }
    const Running running(*this, sender);
    signal(*target, pid_of(sender), reason,
           self_normal && target == &sender ? SignalKind::self_exit : SignalKind::exit);
    drain();
}

void Executor::signal(ProcessContext &target, Word from, const Term &reason, SignalKind kind) {
    if (target.generated_calls().failure()) {
        // The target has already ended.
        return;
    }
    if (kind != SignalKind::link && is_atom(reason, "kill")) {
        end(target, atom(target, "killed"));
    } else if (target.signals().trap_exit()) {
        deliver_exit(target, from, reason);
    } else if (!is_atom(reason, "normal") || kind == SignalKind::self_exit) {
        end(target, reason);
    }
}

void Executor::deliver_exit(ProcessContext &process, Word from, const Term &reason) {
    const auto message = exit_message(process, from, reason);
    if (!message) {
        process.generated_calls().fail_service(term_status(message.error()));
        retire(process);
        return;
    }
    process.mailbox().deliver(message->word());
    if (const auto woken = wake(process); !woken) {
        throw std::bad_alloc();
    }
}

void Executor::end(ProcessContext &target, const Term &reason) {
    auto &calls = target.generated_calls();
    if (const auto copy = reason.copy_to(target.heap())) {
        calls.fail({.code = CallError::exited, .reason = ErrorReason::raised_exit, .value = *copy});
    } else {
        calls.fail_service(term_status(copy.error()));
    }
    retire(target);
}

void Executor::retire(ProcessContext &process) {
    if (&process != running_) {
        withdraw(process);
        ending_.push_back(&process);
    }
}

void Executor::withdraw(ProcessContext &process) noexcept {
    std::erase(queue_, &process);
    if (parked_.erase(&process) != 0) {
        cancel(process);
    }
}

void Executor::finish(ProcessContext &process) {
    if (&process == main_ || ends_program(process) || !notify_links(process)) {
        stop(process);
        return;
    }
    report_exit(process);
    process.runtime().destroy_context(&process);
}

bool Executor::notify_links(ProcessContext &process) {
    const auto links = process.signals().take_links();
    if (links.empty()) {
        return true;
    }
    const auto reason = exit_reason(process);
    if (!reason) {
        auto &calls = process.generated_calls();
        calls.clear();
        calls.fail_service(term_status(reason.error()));
        return false;
    }
    const auto from = pid_of(process);
    for (const auto pid : links) {
        if (auto *target = find(process, pid)) {
            target->signals().unlink(from);
            signal(*target, from, *reason, SignalKind::link);
        }
    }
    return true;
}

void Executor::stop(ProcessContext &process) {
    if (!finished_) {
        finished_ = &process;
    } else {
        stopped_.push_back(&process);
    }
}

void Executor::drain() {
    while (!ending_.empty()) {
        auto &process = *ending_.front();
        ending_.pop_front();
        finish(process);
    }
}

void Executor::fail_program() noexcept {
    if (finished_ || !main_) {
        return;
    }
    withdraw(*main_);
    auto &calls = main_->generated_calls();
    calls.clear();
    calls.fail_service(abi::v1::Status::out_of_memory);
    finished_ = main_;
}
} // namespace erlang_aot::runtime::detail
