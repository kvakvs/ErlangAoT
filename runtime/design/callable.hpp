#pragma once

// REVIEW SKETCH ONLY: scheduler-facing call frames, not an implemented generated-code ABI.
// See code_server.md for argument ownership, suspension and error translation.
#include "terms.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <typeindex>
#include <variant>

namespace erlang_aot::runtime {
class TickBudget;
class NativeArguments;

// Separate invocation failures from module lookup and from successful Erlang values.
enum class CallError : std::uint8_t {
    bad_arity,
    argument_type_mismatch,
    no_compatible_callable,
    generic_arguments_required,
    conversion_failed,
    wrong_owner,
    expired_context,
    resource_limit,
    native_exception,
    invalid_frame,
    not_implemented
};

// Locate failed conversion without retaining argument terms or native exception objects.
struct CallFailure final {
    // Select the runtime/compiler error translation path.
    CallError code;
    // Zero-based argument index; absent for result conversion or non-argument failures.
    std::optional<std::size_t> argument;
    // Preserve a checked term conversion cause such as wrong_type or out_of_range.
    std::optional<TermError> term_error;
};

template <typename Value> using CallResult = std::expected<Value, CallFailure>;

// Return a scheduler suspension or a completed, caller-owned rooted value.
enum class CallSuspension : std::uint8_t { yielded, waiting };
using CallProgress = std::variant<CallSuspension, Term>;

// Bound explicit codec work and result encoding; dispatch never converts arguments.
struct ConversionLimits final {
    // Count sequence elements across explicit argument conversion and result encoding.
    std::size_t max_elements = 4096;
    // Bound nesting before entering another list/container conversion.
    std::size_t max_depth = 32;
    // Limit total copied string/container payload bytes, using checked arithmetic.
    std::size_t max_bytes = std::size_t{1} * 1024 * 1024;
};

// Retain per-invocation roots and continuation state; one process owns each frame.
class CallFrame {
  public:
    // Release arguments/temporaries and cancel waiters before the owning process heap dies.
    virtual ~CallFrame() = default;
    // Run under a scheduler tick grant; completed/failed frames cannot be resumed again.
    virtual CallResult<CallProgress> resume(ProcessContext &context, TickBudget &budget) = 0;
};

// Erase one registered argument signature; several signatures may share an Erlang name/arity.
class Callable {
  public:
    // Destroy registration state only after all module/frame owners release it.
    virtual ~Callable() = default;
    // Report Erlang argument count, excluding an optional injected ProcessContext.
    virtual std::size_t arity() const noexcept = 0;
    // Expose exact C++ value types in argument order; generated generic entries report all Term.
    virtual std::span<const std::type_index> argument_types() const noexcept = 0;
    // Accept boxed arguments only for an all-Term signature, without decoding them to native values.
    virtual CallResult<std::unique_ptr<CallFrame>> prepare(ProcessContext &context, std::span<const Term> arguments,
                                                           ConversionLimits limits = {}) const = 0;
    // Accept only an exact native type match, retaining owned values without coercion or codec calls.
    virtual CallResult<std::unique_ptr<CallFrame>> prepare_native(ProcessContext &context, NativeArguments arguments,
                                                                  ConversionLimits limits = {}) const = 0;
};
} // namespace erlang_aot::runtime
