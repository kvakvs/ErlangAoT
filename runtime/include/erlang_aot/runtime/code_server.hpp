#pragma once
#include "callable.hpp"
#include "features.hpp"

namespace erlang_aot::runtime {
enum class CodeError : std::uint8_t {
    invalid_module,
    invalid_export,
    duplicate_module,
    module_not_found,
    function_not_exported,
    resource_limit
};
template <typename Value> using CodeResult = std::expected<Value, CodeError>;

// Retain linked executable storage until all targets and captures have been destroyed.
class CodeImage {
  public:
    // Create a lifetime anchor for code already linked into the host program.
    static std::shared_ptr<const CodeImage> linked();
    // Future loader subclasses release their executable storage here.
    virtual ~CodeImage() = default;
};

struct ModuleDefinition final {
    // Own exact module spelling; runtime atom metadata is deferred to step 28.
    std::string name;
    // Declaration order keeps executable memory alive through target destruction.
    std::shared_ptr<const CodeImage> image;
    // Transfer the sole mutable registry into its published module.
    std::unique_ptr<ModuleRegistry> functions;
};

// Pin one immutable registry and its code image independently of future lookup access.
class LoadedModule final {
  public:
    // Release targets before their executable image, including callable capture destructors.
    ~LoadedModule() = default;
    LoadedModule(const LoadedModule &) = delete;
    LoadedModule &operator=(const LoadedModule &) = delete;
    // Borrow the module spelling while retaining this handle.
    std::string_view name() const noexcept;
    // Borrow the single frozen registry; direct calls require this module handle to remain live.
    const ModuleRegistry &functions() const noexcept;

  private:
    friend class CodeServer;
    // Adopt a validated draft without copying targets or their mutable capture state.
    explicit LoadedModule(ModuleDefinition definition);
    // Preserve image-before-registry declaration order for safe destruction.
    ModuleDefinition definition_;
};

class ResolvedFunction final {
  public:
    // Report the fixed arity selected by generic lookup.
    std::size_t arity() const noexcept;
    // Check immediate arguments/result and contain host exceptions; unavailable bodies report once.
    CallResult<Term> call(ProcessContext &context, std::span<const Term> arguments,
                          DiagnosticSink sink = {}) const noexcept;

  private:
    friend class CodeServer;
    // Pin the registry and image before borrowing a stable target address from them.
    ResolvedFunction(std::shared_ptr<const LoadedModule> module, const Callable *target, std::size_t arity);
    // Retain immutable code and targets even after their runtime is destroyed.
    std::shared_ptr<const LoadedModule> module_;
    // Borrow exactly one stored generic target without copying captured state.
    const Callable *target_;
    // Check span length before any target invocation.
    std::size_t arity_;
};

struct FunctionRequest final {
    // Borrow exact module/function names for one synchronous lookup.
    std::string_view module;
    std::string_view function;
    // Select the fixed generic signature without interpreting argument values.
    std::size_t arity;
};

// Runtime-owned publication service; mutation and lookup require host serialization for now.
class CodeServer final {
  public:
    // Create an empty registry; native registration never loads files or performs hot upgrades.
    CodeServer() = default;
    CodeServer(const CodeServer &) = delete;
    CodeServer &operator=(const CodeServer &) = delete;
    ~CodeServer() = default;
    // Publish a complete registry transactionally; duplicates never replace existing code.
    CodeResult<std::shared_ptr<const LoadedModule>> load(ModuleDefinition definition);
    // Select only the exact all-Term entry and return a module-pinning call handle.
    CodeResult<ResolvedFunction> resolve(FunctionRequest request) const;
    // Retain immutable module ownership or report a missing module.
    CodeResult<std::shared_ptr<const LoadedModule>> find_module(std::string_view name) const;

  private:
    // One registry per exact module spelling; no secondary BIF overload table exists.
    std::map<std::string, std::shared_ptr<const LoadedModule>, std::less<>> modules_;
};
} // namespace erlang_aot::runtime
