#include "analysis.hpp"
#include "../project/paths.hpp"
#include "../semantic/bindings.hpp"
#include "../semantic/capabilities.hpp"

namespace erlang_aot::cli {
namespace {
// Establish all owned declaration tables before resolving inter-module references.
void index_inputs(std::span<const codegen::CompilationInput> inputs, Analysis &analysis,
                  const semantic::Reporter &report) {
    for (const auto &input : inputs) {
        auto module = semantic::index(input.syntax, project::path_text(input.source_path), report);
        semantic::check_capabilities(*module, report);
        semantic::bind_parameters(*module, report);
        analysis.modules.push_back(std::move(module));
    }
}

// Preserve existing implementation-step summaries independently of normal progress tracing.
void debug_inference(const Analysis &analysis, const ImplementationDebug &debug, const DiagnosticSink &sink) {
    for (const auto step : {23, 24, 25, 26, 27}) {
        if (debug.enabled(step)) {
            semantic::types::trace_inference(*analysis.inferred, analysis.calls, sink, step);
        }
    }
}
} // namespace

bool analyze(std::span<const codegen::CompilationInput> inputs, Analysis &analysis, const ImplementationDebug &debug,
             const DiagnosticSink &sink) {
    bool failed = false;
    const semantic::Reporter report = [&](const Diagnostic &diagnostic) {
        failed = failed || diagnostic.severity == Severity::error;
        const auto prefix = diagnostic.severity == Severity::warning ? "warning: " : "error: ";
        sink(prefix + render(diagnostic));
    };
    index_inputs(inputs, analysis, report);
    if (failed) {
        return false;
    }
    analysis.calls = semantic::resolve_calls(analysis.modules, report);
    if (failed) {
        return false;
    }
    analysis.declared = semantic::types::resolve_declarations(analysis.modules, report);
    if (failed) {
        return false;
    }
    analysis.inferred = semantic::types::infer(analysis.calls);
    semantic::types::check_contracts(*analysis.declared, *analysis.inferred, analysis.calls, report);
    debug_inference(analysis, debug, sink);
    return !failed;
}
} // namespace erlang_aot::cli
