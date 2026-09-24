#pragma once

// REVIEW SKETCH ONLY: one function registry per module; registration and lookup have no definitions.
// Targets receive exactly their declared arguments without implicit conversions.
#include "terms.hpp"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <vector>

namespace erlang_aot::runtime {
// Report invocation failures separately from registry lookup failures.
enum class CallError : std::uint8_t {
    bad_arity,
    argument_type_mismatch,
    wrong_owner,
    expired_context,
    resource_limit,
    native_exception,
    not_implemented
};

// Preserve an optional argument position and term error for caller-side error translation.
struct CallFailure final {
    // Identify the failed operation without constructing an Erlang exception term.
    CallError code;
    // Locate a failed argument when available; result failures have no argument index.
    std::optional<std::size_t> argument;
    // Retain the cause of explicit term operations.
    std::optional<TermError> term_error;
};

template <typename Value> using CallResult = std::expected<Value, CallFailure>;

// Use all-Term arguments by default; the registration supplies the span's fixed Erlang arity.
using Callable = std::function<CallResult<Term>(ProcessContext &, std::span<const Term>)>;
} // namespace erlang_aot::runtime