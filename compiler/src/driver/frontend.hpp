#pragma once
#include <erlang_aot/compiler/preprocessor.hpp>
#include <functional>

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
};

// Allow callers to attach project context without changing frontend source rendering.
using DiagnosticSink = std::function<void(std::string_view)>;
// Process one source in isolated sessions, reporting failures through the caller's sink.
bool process_file(const std::filesystem::path &path, const FrontendRequest &request, const DiagnosticSink &diagnostics);
} // namespace erlang_aot::cli
