#pragma once
#include "terms.hpp"
#include <functional>
#include <map>
#include <typeindex>

namespace erlang_aot::runtime {
// Separate invocation failures from registration and lookup failures.
enum class CallError : std::uint8_t {
    bad_arity,
    argument_type_mismatch,
    wrong_owner,
    expired_context,
    resource_limit,
    native_exception,
    not_implemented,
    diagnostic_failure
};

struct CallFailure final {
    // Preserve the operation failure without manufacturing an Erlang exception term.
    CallError code;
    // Identify an argument failure, or leave empty for a result/body failure.
    std::optional<std::size_t> argument = {};
    // Retain explicit term validation errors for host callers.
    std::optional<TermError> term_error = {};
    // Prevent nested checked calls from reporting the same unavailable body again.
    bool reported = false;
};

template <typename Value> using CallResult = std::expected<Value, CallFailure>;
// Generic entries always receive exactly their registered Erlang arity, with no conversions.
using Callable = std::function<CallResult<Term>(ProcessContext &, std::span<const Term>)>;

enum class RegistryError : std::uint8_t { invalid_entry, duplicate_key, function_not_found, frozen, resource_limit };
template <typename Value> using RegistryResult = std::expected<Value, RegistryError>;

struct FunctionKey final {
    // Own exact spelling independently of draft buffers; atom bindings arrive in step 28.
    std::string name;
    // Exclude the leading process context from Erlang arity and type identity.
    std::size_t arity;
    // Generic keys contain one typeid(Term) per argument; native extensions must match exactly.
    std::vector<std::type_index> argument_types;
    // Order canonical signatures without normalizing names or native type identities.
    auto operator<=>(const FunctionKey &) const = default;
};

// Build one registry privately, then freeze that same owner at module publication.
class ModuleRegistry final {
  public:
    // Start a mutable, empty draft; its address stays stable through publication.
    ModuleRegistry() = default;
    ModuleRegistry(const ModuleRegistry &) = delete;
    ModuleRegistry &operator=(const ModuleRegistry &) = delete;
    ~ModuleRegistry() = default;
    // Add only all-Term entries; invalid targets, arities above 255 and duplicate signatures fail.
    RegistryResult<void> add(std::string_view name, std::size_t arity, Callable &&target);
    // Borrow the exact generic target; callers retain its registry/module through use.
    RegistryResult<const Callable *> find(std::string_view name, std::size_t arity) const;
    // Copy canonical signatures in stable name/arity order.
    std::vector<FunctionKey> keys() const;
    // Observe publication without exposing mutable entries.
    bool frozen() const noexcept;

  private:
    friend class CodeServer;
    // Reject subsequent additions, including through surviving draft aliases.
    void freeze() noexcept;
    // Share one stored callable instance across every lookup of its exact key.
    std::map<FunctionKey, Callable> entries_;
    // Change only after successful publication; draft mutation requires host serialization.
    bool frozen_ = false;
};
} // namespace erlang_aot::runtime
