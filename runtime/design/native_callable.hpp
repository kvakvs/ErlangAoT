#pragma once

// REVIEW SKETCH ONLY: signature constraints are checked; adapters/converters have no definitions.
// Native calls are bounded synchronous work executed inside scheduler-owned CallFrames.
#include "native_types.hpp"

#include <functional>

namespace erlang_aot::runtime {
// Reject unsupported signatures by leaving the primary template incomplete.
template <typename Signature> class NativeCallable;

// Infer Erlang arity from value parameters, with optional context injection outside that arity.
template <typename Return, NativeArgument... Arguments>
    requires(std::same_as<Return, void> || NativeReturn<Return>)
class NativeCallable<Return(Arguments...)> final : public Callable {
  public:
    // Own a copyable native target; captures may not retain process-local terms or borrowed state.
    using Function = std::function<Return(Arguments...)>;
    // Offer process services and term construction without exposing a ProcessContext as an Erlang argument.
    using ContextFunction = std::function<Return(ProcessContext &, Arguments...)>;
    // Keep exact arity available for registration and compile-time diagnostics.
    static constexpr std::size_t erlang_arity = sizeof...(Arguments);
    // Identify the fallback signature independently of return type or optional context injection.
    static constexpr bool generic_arguments = (std::same_as<Arguments, Term> && ...);

    // Adopt a plain function, function pointer or explicitly typed copyable callable.
    explicit NativeCallable(Function function);
    // Adopt a function with a leading runtime-supplied context reference.
    explicit NativeCallable(ContextFunction function);
    // Release native target captures after all call frames and registrations release them.
    ~NativeCallable() override;
    // Keep registration identity fixed; shared ownership replaces copying adapters.
    NativeCallable(const NativeCallable &) = delete;
    NativeCallable &operator=(const NativeCallable &) = delete;

    // Return sizeof...(Arguments), never container size or the injected context parameter count.
    std::size_t arity() const noexcept override;
    // Publish exact argument type keys; return type never participates in overload resolution.
    std::span<const std::type_index> argument_types() const noexcept override;
    // Prepare all-Term calls only; a typed signature reports argument_type_mismatch rather than decoding.
    CallResult<std::unique_ptr<CallFrame>> prepare(ProcessContext &context, std::span<const Term> arguments,
                                                   ConversionLimits limits = {}) const override;
    // Move an exactly typed payload into the frame; perform no argument conversion or container reconstruction.
    CallResult<std::unique_ptr<CallFrame>> prepare_native(ProcessContext &context, NativeArguments arguments,
                                                          ConversionLimits limits = {}) const override;

  private:
    // Share immutable signature/target registration with frames that outlive the adapter handle.
    class Binding;
    std::shared_ptr<const Binding> binding_;
};
} // namespace erlang_aot::runtime
