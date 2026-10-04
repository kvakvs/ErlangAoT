#include "backend.hpp"
#include "../codegen/emission.hpp"
#include "../codegen/lowering.hpp"
#include "../codegen/optimization.hpp"
#include "../codegen/serialization.hpp"
#include "../codegen/target.hpp"
#include "../linking/link.hpp"
#include "analysis.hpp"
#include "inspection.hpp"
#include "progress.hpp"
#include "publication.hpp"
#include "type_report.hpp"
#include <erlang_aot/abi/feature_diagnostic.hpp>
#include <erlang_aot/compiler/source.hpp>

namespace erlang_aot::cli {
namespace {
// Carry invocation policy into the private backend without exposing LLVM types to the driver.
codegen::CompilationRequest backend_request(std::vector<codegen::CompilationInput> inputs,
                                            const FrontendRequest &frontend) {
    codegen::CompilationRequest request;
    request.inputs = std::move(inputs);
    request.implementation_debug = frontend.implementation_debug;
    request.project_target = frontend.project_target;
    request.progress = progress_callback(frontend);
    const auto &options = frontend.backend;
    request.target_triple = options.target_triple;
    request.optimization = options.optimization.value_or(codegen::OptimizationLevel::none);
    request.disable_type_specialization = options.disable_type_specialization;
    request.output_kind = options.emit.value_or(codegen::OutputKind::object);
    request.annotate_source = options.inspect_ir() || request.output_kind == codegen::OutputKind::llvm_ir;
    return request;
}

// Preserve severity when presenting owned backend messages.
std::string_view diagnostic_prefix(codegen::DiagnosticLevel level) {
    switch (level) {
    case codegen::DiagnosticLevel::error:
        return "error: ";
    case codegen::DiagnosticLevel::warning:
        return "warning: ";
    case codegen::DiagnosticLevel::note:
        return "note: ";
    }
    return "error: ";
}

// Deliver owned backend diagnostics exactly once through the same positional/project sink.
void report_backend(const codegen::CompilationResult &result, const DiagnosticSink &sink) {
    for (const auto &diagnostic : result.diagnostics()) {
        if (!diagnostic.reported) {
            sink(std::string(diagnostic_prefix(diagnostic.level)) + diagnostic.message);
        }
    }
    if (result.diagnostic_capture_failed()) {
        sink("error: backend diagnostic capture failed");
    }
}

// Keep object/IR serialization in memory until every module has successfully completed its phases.
bool emit(codegen::Compilation &compilation) {
    const auto kind = compilation.request().output_kind;
    return kind == codegen::OutputKind::object ? codegen::emit_objects(compilation)
                                               : codegen::emit_ir(compilation, kind);
}

// Queue complete project batches or publish a successful positional invocation immediately.
void deliver(codegen::Compilation compilation, const FrontendRequest &frontend) {
    if (!frontend.backend.emit) {
        return;
    }
    auto pending = publication(std::move(compilation), frontend.backend.artifact_directory.value_or("build/aot"),
                               frontend.protected_inputs);
    if (frontend.pending_publications) {
        frontend.pending_publications->push_back(std::move(pending));
    } else {
        publish(pending);
    }
}

// Share lowering and specialization, then stop at the selected inspection or artifact boundary.
bool generate(codegen::Compilation &compilation, const Analysis &analysis, const FrontendRequest &frontend) {
    if (!codegen::lower(compilation, analysis.modules, *analysis.inferred)) {
        return false;
    }
    if (frontend.backend.inspect_ir()) {
        return inspect_ir(compilation, frontend);
    }
    return codegen::optimize(compilation) && emit(compilation);
}

// Link the in-memory module and startup objects into the explicit --output executable.
void link(const codegen::Compilation &compilation, const FrontendRequest &frontend, const DiagnosticSink &sink) {
    auto inputs = frontend.protected_inputs;
    for (const auto &input : compilation.request().inputs) {
        inputs.push_back(input.source_path);
    }
    const auto warnings = linking::link_executable({.output = *frontend.executable_output,
                                                    .target_triple = codegen::target_triple(compilation),
                                                    .objects = compilation.result().outputs(),
                                                    .linker = frontend.backend.linker,
                                                    .runtime_library = frontend.backend.runtime_library,
                                                    .protected_inputs = inputs});
    if (!warnings.empty()) {
        sink(warnings);
    }
}

// Select the startup module from the resolved entry, if any.
std::optional<codegen::StartupRequest> startup_request(const Analysis &analysis) {
    if (!analysis.entry) {
        return std::nullopt;
    }
    const auto &entry = *analysis.entry;
    return codegen::StartupRequest{entry.module, utf8(entry.function.name), entry.escript};
}

// Analyze before constructing LLVM state; moving the vector preserves borrowed AST addresses.
bool compile(std::vector<codegen::CompilationInput> inputs, const FrontendRequest &frontend,
             const DiagnosticSink &sink) {
    auto request = backend_request(std::move(inputs), frontend);
    Analysis analysis;
    const EntryRequest entry{frontend.entry, frontend.executable_output.has_value(), !request.project_target.empty()};
    if (!analyze(request, entry, analysis, sink)) {
        return true;
    }
    // Project targets link to their manifest outputs in a later step; positional batches link below.
    if (frontend.executable_output && !request.project_target.empty()) {
        sink("error: " + abi::v1::format_feature_failure(abi::v1::FeatureId::executable_linking,
                                                         {.target = request.project_target, .operation = "link"}));
        return true;
    }
    if (frontend.backend.print_types) {
        print_types(analysis, request);
        return false;
    }
    request.startup = startup_request(analysis);
    codegen::Compilation compilation(std::move(request));
    const bool succeeded = generate(compilation, analysis, frontend);
    report_backend(compilation.result(), sink);
    if (!succeeded || !compilation.result().complete()) {
        return true;
    }
    if (frontend.executable_output) {
        link(compilation, frontend, sink);
        return false;
    }
    deliver(std::move(compilation), frontend);
    return false;
}
} // namespace

bool compile_batch(std::vector<codegen::CompilationInput> inputs, const FrontendRequest &request,
                   const DiagnosticSink &sink) {
    try {
        return compile(std::move(inputs), request, sink);
    } catch (const std::exception &error) {
        sink("error: " + std::string(error.what()));
    }
    return true;
}
} // namespace erlang_aot::cli
