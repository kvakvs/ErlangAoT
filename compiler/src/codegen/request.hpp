#pragma once
#include "output.hpp"
#include <cstdint>
#include <erlang_aot/compiler/ast/module.hpp>
#include <filesystem>
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
    // Retain immutable syntax and its source provenance beyond the parsing session.
    ast::Module syntax;
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
    // Attach project diagnostic context without interpreting it as a machine target.
    std::string project_target;
    // Reserve target selection for step 4; empty means the eventual native default.
    std::string target_triple;
    // Carry the requested pipeline policy without performing optimization yet.
    OptimizationLevel optimization = OptimizationLevel::none;
    // Keep the specialization override independent of option ordering.
    bool disable_type_specialization = false;
    // Select future in-memory serialization; filesystem publication belongs to the driver.
    OutputKind output_kind = OutputKind::object;
};
} // namespace erlang_aot::codegen
