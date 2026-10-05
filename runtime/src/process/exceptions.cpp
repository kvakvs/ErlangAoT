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

// Contain allocation and other native exceptions while building the catch value.
TermResult<Term> guarded_catch_value(ProcessContext &context, const CallFailure &failure) noexcept {
    try {
        return catch_value(context, failure);
    } catch (const std::bad_alloc &) {
        return std::unexpected(TermError::out_of_memory);
    } catch (...) {
        return std::unexpected(TermError::diagnostic_failure);
    }
}

// Replace a pending Erlang exception by its catch value; halts and infrastructure failures stay pending.
Status catch_exception(ProcessContext &context, Word *output) noexcept {
    auto &state = context.generated_calls();
    const auto &failure = state.failure();
    if (!output || !state.active() || !failure) {
        return Status::invalid_argument;
    }
    if (failure->code != CallError::erlang_exception) {
        return Status::stopped;
    }
    const auto value = guarded_catch_value(context, *failure);
    state.clear();
    if (!value) {
        // The exception cannot be turned into a value, so the invocation stops with the infrastructure status.
        state.fail_service(term_status(value.error()));
        return term_status(value.error());
    }
    *output = value->word();
    return Status::ok;
}
} // namespace

std::string_view error_name(const ErrorReason reason) noexcept {
    static constexpr std::array<std::string_view, 11> names{"",          "function_clause", "badmatch", "badarg",
                                                            "badarg",    "badarith",        "badmap",   "badkey",
                                                            "badrecord", "case_clause",     "if_clause"};
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
    if (!context) {
        return static_cast<std::uint8_t>(erlang_aot::abi::v1::Status::invalid_argument);
    }
    return static_cast<std::uint8_t>(erlang_aot::runtime::detail::catch_exception(
        *static_cast<erlang_aot::runtime::ProcessContext *>(context), output));
}
