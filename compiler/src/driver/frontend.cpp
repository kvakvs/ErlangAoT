#include "options.hpp"
#include <erlang_aot/compiler/parser.hpp>
#include <erlang_aot/compiler/printing.hpp>
#include <fstream>
#include <iostream>

namespace erlang_aot::cli {
// Keep all frontend diagnostics on stderr with consistent severity and source context.
void print_diagnostic(const erlang_aot::Diagnostic &diagnostic) {
    std::cerr << (diagnostic.severity == erlang_aot::Severity::warning ? "warning: " : "error: ")
              << erlang_aot::render(diagnostic) << '\n';
}

// Emit only expanded source forms from a preprocessing event.
void print_form(const erlang_aot::PreprocessorEvent &event) {
    if (const auto *form = std::get_if<erlang_aot::OrdinaryForm>(&event)) {
        erlang_aot::print_preprocessed(std::cout, *form);
    }
}

// Consume one preprocessing pass, optionally printing source before the recovered AST.
bool parse_and_print(erlang_aot::PreprocessorSession &session, const Options &options) {
    erlang_aot::ParserSession parser;
    while (!parser.stopped()) {
        const auto event = session.next();
        if (!event) {
            break;
        }
        if (options.print_pp) {
            print_form(*event);
        }
        parser.consume(*event);
    }
    auto result = std::move(parser).finish(session.features());
    for (const auto &diagnostic : result.diagnostics) {
        print_diagnostic(diagnostic);
    }
    if (options.print_ast)
        erlang_aot::print_ast(std::cout, result.module);
    return result.failed || session.failed();
}

// Drain one module's events; diagnostics retain logical and physical provenance.
bool preprocess_module(const std::filesystem::path &path, const Options &options) {
    erlang_aot::SourceManager sources;
    erlang_aot::PreprocessorSession session(sources.read(path), options.preprocessing);
    if (options.parse_check || options.print_ast) {
        return parse_and_print(session, options);
    }
    while (const auto event = session.next()) {
        if (const auto *diagnostic = std::get_if<erlang_aot::Diagnostic>(&*event)) {
            print_diagnostic(*diagnostic);
        }
        if (options.print_pp) {
            print_form(*event);
        }
    }
    return session.failed();
}

// Process all modules while preserving warning-only success and per-module isolation.
int preprocess(const Options &options) {
    bool failed = false;
    for (const auto &path : options.inputs) {
        try {
            failed = preprocess_module(path, options) || failed;
        } catch (const erlang_aot::EncodingError &error) {
            std::cerr << path << ": byte " << error.byte << ": " << error.what() << '\n';
            failed = true;
        }
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
