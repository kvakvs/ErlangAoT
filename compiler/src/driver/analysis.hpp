#pragma once
#include "../codegen/request.hpp"
#include "../semantic/types/contracts.hpp"

namespace erlang_aot::cli {
struct Analysis {
    // Own side tables while the input batch retains every borrowed AST and source location.
    std::vector<std::unique_ptr<semantic::Module>> modules;
    semantic::CallGraph calls;
    std::unique_ptr<semantic::types::Registry> declared;
    std::unique_ptr<semantic::types::Inference> inferred;
};

// Run declaration, binding, call, declared-type and inference phases without creating LLVM state.
bool analyze(std::span<const codegen::CompilationInput> inputs, Analysis &analysis, const ImplementationDebug &debug,
             const DiagnosticSink &sink);
} // namespace erlang_aot::cli
