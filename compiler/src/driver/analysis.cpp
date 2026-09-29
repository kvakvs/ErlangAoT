#include "analysis.hpp"
#include "../codegen/progress.hpp"
#include "../project/paths.hpp"
#include "../semantic/bindings.hpp"
#include "../semantic/capabilities.hpp"

namespace erlang_aot::cli {
namespace {
// Establish all owned declaration tables before resolving inter-module references.
void index_inputs(const codegen::CompilationRequest &request, Analysis &analysis, const semantic::Reporter &report) {
    for (const auto &input : request.inputs) {
        codegen::progress(request, "analysis", input.source_path);
        auto module = semantic::index(input.syntax, project::path_text(input.source_path), report);
        semantic::check_capabilities(*module, report);
        semantic::bind_parameters(*module, report);
        analysis.modules.push_back(std::move(module));
    }
}

// Attach known module identities to each batch analysis phase before invoking its shared implementation.
void trace_analysis(std::string_view phase, const codegen::CompilationRequest &request, const Analysis &analysis) {
    for (std::size_t i = 0; i < analysis.modules.size(); ++i) {
        codegen::progress(request, phase, request.inputs[i].source_path, utf8(analysis.modules[i]->name));
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

bool analyze(const codegen::CompilationRequest &request, Analysis &analysis, const DiagnosticSink &sink) {
    bool failed = false;
    const semantic::Reporter report = [&](const Diagnostic &diagnostic) {
        failed = failed || diagnostic.severity == Severity::error;
        const auto prefix = diagnostic.severity == Severity::warning ? "warning: " : "error: ";
        sink(prefix + render(diagnostic));
    };
    index_inputs(request, analysis, report);
    if (failed) {
        return false;
    }
    trace_analysis("calls", request, analysis);
    analysis.calls = semantic::resolve_calls(analysis.modules, report);
    if (failed) {
        return false;
    }
    trace_analysis("declared-types", request, analysis);
    analysis.declared = semantic::types::resolve_declarations(analysis.modules, report);
    if (failed) {
        return false;
    }
    trace_analysis("inference", request, analysis);
    analysis.inferred = semantic::types::infer(analysis.calls);
    trace_analysis("contracts", request, analysis);
    semantic::types::check_contracts(*analysis.declared, *analysis.inferred, analysis.calls, report);
    debug_inference(analysis, request.implementation_debug, sink);
    return !failed;
}
} // namespace erlang_aot::cli
