#include "frontend.hpp"
#include "../codegen/request.hpp"
#include "../semantic/bindings.hpp"
#include "../semantic/calls.hpp"
#include "../semantic/capabilities.hpp"
#include "../semantic/types/declarations.hpp"
#include "../semantic/types/inference.hpp"
#include "options.hpp"
#include <erlang_aot/compiler/parser.hpp>
#include <erlang_aot/compiler/printing.hpp>
#include <fstream>
#include <iostream>

namespace erlang_aot::cli {
namespace {
// Retain native filename text in encoding, I/O, and ingestion messages.
std::string filename(const std::filesystem::path &path) {
    const auto bytes = path.generic_u8string();
    return {bytes.begin(), bytes.end()};
}

// Keep ingestion messages out of source/AST output and project diagnostic wrappers.
void trace_ingestion(const bool verbose, const std::string_view stage, const std::filesystem::path &path) {
    if (verbose) {
        std::cerr << '[' << stage << "] " << filename(path) << '\n';
    }
}

// Trace resolved includes while preserving any caller-provided observation callback.
PreprocessorOptions preprocessing_options(const FrontendRequest &request) {
    auto options = request.preprocessing;
    if (request.verbose) {
        options.include_loaded = [previous = std::move(options.include_loaded)](const auto &path) {
            trace_ingestion(true, "pp", path);
            if (previous) {
                previous(path);
            }
        };
    }
    return options;
}

// Preserve severity and existing logical/physical source rendering in every caller.
void print_diagnostic(const Diagnostic &diagnostic, const DiagnosticSink &sink) {
    const auto prefix = diagnostic.severity == Severity::warning ? "warning: " : "error: ";
    sink(prefix + erlang_aot::render(diagnostic));
}

// Emit expanded source without adding project-specific stdout banners.
void print_form(const PreprocessorEvent &event) {
    if (const auto *form = std::get_if<OrdinaryForm>(&event)) {
        print_preprocessed(std::cout, *form);
    }
}

// Own syntax for the entire batch before borrowing it in semantic side tables.
using Inputs = std::vector<codegen::CompilationInput>;

// Finish declaration and binding checks before resolving the batch dependency graph.
bool compile_batch(const Inputs &inputs, const DiagnosticSink &sink) {
    bool failed = false;
    const semantic::Reporter report = [&](const Diagnostic &diagnostic) {
        failed = failed || diagnostic.severity == Severity::error;
        print_diagnostic(diagnostic, sink);
    };
    std::vector<std::unique_ptr<semantic::Module>> modules;
    for (const auto &input : inputs) {
        auto module = semantic::index(input.syntax, filename(input.source_path), report);
        semantic::check_capabilities(*module, report);
        semantic::bind_parameters(*module, report);
        modules.push_back(std::move(module));
    }
    if (!failed) {
        const auto calls = semantic::resolve_calls(modules, report);
        if (!failed) {
            const auto types = semantic::types::resolve_declarations(modules, report);
            if (!failed) {
                const auto inferred = semantic::types::infer(calls);
            }
        }
    }
    return failed;
}

// Consume a parsing pass and dispatch successful modules to the requested final stage.
bool parse_and_print(PreprocessorSession &session, const FrontendRequest &request, const DiagnosticSink &sink,
                     const std::filesystem::path &path, Inputs &inputs) {
    ParserSession parser;
    while (!parser.stopped()) {
        const auto event = session.next();
        if (!event) {
            break;
        }
        if (request.print_pp) {
            print_form(*event);
        }
        parser.consume(*event);
    }
    auto result = std::move(parser).finish(session.features());
    for (const auto &diagnostic : result.diagnostics) {
        print_diagnostic(diagnostic, sink);
    }
    if (request.print_ast) {
        print_ast(std::cout, result.module);
    }
    if (result.failed || session.failed()) {
        return true;
    }
    if (request.compile) {
        inputs.emplace_back(path, std::move(result.module));
    }
    return false;
}

// Keep source ownership and all mutable frontend state local to one file.
bool process_module(const std::filesystem::path &path, const FrontendRequest &request, const DiagnosticSink &sink,
                    Inputs &inputs) {
    SourceManager sources;
    const auto source = sources.read(path);
    trace_ingestion(request.verbose, "pp", path);
    PreprocessorSession session(source, preprocessing_options(request));
    if (request.parse_check || request.print_ast || request.compile) {
        trace_ingestion(request.verbose, "parse", path);
        return parse_and_print(session, request, sink, path, inputs);
    }
    while (const auto event = session.next()) {
        if (const auto *diagnostic = std::get_if<Diagnostic>(&*event)) {
            print_diagnostic(*diagnostic, sink);
        }
        if (request.print_pp) {
            print_form(*event);
        }
    }
    return session.failed();
}

// Preserve per-file recovery while retaining successful syntax for the batch.
bool process_file(const std::filesystem::path &path, const FrontendRequest &request, const DiagnosticSink &diagnostics,
                  Inputs &inputs) {
    try {
        return process_module(path, request, diagnostics, inputs);
    } catch (const EncodingError &error) {
        diagnostics(filename(path) + ": byte " + std::to_string(error.byte) + ": " + error.what());
    } catch (const std::exception &error) {
        diagnostics("error: " + filename(path) + ": " + error.what());
    }
    return true;
}

} // namespace

bool process_files(std::span<const std::filesystem::path> paths, const FrontendRequest &request,
                   const DiagnosticSink &sink) {
    Inputs inputs;
    bool failed = false;
    for (const auto &path : paths) {
        failed = process_file(path, request, sink, inputs) || failed;
    }
    if (request.compile) {
        failed = compile_batch(inputs, sink) || failed;
    }
    return failed;
}

// Preserve positional order and warning-only success using the same per-file operation.
int process_inputs(const Options &options) {
    const FrontendRequest request{options.print_pp,    options.print_ast, options.parse_check,
                                  !options.preprocess, options.verbose,   options.preprocessing};
    const DiagnosticSink sink = [](const std::string_view message) { std::cerr << message << '\n'; };
    return process_files(options.inputs, request, sink) ? 1 : 0;
}

// Check physical inputs before entering either preprocessing or future compilation.
bool validate_inputs(const std::vector<std::filesystem::path> &inputs) {
    for (const auto &input : inputs) {
        std::error_code error;
        const bool regular = std::filesystem::is_regular_file(input, error);
        if (error) {
            std::cerr << "erlangaot: error: cannot access " << input << ": " << error.message() << '\n';
            return false;
        }
        if (!regular) {
            std::cerr << "erlangaot: error: input is not a regular file: " << input << '\n';
            return false;
        }
        if (!std::ifstream{input, std::ios::binary}) {
            std::cerr << "erlangaot: error: cannot read input: " << input << '\n';
            return false;
        }
    }
    return true;
}

} // namespace erlang_aot::cli
