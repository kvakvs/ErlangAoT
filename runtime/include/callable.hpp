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

// Keep RTTI keys exact by admitting unqualified value parameters rather than reference signatures.
template <typename Value>
concept CallableArgument = std::same_as<Value, std::remove_cvref_t<Value>> && std::move_constructible<Value>;

// Pass native values directly; the target explicitly constructs its caller-owned Term result.
template <CallableArgument... Arguments>
using TypedCallable = std::function<CallResult<Term>(ProcessContext &, Arguments...)>;

// Identify one variant within a module; module identity is supplied by the owning registry.
struct FunctionKey final {
    // Own the exact decoded function atom spelling, with no retained process-local root.
    std::string function;
    // Count Erlang arguments, excluding the ProcessContext parameter.
    std::size_t arity;
    // Store one exact type per argument; generic entries contain arity copies of typeid(Term).
    std::vector<std::type_index> argument_types;
};

// Distinguish invalid registration, immutable publication and an absent exact signature.
enum class RegistryError : std::uint8_t {
    invalid_function,
    duplicate_function,
    function_not_found,
    frozen,
    resource_limit
};
template <typename Value> using RegistryResult = std::expected<Value, RegistryError>;

// Build one module's function table privately, then publish it for immutable concurrent lookup.
class ModuleRegistry final {
  public:
    // Start an empty draft; a LoadedModule takes exclusive ownership during publication.
    ModuleRegistry();
    // Release function targets while their owning module's code image is still retained.
    ~ModuleRegistry();
    // Keep registry identity unique; transfer ownership with unique_ptr instead of copying tables.
    ModuleRegistry(const ModuleRegistry &) = delete;
    ModuleRegistry &operator=(const ModuleRegistry &) = delete;
    ModuleRegistry(ModuleRegistry &&) = delete;
    ModuleRegistry &operator=(ModuleRegistry &&) = delete;

    // Register the default all-Term signature; reject an empty target or duplicate key.
    RegistryResult<void> add(const Term &function, std::size_t arity, Callable target);
    // Infer arity and exact argument types directly from a std::function.
    template <CallableArgument... Arguments>
    RegistryResult<void> add(const Term &function, TypedCallable<Arguments...> target);
    // Find the all-Term variant by default; returned targets borrow this registry's lifetime.
    RegistryResult<const Callable *> find(std::string_view function, std::size_t arity) const;
    // Return an exact typed view of the stored target; keep its module alive and request fallback explicitly.
    template <CallableArgument... Arguments>
    RegistryResult<TypedCallable<Arguments...>> find_typed(std::string_view function) const;
    // Copy canonical keys in function/arity/type order for module export inspection.
    std::vector<FunctionKey> keys() const;

  private:
    friend class LoadedModule;
    // Seal the draft before module publication; subsequent additions fail with frozen.
    void freeze();
    // Own the FunctionKey-to-erased-std::function table and publication state, never conversion functions.
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace erlang_aot::runtime
