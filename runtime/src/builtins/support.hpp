#pragma once
#include "../terms/service_errors.hpp"
#include <erlang_aot/abi/calls.hpp>
#include <erlang_aot/abi/term.hpp>
#include <erlang_aot/runtime/builtin_registry.hpp>
#include <erlang_aot/runtime/process_context.hpp>
#include <optional>

// Helpers shared by builtin implementations: argument admission, error raising and result publication.
namespace erlang_aot::runtime::builtins {
using Arguments = std::span<const Word>;

// Record an Erlang error with an optional payload, as generated code does; returns the unused result word.
inline Word raise(ProcessContext &context, abi::v1::ErrorReason reason, Word payload = 0) {
    erlang_aot_raise_v2(&context, reason, payload);
    return 0;
}

// Record badarg; returns the unused result word.
inline Word badarg(ProcessContext &context) { return raise(context, abi::v1::ErrorReason::badarg); }

// Whether a failure is already recorded; a service outcome then needs no error of its own.
inline bool failed(ProcessContext &context) { return context.generated_calls().failure().has_value(); }

// Admit an argument word as a term of this process, recording the failure when it is not one.
inline std::optional<Term> admit(ProcessContext &context, Word word) {
    auto term = Term::from_word(word, context);
    if (!term) {
        context.generated_calls().fail_service(detail::term_status(term.error()));
        return std::nullopt;
    }
    return std::move(*term);
}

// The result word of a constructed term, or 0 after recording why construction failed.
inline Word publish(ProcessContext &context, const TermResult<Term> &result) {
    if (!result) {
        context.generated_calls().fail_service(detail::term_status(result.error()));
        return 0;
    }
    return result->word();
}

// The value of a small integer word, as OTP's is_small; none for anything else.
inline std::optional<std::int64_t> small(Word word) {
    const auto value = abi::v1::NativeIntegerEncoding::decode(word);
    return value ? std::optional{*value} : std::nullopt;
}
} // namespace erlang_aot::runtime::builtins
