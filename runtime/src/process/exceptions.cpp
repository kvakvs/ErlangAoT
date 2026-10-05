#include "exceptions.hpp"
#include "../terms/service_errors.hpp"
#include "terms.hpp"
#include <array>
#include <erlang_aot/abi/calls.hpp>
#include <new>

namespace erlang_aot::runtime::detail {
namespace {
using abi::v1::ErrorReason;
using abi::v1::Status;

// Build the value of `catch Expr`: a thrown term as is, {'EXIT', Reason} for an exit and
// {'EXIT', {Reason, Stack}} for an error; Stack stays [] until stack traces exist (plan step 15).
TermResult<Term> catch_value(ProcessContext &context, const CallFailure &failure) {
    auto reason = exception_reason_term(context, failure);
    if (!reason || failure.reason == ErrorReason::raised_throw) {
        return reason;
    }
    TermFactory factory(context);
    if (failure.reason != ErrorReason::raised_exit) {
        const auto stack = factory.nil();
        if (!stack) {
            return stack;
        }
        reason = factory.tuple(std::array{*reason, *stack});
        if (!reason) {
            return reason;
        }
    }
    const auto tag = factory.atom("EXIT");
    if (!tag) {
        return tag;
    }
    return factory.tuple(std::array{*tag, *reason});
}

// Build the class atom and reason handed to a try ... catch handler.
TermResult<std::array<Term, 2>> class_and_reason(ProcessContext &context, const CallFailure &failure) {
    auto reason = exception_reason_term(context, failure);
    if (!reason) {
        return std::unexpected(reason.error());
    }
    auto name = TermFactory(context).atom(exception_class(failure));
    if (!name) {
        return std::unexpected(name.error());
    }
    return std::array{*name, *reason};
}

// Contain allocation and other native exceptions while building terms from a pending exception.
template <typename Build>
auto guarded(Build build, ProcessContext &context, const CallFailure &failure) noexcept
    -> decltype(build(context, failure)) {
    try {
        return build(context, failure);
    } catch (const std::bad_alloc &) {
        return std::unexpected(TermError::out_of_memory);
    } catch (...) {
        return std::unexpected(TermError::diagnostic_failure);
    }
}

// Take a pending Erlang exception as terms and clear the channel; halts and infrastructure failures stay pending.
template <typename Build, typename Store> Status take_exception(ProcessContext &context, Build build, Store store) {
    auto &state = context.generated_calls();
    const auto &failure = state.failure();
    if (!state.active() || !failure) {
        return Status::invalid_argument;
    }
    if (failure->code != CallError::erlang_exception) {
        return Status::stopped;
    }
    const auto value = guarded(build, context, *failure);
    state.clear();
    if (!value) {
        // The exception cannot be turned into terms, so the invocation stops with the infrastructure status.
        state.fail_service(term_status(value.error()));
        return term_status(value.error());
    }
    store(*value);
    return Status::ok;
}

// Map a class atom back to the raised reason that carries it; any other term is not a class.
std::optional<ErrorReason> raised_reason(const Term &name) {
    if (!name.is_atom()) {
        return {};
    }
    const auto spelling = name.atom_spelling();
    if (!spelling) {
        return {};
    }
    if (*spelling == "error") {
        return ErrorReason::raised_error;
    }
    if (*spelling == "exit") {
        return ErrorReason::raised_exit;
    }
    return *spelling == "throw" ? std::optional{ErrorReason::raised_throw} : std::nullopt;
}

// Record Class:Reason as the pending exception; an invalid class or term records badarg instead.
Status reraise(ProcessContext &context, const Word exception_class, const Word reason) noexcept {
    auto &state = context.generated_calls();
    if (!state.active()) {
        return Status::invalid_argument;
    }
    const auto name = Term::from_word(exception_class, context);
    const auto value = Term::from_word(reason, context);
    const auto raised = name ? raised_reason(*name) : std::nullopt;
    if (!raised || !value) {
        state.fail({.code = CallError::erlang_exception, .reason = ErrorReason::badarg});
        return Status::ok;
    }
    state.fail({.code = CallError::erlang_exception, .reason = raised, .value = *value});
    return Status::ok;
}
} // namespace

std::string_view exception_class(const CallFailure &failure) {
    if (failure.reason == ErrorReason::raised_exit) {
        return "exit";
    }
    return failure.reason == ErrorReason::raised_throw ? "throw" : "error";
}

std::string_view error_name(const ErrorReason reason) noexcept {
    // Raised reasons (11-13) carry their whole reason term and have no name.
    static constexpr std::array<std::string_view, 15> names{
        "",          "function_clause", "badmatch",  "badarg", "badarg", "badarith", "badmap",    "badkey",
        "badrecord", "case_clause",     "if_clause", "",       "",       "",         "try_clause"};
    const auto index = static_cast<std::size_t>(reason);
    return index < names.size() ? names[index] : std::string_view{};
}

TermResult<Term> exception_reason_term(ProcessContext &context, const CallFailure &failure) {
    const auto name = failure.reason ? error_name(*failure.reason) : std::string_view{};
    if (name.empty()) {
        // Raised reasons (error/exit/throw) are the whole payload term.
        if (!failure.reason || !failure.value) {
            return std::unexpected(TermError::invalid_argument);
        }
        return *failure.value;
    }
    TermFactory factory(context);
    auto atom = factory.atom(name);
    if (!atom || !failure.value) {
        return atom;
    }
    return factory.tuple(std::array{*atom, *failure.value});
}
} // namespace erlang_aot::runtime::detail

std::uint8_t erlang_aot_catch_v1(void *context, erlang_aot::abi::v1::TermWord *output) noexcept {
    using namespace erlang_aot::runtime;
    if (!context || !output) {
        return static_cast<std::uint8_t>(erlang_aot::abi::v1::Status::invalid_argument);
    }
    const auto store = [output](const Term &value) { *output = value.word(); };
    return static_cast<std::uint8_t>(
        detail::take_exception(*static_cast<ProcessContext *>(context), detail::catch_value, store));
}

std::uint8_t erlang_aot_exception_v1(void *context, erlang_aot::abi::v1::TermWord *exception_class,
                                     erlang_aot::abi::v1::TermWord *reason) noexcept {
    using namespace erlang_aot::runtime;
    if (!context || !exception_class || !reason) {
        return static_cast<std::uint8_t>(erlang_aot::abi::v1::Status::invalid_argument);
    }
    const auto store = [exception_class, reason](const std::array<Term, 2> &value) {
        *exception_class = value[0].word();
        *reason = value[1].word();
    };
    return static_cast<std::uint8_t>(
        detail::take_exception(*static_cast<ProcessContext *>(context), detail::class_and_reason, store));
}

std::uint8_t erlang_aot_reraise_v1(void *context, erlang_aot::abi::v1::TermWord exception_class,
                                   erlang_aot::abi::v1::TermWord reason) noexcept {
    if (!context) {
        return static_cast<std::uint8_t>(erlang_aot::abi::v1::Status::invalid_argument);
    }
    return static_cast<std::uint8_t>(erlang_aot::runtime::detail::reraise(
        *static_cast<erlang_aot::runtime::ProcessContext *>(context), exception_class, reason));
}
