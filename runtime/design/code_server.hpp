#pragma once

// REVIEW SKETCH ONLY: loaded-module registry and MFA resolution, without a file loader or hot upgrades.
// Both native adapters and future generated-code adapters implement Callable.
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
    // Release resources only after module registrations and active call frames have gone away.
    virtual ~CodeImage() = default;
};

// Register one argument-signature variant; name/arity may repeat for distinct native signatures.
struct FunctionRegistration final {
    // Supply a live atom term; registration extracts its spelling without retaining a process root.
    Term name;
    // Retain a native or generated adapter; null registration is invalid.
    std::shared_ptr<const Callable> callable;
};

// Assemble a complete module privately before publishing any of its exports.
struct ModuleDefinition final {
    // Supply the module atom on its owner thread; the published registry owns independent name metadata.
    Term name;
    // Declare before exports so adapters/captures die before their code image is released.
    std::shared_ptr<const CodeImage> image;
    // Supply unique function/arity/argument-type variants, including an optional all-Term fallback.
    std::vector<FunctionRegistration> exports;
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
    // Destroy adapters before executable memory and module-local metadata.
    ~LoadedModule();
    // Prevent copying mutable ownership internals; use shared immutable module handles.
    LoadedModule(const LoadedModule &) = delete;
    LoadedModule &operator=(const LoadedModule &) = delete;
    // Inspect stable module spelling for the duration of this module handle.
    std::string_view name() const noexcept;
    Term name_atom() const noexcept;
    // Materialize a deterministic export listing with atom names rooted in the supplied caller context.
    TermResult<std::vector<ExportName>> exports(ProcessContext &context) const;

  private:
    friend class CodeServer;
    friend class ResolvedFunction;
    // Freeze a validated definition into a name/arity lookup table and retained CodeImage.
    explicit LoadedModule(ModuleDefinition definition);
    // Own code storage, adapters and immutable export index, with no process-heap terms.
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// Retain one exported name/arity and its registered variants; each call selects by its supplied argument types.
class ResolvedFunction final {
  public:
    // Report the resolved export family's fixed Erlang arity.
    std::size_t arity() const noexcept;
    // Select the all-Term registration for boxed arguments; never inspect values to choose a typed variant.
    CallResult<std::unique_ptr<CallFrame>> prepare(ProcessContext &context, std::span<const Term> arguments,
                                                   ConversionLimits limits = {}) const;
    // Select an exact native signature, else require an all-Term registration and explicit fallback arguments.
    CallResult<std::unique_ptr<CallFrame>>
    prepare_native(ProcessContext &context, NativeArguments arguments,
                   std::optional<std::span<const Term>> generic_arguments = std::nullopt,
                   ConversionLimits limits = {}) const;

  private:
    friend class CodeServer;
    // Pin immutable module storage before retaining its selected export adapter.
    std::shared_ptr<const LoadedModule> module_;
    // Retain an immutable exact-signature index and the optional all-Term fallback for this name/arity.
    class Overloads;
    std::shared_ptr<const Overloads> overloads_;
    // Construct only after successful export-family lookup in a retained module snapshot.
    ResolvedFunction(std::shared_ptr<const LoadedModule> module, std::shared_ptr<const Overloads> overloads);
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
    // Resolve module:function/arity to its registered signature family; argument selection happens on prepare.
    CodeResult<ResolvedFunction> resolve(std::string_view module, std::string_view function, std::size_t arity) const;
    // Resolve live atom terms on their owner thread, extracting the same exact name keys as string lookup.
    CodeResult<ResolvedFunction> resolve(const Term &module, const Term &function, std::size_t arity) const;
    // Retain one immutable module snapshot or report module_not_found.
    CodeResult<std::shared_ptr<const LoadedModule>> find_module(std::string_view name) const;
    // Copy a name-sorted snapshot of currently published modules without exposing the registry map.
    std::vector<std::shared_ptr<const LoadedModule>> loaded_modules() const;
    // Remove a module from future lookups; existing resolutions/frames still pin its code.
    CodeResult<void> unload(std::string_view module);

  private:
    // Own a synchronized name-to-module map and registry resource/admission limits.
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace erlang_aot::runtime
