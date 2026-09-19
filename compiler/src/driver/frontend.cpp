#include "frontend.hpp"
#include "options.hpp"
#include <erlang_aot/compiler/parser.hpp>
#include <erlang_aot/compiler/printing.hpp>
#include <fstream>
#include <iostream>

namespace erlang_aot::cli {
namespace {
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

// Consume a parsing pass while preserving recoverable AST output and failure latching.
bool parse_and_print(PreprocessorSession &session, const FrontendRequest &request, const DiagnosticSink &sink) {
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
    return result.failed || session.failed();
}

// Keep source ownership and all mutable frontend state local to one file.
bool process_module(const std::filesystem::path &path, const FrontendRequest &request, const DiagnosticSink &sink) {
    SourceManager sources;
    PreprocessorSession session(sources.read(path), request.preprocessing);
    if (request.parse_check || request.print_ast) {
        return parse_and_print(session, request, sink);
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

// Retain native filename text in encoding and I/O diagnostics.
std::string filename(const std::filesystem::path &path) {
    const auto bytes = path.generic_u8string();
    return {bytes.begin(), bytes.end()};
}
} // namespace

bool process_file(const std::filesystem::path &path, const FrontendRequest &request,
                  const DiagnosticSink &diagnostics) {
    try {
        return process_module(path, request, diagnostics);
    } catch (const EncodingError &error) {
        diagnostics(filename(path) + ": byte " + std::to_string(error.byte) + ": " + error.what());
    } catch (const std::exception &error) {
        diagnostics("error: " + filename(path) + ": " + error.what());
    }
    return true;
}

// Preserve positional order and warning-only success using the same per-file operation.
int preprocess(const Options &options) {
    const FrontendRequest request{options.print_pp, options.print_ast, options.parse_check, options.preprocessing};
    const DiagnosticSink sink = [](std::string_view message) { std::cerr << message << '\n'; };
    bool failed = false;
    for (const auto &path : options.inputs) {
        failed = process_file(path, request, sink) || failed;
    }
    return failed ? 1 : 0;
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
