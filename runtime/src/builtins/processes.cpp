#include "../scheduler/executor.hpp"
#include "typed.hpp"
#include <array>

// The process builtins of the bridge (docs/builtins.md, docs/processes.md): self/0, make_ref/0, spawn/1,3,
// spawn_link/1,3, is_process_alive/1, sends ('!'/2, send/2), links (link/1, unlink/1), exit signals (exit/2,
// exit_signal/2) and process_flag(trap_exit, Bool).
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

// spawn_link(Fun): spawn(Fun), linked to the caller before the new process runs.
TermResult<Term> spawn_link_fun(ProcessContext &context, const Term &fun) {
    if (!fun.is_function()) {
        bad_argument();
    }
    return detail::Executor::of(context).spawn(context, fun, true);
}

// spawn_link(Module, Function, Args): spawn(Module, Function, Args), linked to the caller.
TermResult<Term> spawn_link_call(ProcessContext &context, const AtomArgument &module, const AtomArgument &function,
                                 const ListArgument &arguments) {
    return detail::Executor::of(context).spawn(context, {module.term, function.term, arguments.term}, true);
}

// The atom `name`.
Term atom(ProcessContext &context, std::string_view name) { return need(TermFactory(context).atom(name)); }

// link(Pid): true once linked; error:noproc for an ended process unless the caller traps exits.
TermResult<Term> link(ProcessContext &context, const Term &pid) {
    if (!pid.is_pid()) {
        bad_argument();
    }
    if (!detail::Executor::of(context).link(context, pid.word())) {
        throw BuiltinFailure{.reason = abi::v1::ErrorReason::raised_error, .payload = atom(context, "noproc").word()};
    }
    return atom(context, "true");
}

// unlink(Pid): true; the link, if any, has no effect from now on.
TermResult<Term> unlink(ProcessContext &context, const Term &pid) {
    if (!pid.is_pid()) {
        bad_argument();
    }
    detail::Executor::unlink(context, pid.word());
    return atom(context, "true");
}

// exit(Dest, Reason) and exit_signal(Dest, Reason): true after sending an exit signal to the process of a pid; a
// reference names no alias here, so nothing is sent. exit/2 to the caller itself with reason normal ends it.
template <bool SelfNormal>
TermResult<Term> exit_signal(ProcessContext &context, const Term &destination, const Term &reason) {
    if (destination.is_pid()) {
        detail::Executor::of(context).exit(context, destination.word(), reason, SelfNormal);
    } else if (!destination.is_reference()) {
        bad_argument();
    }
    return atom(context, "true");
}

// process_flag(trap_exit, Bool): the previous setting; every other flag is badarg.
TermResult<Term> process_flag(ProcessContext &context, const AtomArgument &flag, const Term &value) {
    const auto spelling = value.is_atom() ? value.atom_spelling().value_or("") : "";
    if (flag.term.atom_spelling().value_or("") != "trap_exit" || (spelling != "true" && spelling != "false")) {
        bad_argument();
    }
    return TermFactory(context).boolean(context.signals().set_trap_exit(spelling == "true"));
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
    typed_entry<spawn_link_fun>("erlang", "spawn_link"),
    typed_entry<spawn_link_call>("erlang", "spawn_link"),
    typed_entry<link>("erlang", "link"),
    typed_entry<unlink>("erlang", "unlink"),
    typed_entry<exit_signal<true>>("erlang", "exit"),
    typed_entry<exit_signal<false>>("erlang", "exit_signal"),
    typed_entry<process_flag>("erlang", "process_flag"),
    typed_entry<send>("erlang", "!"),
    typed_entry<send>("erlang", "send"),
};
} // namespace
} // namespace erlang_aot::runtime::builtins

namespace erlang_aot::runtime {
std::span<const BuiltinEntry> process_builtins() noexcept { return builtins::PROCESS_BUILTINS; }
} // namespace erlang_aot::runtime
