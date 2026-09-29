#include "backend.hpp"
#include "../artifacts/artifacts.hpp"
#include "../codegen/emission.hpp"
#include "../codegen/lowering.hpp"
#include "../codegen/optimization.hpp"
#include "../codegen/serialization.hpp"
#include "../codegen/target.hpp"
#include "analysis.hpp"

namespace erlang_aot::cli {
namespace {
// Carry invocation policy into the private backend without exposing LLVM types to the driver.
codegen::CompilationRequest backend_request(std::vector<codegen::CompilationInput> inputs,
                                            const FrontendRequest &frontend) {
    codegen::CompilationRequest request;
    request.inputs = std::move(inputs);
    request.implementation_debug = frontend.implementation_debug;
    if (frontend.backend) {
        const auto &options = *frontend.backend;
        request.target_triple = options.target_triple;
        request.optimization = options.optimization.value_or(codegen::OptimizationLevel::none);
        request.disable_type_specialization = options.disable_type_specialization;
        request.output_kind = options.emit.value_or(codegen::OutputKind::object);
    }
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

// Protect original source files during publication and retain the target-selected object suffix.
void publish(const codegen::Compilation &compilation, const BackendOptions &options) {
    if (!options.emit) {
        return;
    }
    std::vector<std::filesystem::path> inputs;
    for (const auto &input : compilation.request().inputs) {
        inputs.push_back(input.source_path);
    }
    artifacts::publish(compilation.result().outputs(), options.artifact_directory.value_or("build/aot"), inputs,
                       codegen::object_extension(compilation));
}

// Analyze before constructing LLVM state; moving the vector preserves borrowed AST addresses.
bool compile(std::vector<codegen::CompilationInput> inputs, const FrontendRequest &frontend,
             const DiagnosticSink &sink) {
    auto request = backend_request(std::move(inputs), frontend);
    Analysis analysis;
    if (!analyze(request.inputs, analysis, request.implementation_debug, sink)) {
        return true;
    }
    if (!frontend.backend) {
        return false;
    }
    codegen::Compilation compilation(std::move(request));
    const bool succeeded = codegen::lower(compilation, analysis.modules, *analysis.inferred) &&
                           codegen::optimize(compilation) && emit(compilation);
    report_backend(compilation.result(), sink);
    if (!succeeded || !compilation.result().complete()) {
        return true;
    }
    publish(compilation, *frontend.backend);
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
