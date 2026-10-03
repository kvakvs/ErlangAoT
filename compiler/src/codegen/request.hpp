#pragma once
#include "../implementation_debug.hpp"
#include "output.hpp"
#include <cstdint>
#include <erlang_aot/compiler/ast/module.hpp>
#include <filesystem>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace erlang_aot::codegen {
enum class OptimizationLevel : std::uint8_t { none, speed };

struct CompilationInput {
    // Transfer syntax and its original path into the batch without copying AST ownership.
    CompilationInput(std::filesystem::path path, ast::Module module)
        : source_path(std::move(path)), syntax(std::move(module)) {}

    CompilationInput(CompilationInput &&) noexcept = default;
    CompilationInput &operator=(CompilationInput &&) noexcept = default;
    CompilationInput(const CompilationInput &) = delete;
    CompilationInput &operator=(const CompilationInput &) = delete;
    ~CompilationInput() = default;
    // Preserve the original native filename for diagnostics and future artifact planning.
    std::filesystem::path source_path;
    // Mark sources that began with a "#!" escript header (implicit main/1 export, escript exit codes).
    bool escript = false;
    // Retain immutable syntax and its source provenance beyond the parsing session.
    ast::Module syntax;
};

struct CompilationProgress {
    // Own one synchronous event's phase and original source/module context.
    std::string phase;
    std::filesystem::path source_path;
    std::string module_name;
    // Explain specialization policy or phase details without exposing LLVM objects.
    std::string detail;
};

using ProgressCallback = std::function<void(const CompilationProgress &)>;

struct CompilationLimits {
    // Bound retained module owners and total syntax work before semantic/backend traversal.
    std::size_t modules = 1024;
    std::size_t module_nodes = 250000;
    std::size_t batch_nodes = 1000000;
    // Stop serialization before buffers exceed per-module or aggregate byte budgets.
    std::size_t module_bytes = 64 * 1024 * 1024;
    std::size_t batch_bytes = 256 * 1024 * 1024;
};

struct CompilationRequest {
    // Create an empty batch, then transfer it as a single owner into compilation.
    CompilationRequest() = default;
    CompilationRequest(CompilationRequest &&) noexcept = default;
    CompilationRequest &operator=(CompilationRequest &&) noexcept = default;
    CompilationRequest(const CompilationRequest &) = delete;
    CompilationRequest &operator=(const CompilationRequest &) = delete;
    ~CompilationRequest() = default;
    // Own one ordered batch; project targets must supply separate requests.
    std::vector<CompilationInput> inputs;
    // Keep injectable internal ceilings separate from language and optimization policy.
    CompilationLimits limits;
    // Attach project diagnostic context without interpreting it as a machine target.
    std::string project_target;
    // Observe only started phases; absent observers keep ordinary compilation silent.
    ProgressCallback progress;
    // Carry opt-in implementation-step diagnostics without coupling them to ordinary tracing.
    ImplementationDebug implementation_debug;
    // Select a normalized LLVM triple; empty prefers the running host's triple, CPU and features.
    std::string target_triple;
    // Carry the requested pipeline policy without performing optimization yet.
    OptimizationLevel optimization = OptimizationLevel::none;
    // Keep the specialization override independent of option ordering.
    bool disable_type_specialization = false;
    // Preserve instruction locations for source comments in requested human-readable IR.
    bool annotate_source = false;
    // Select future in-memory serialization; filesystem publication belongs to the driver.
    OutputKind output_kind = OutputKind::object;
};
} // namespace erlang_aot::codegen
