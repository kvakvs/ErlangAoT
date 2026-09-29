#pragma once
#include "../implementation_debug.hpp"
#include "backend_options.hpp"
#include <erlang_aot/compiler/diagnostic.hpp>
#include <erlang_aot/compiler/preprocessor.hpp>
#include <functional>
#include <span>

namespace erlang_aot::cli {
struct FrontendRequest {
    // Select existing frontend output/check behavior independently of input selection.
    bool print_pp = false;
    bool print_ast = false;
    bool parse_check = false;
    // Continue successful parsing into the compilation placeholder for default requests.
    bool compile = false;
    // Report physical input filenames to stderr as each frontend stage ingests them.
    bool verbose = false;
    // Supply a fresh preprocessing configuration to each module session.
    PreprocessorOptions preprocessing;
    // Preserve the same debug selection for every module in a positional or project batch.
    ImplementationDebug implementation_debug;
    // Enable the shared backend for positional batches; project routing follows separately.
    std::optional<BackendOptions> backend = {};
};

// Process one isolated batch; project targets never share declaration tables.
bool process_files(std::span<const std::filesystem::path> paths, const FrontendRequest &request,
                   const DiagnosticSink &diagnostics);
} // namespace erlang_aot::cli
