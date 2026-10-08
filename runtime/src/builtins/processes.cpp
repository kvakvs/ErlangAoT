#include "../scheduler/executor.hpp"
#include "typed.hpp"
#include <array>

// The process builtins of the bridge (docs/builtins.md, docs/processes.md): self/0, make_ref/0, spawn/1,3,
// is_process_alive/1 and sends ('!'/2, send/2).
namespace erlang_aot::runtime::builtins {
namespace {
// self(): the pid of the calling process.
TermResult<Term> self(ProcessContext &context) { return TermFactory(context).pid(context.identity()); }

// make_ref(): a reference unique within the program run.
TermResult<Term> make_ref(ProcessContext &context) { return TermFactory(context).make_reference(); }

// spawn(Fun): a new process calling Fun(); badarg unless Fun is a fun.
TermResult<Term> spawn_fun(ProcessContext &context, const Term &fun) {
    if (!fun.is_function()) {
        bad_argument();
    }
    return detail::Executor::of(context).spawn(context, fun);
}

// spawn(Module, Function, Args): a new process calling Module:Function(Args...).
TermResult<Term> spawn_call(ProcessContext &context, const AtomArgument &module, const AtomArgument &function,
                            const ListArgument &arguments) {
    return detail::Executor::of(context).spawn(context, {module.term, function.term, arguments.term});
}

// is_process_alive(Pid): whether the process has not ended; badarg unless Pid is a pid.
TermResult<Term> is_process_alive(ProcessContext &context, const Term &pid) {
    if (!pid.is_pid()) {
        bad_argument();
    }
    return TermFactory(context).boolean(detail::Executor::alive(context, pid.word()));
}

// A {Name, Node} destination: two atoms.
bool remote_name(const Term &destination) {
    if (!destination.is_tuple() || destination.tuple_size().value_or(0) != 2) {
        return false;
    }
    return need(destination.tuple_element(0)).is_atom() && need(destination.tuple_element(1)).is_atom();
}

// Dest ! Msg and erlang:send(Dest, Msg): Msg, after delivering it to the process of a pid (nothing when that process
// has ended). A {Name, Node} message is dropped: no name is registered (plan step 50), so a bare name is badarg, as
// is every other destination.
TermResult<Term> send(ProcessContext &context, const Term &destination, const Term &message) {
    if (destination.is_pid()) {
        if (const auto sent = detail::Executor::send(context, destination.word(), message); !sent) {
            throw BuiltinFailure{.term = sent.error()};
        }
        return message;
    }
    if (!remote_name(destination)) {
        bad_argument();
    }
    return message;
}

constexpr std::array PROCESS_BUILTINS{
    typed_entry<self>("erlang", "self"),
    typed_entry<make_ref>("erlang", "make_ref"),
    typed_entry<spawn_fun>("erlang", "spawn"),
    typed_entry<spawn_call>("erlang", "spawn"),
    typed_entry<is_process_alive>("erlang", "is_process_alive"),
    typed_entry<send>("erlang", "!"),
    typed_entry<send>("erlang", "send"),
};
} // namespace
} // namespace erlang_aot::runtime::builtins

namespace erlang_aot::runtime {
std::span<const BuiltinEntry> process_builtins() noexcept { return builtins::PROCESS_BUILTINS; }
} // namespace erlang_aot::runtime
