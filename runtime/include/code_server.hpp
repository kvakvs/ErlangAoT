#pragma once

// REVIEW SKETCH ONLY: loaded-module registry and MFA resolution, without a file loader or hot upgrades.
// Each loaded module owns one immutable ModuleRegistry of std::function targets.
#include "callable.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace erlang_aot::runtime {
// Keep registration and name-resolution failures separate from invoking a found function.
enum class CodeError : std::uint8_t {
    invalid_module,
    invalid_export,
    duplicate_module,
    duplicate_export,
    module_not_found,
    function_not_exported,
    resource_limit,
    stopped
};
template <typename Value> using CodeResult = std::expected<Value, CodeError>;

// Retain executable memory, immutable module data and loader handles while code can be called.
class CodeImage {
  public:
    // Create a lifetime anchor for code statically linked into the runtime executable.
    static std::shared_ptr<const CodeImage> linked();
    // Release resources only after module registrations and active calls have gone away.
    virtual ~CodeImage() = default;
};

// Assemble a complete module privately before publishing any of its exports.
struct ModuleDefinition final {
    // Supply the module atom on its owner thread; the published registry owns independent name metadata.
    Term name;
    // Declare before functions so targets/captures die before their code image is released.
    std::shared_ptr<const CodeImage> image;
    // Transfer the sole function registry into the loaded module; null is invalid.
    std::unique_ptr<ModuleRegistry> functions;
};

// Describe an export without exposing executable addresses or mutable registration state.
struct ExportName final {
    // Root the exported function atom in the context requesting this listing.
    Term function;
    std::size_t arity; // Argument count
};

// Hold an immutable published module and its code-image ownership.
class LoadedModule final {
  public:
    // Destroy the function registry before executable memory and module-local metadata.
    ~LoadedModule();
    // Prevent copying mutable ownership internals; use shared immutable module handles.
    LoadedModule(const LoadedModule &) = delete;
    LoadedModule &operator=(const LoadedModule &) = delete;
    // Inspect stable module spelling for the duration of this module handle.
    std::string_view name() const noexcept;
    // Retain the runtime-bound module atom while this loaded module is alive.
    Term name_atom() const noexcept;
    // Expose the sole frozen registry; borrowed targets require this module handle to stay alive.
    const ModuleRegistry &functions() const noexcept;
    // Materialize a deterministic export listing with atom names rooted in the supplied caller context.
    TermResult<std::vector<ExportName>> exports(ProcessContext &context) const;

  private:
    friend class CodeServer;
    friend class ResolvedFunction;
    // Take the unique registry, freeze it and retain the validated module metadata/code image.
    explicit LoadedModule(ModuleDefinition definition);
    // Own code storage, module atom bindings and exactly one unique_ptr<ModuleRegistry>.
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// Pin a module while invoking its default all-Term registration.
class ResolvedFunction final {
  public:
    // Report the fixed Erlang arity selected during lookup.
    std::size_t arity() const noexcept;
    // Validate arity/ownership, call once and translate host exceptions without any conversions.
    CallResult<Term> call(ProcessContext &context, std::span<const Term> arguments) const;

  private:
    friend class CodeServer;
    // Keep the registry, module atom bindings and executable storage alive throughout invocation.
    std::shared_ptr<const LoadedModule> module_;
    // Borrow the generic target from the immutable registry pinned by module_.
    const Callable *target_;
    // Check the dynamic argument span before entering the target.
    std::size_t arity_;
    // Construct only after successful lookup of an all-Term registration in the retained module.
    ResolvedFunction(std::shared_ptr<const LoadedModule> module, const Callable *target, std::size_t arity);
};

// Store the runtime's currently published modules, keyed by exact decoded module names.
class CodeServer final {
  public:
    // Create an empty registry; the runtime owns one server shared by its scheduler workers.
    CodeServer();
    // Close registration and release registry references after process workers have stopped.
    ~CodeServer();
    // Keep registry identity and synchronization unique within the runtime.
    CodeServer(const CodeServer &) = delete;
    CodeServer &operator=(const CodeServer &) = delete;

    // Atomically publish a validated module; duplicate names fail instead of replacing live code.
    CodeResult<std::shared_ptr<const LoadedModule>> load(ModuleDefinition definition);
    // Resolve the default all-Term target; typed lookup uses find_module()->functions().find_typed<...>().
    CodeResult<ResolvedFunction> resolve(std::string_view module, std::string_view function, std::size_t arity) const;
    // Resolve live atom terms on their owner thread, extracting the same exact name keys as string lookup.
    CodeResult<ResolvedFunction> resolve(const Term &module, const Term &function, std::size_t arity) const;
    // Retain one immutable module snapshot or report module_not_found.
    CodeResult<std::shared_ptr<const LoadedModule>> find_module(std::string_view name) const;
    // Copy a name-sorted snapshot of currently published modules without exposing the registry map.
    std::vector<std::shared_ptr<const LoadedModule>> loaded_modules() const;
    // Remove a module from future lookups; existing module/resolution handles still pin its code.
    CodeResult<void> unload(std::string_view module);

  private:
    // Own a synchronized name-to-module map and registry resource/admission limits.
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace erlang_aot::runtime
