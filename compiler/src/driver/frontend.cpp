#include "frontend.hpp"
#include "../codegen/limits.hpp"
#include "../codegen/request.hpp"
#include "../linking/link.hpp"
#include "../project/paths.hpp"
#include "../semantic/calls.hpp"
#include "backend.hpp"
#include "escript.hpp"
#include "options.hpp"
#include "predefined.hpp"
#include "transforms/export.hpp"
#include <algorithm>
#include <clause/compiler/parser.hpp>
#include <clause/compiler/printing.hpp>
#include <clause/compiler/source.hpp>
#include <fstream>
#include <iostream>

namespace clause::cli {
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
    sink(prefix + clause::render(diagnostic));
}

// Emit expanded source without adding project-specific stdout banners.
void print_form(const PreprocessorEvent &event) {
    if (const auto *form = std::get_if<OrdinaryForm>(&event)) {
        print_preprocessed(std::cout, *form);
    }
}

// Own syntax for the entire batch before borrowing it in semantic side tables.
using Inputs = std::vector<codegen::CompilationInput>;

// Print a parsed module as the request asks: as a syntax tree, as source, as abstract forms of the main source.
void print_parsed(const FrontendRequest &request, const ast::Module &module, const Source &main) {
    if (request.print_ast) {
        print_ast(std::cout, module);
    }
    if (request.print_source) {
        print_source(std::cout, module);
    }
    if (request.print_abstr) {
        // Added for parse transforms.
        std::cout << transforms::abstract_text(transforms::export_module(module, module.forms(), main));
    }
}

// Feed every preprocessed event to the parser until EOF or a parser resource limit.
void parse_events(PreprocessorSession &session, const FrontendRequest &request, ParserSession &parser) {
    while (!parser.stopped()) {
        const auto event = session.next();
        if (!event) {
            return;
        }
        if (request.print_pp) {
            print_form(*event);
        }
        parser.consume(*event);
    }
}

// Consume a parsing pass and dispatch successful modules to the requested final stage; compiled modules get OTP's
// predefined functions.
bool parse_and_print(PreprocessorSession &session, const FrontendRequest &request, const DiagnosticSink &sink,
                     const SourcePtr &source, const std::filesystem::path &path, const bool escript, Inputs &inputs) {
    ParserSession parser;
    parse_events(session, request, parser);
    const auto predefined = request.compile && !parser.stopped()
                                ? add_predefined(parser, session.features(), path, escript)
                                : std::size_t{0};
    auto result = std::move(parser).finish(session.features());
    for (const auto &diagnostic : result.diagnostics) {
        print_diagnostic(diagnostic, sink);
    }
    print_parsed(request, result.module, *source);
    if (result.failed || session.failed()) {
        return true;
    }
    if (request.compile) {
        codegen::validate_input_limits(inputs, {}, &result.module);
        inputs.emplace_back(path, std::move(result.module));
        inputs.back().escript = escript;
        inputs.back().predefined_ = predefined;
    }
    return false;
}

// Read a source, rewriting a "#!" escript header and warning about ignored emulator arguments.
SourcePtr read_source(SourceManager &sources, const std::filesystem::path &path, const DiagnosticSink &sink,
                      bool &escript) {
    auto source = sources.read(path);
    const auto rewritten = escript_source(path, source->bytes);
    escript = rewritten.has_value();
    if (!rewritten) {
        return source;
    }
    if (rewritten->emulator_arguments) {
        sink("warning: " + filename(path) + ":" + std::to_string(*rewritten->emulator_arguments) +
             ":1: escript emulator arguments (%%!) are ignored by compiled executables");
    }
    return sources.add(source->name, rewritten->bytes);
}

// Keep source ownership and all mutable frontend state local to one file.
bool process_module(const std::filesystem::path &path, const FrontendRequest &request, const DiagnosticSink &sink,
                    Inputs &inputs) {
    SourceManager sources;
    bool escript = false;
    const auto source = read_source(sources, path, sink, escript);
    trace_ingestion(request.verbose, "pp", path);
    PreprocessorSession session(source, preprocessing_options(request));
    if (request.parse_check || request.print_ast || request.print_source || request.print_abstr || request.compile) {
        trace_ingestion(request.verbose, "parse", path);
        return parse_and_print(session, request, sink, source, path, escript, inputs);
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

// Whether a module of the batch declares `name`.
bool defined(const Inputs &inputs, const std::u32string &name) {
    return std::ranges::any_of(inputs,
                               [&](const auto &input) { return semantic::declared_module(input.syntax) == name; });
}

// The first Module.erl in the directories, in order; empty when none exists.
std::filesystem::path module_source(const std::vector<std::filesystem::path> &directories, const std::u32string &name) {
    const auto file = project::native_path(utf8(name) + ".erl");
    for (const auto &directory : directories) {
        std::error_code error;
        if (std::filesystem::is_regular_file(directory / file, error)) {
            return directory / file;
        }
    }
    return {};
}

// Added for --print-inputs.
struct ReferencedInput {
    // A module source added because a batch module names it, and "file:line" of the first naming site.
    std::filesystem::path path;
    std::string site;
};

// Module discovery state shared by add_referenced and the listing. Added for --print-inputs.
struct Discovery {
    // Search directories in order (source_search_paths, then the library), and the module names already handled.
    std::vector<std::filesystem::path> directories;
    std::set<std::u32string> added;
    // Sources added by reference, in the order they joined the batch.
    std::vector<ReferencedInput> referenced;
};

// Render where a module was named as "file:line" of the token's logical location. Added for --print-inputs.
std::string site_text(const ast::Module &syntax, const ast::NodeSource &source) {
    const auto &location = syntax.anchor(source).location;
    return project::path_text(project::native_path(location.file)) + ":" + std::to_string(location.line);
}

// Load the module `name` when the batch lacks it and a search directory has it; true on a source failure.
// Added for --print-inputs.
bool add_module(const std::u32string &name, std::string site, Discovery &discovery, const FrontendRequest &request,
                const DiagnosticSink &sink, Inputs &inputs) {
    if (defined(inputs, name) || !discovery.added.insert(name).second) {
        return false;
    }
    auto path = module_source(discovery.directories, name);
    if (path.empty()) {
        return false;
    }
    discovery.referenced.push_back({path, std::move(site)});
    return process_file(path, request, sink, inputs);
}

// Add the modules the batch references but does not define, from the search directories, then the library,
// including those they reference in turn.
bool add_referenced(const FrontendRequest &request, const DiagnosticSink &sink, Inputs &inputs, Discovery &discovery) {
    discovery.directories = request.module_search_paths_;
    discovery.directories.push_back(linking::library_directory());
    bool failed = false;
    for (std::size_t scanned = 0; scanned < inputs.size(); ++scanned) {
        for (const auto &[name, source] : semantic::referenced_modules(inputs[scanned].syntax)) {
            auto site = site_text(inputs[scanned].syntax, source);
            failed = add_module(name, std::move(site), discovery, request, sink, inputs) || failed;
        }
    }
    return failed;
}

// List paths one per line as given. Added for --print-inputs.
void print_inputs(const std::span<const std::filesystem::path> paths) {
    for (const auto &path : paths) {
        std::cout << filename(path) << '\n';
    }
}

// Print the batch's inputs: listed sources, then each source added by reference with the site that named it.
// Added for --print-inputs.
void list_batch(const std::span<const std::filesystem::path> paths, const Discovery &discovery,
                const FrontendRequest &request) {
    if (request.multiple_targets) {
        std::cout << "[target " << request.project_target << "]\n";
    }
    print_inputs(paths);
    for (const auto &input : discovery.referenced) {
        std::cout << filename(input.path) << " (referenced at " << input.site << ")\n";
    }
}
} // namespace

bool process_files(const std::span<const std::filesystem::path> paths, const FrontendRequest &request,
                   const DiagnosticSink &sink) {
    Inputs inputs;
    if (request.compile && paths.size() > codegen::CompilationLimits{}.modules) {
        sink("error: compilation module count limit exceeded");
        return true;
    }
    bool failed = false;
    for (const auto &path : paths) {
        failed = process_file(path, request, sink, inputs) || failed;
    }
    Discovery discovery;
    if (request.compile && !failed) {
        failed = add_referenced(request, sink, inputs, discovery);
    }
    // Stop before compilation and list the batch. Added for --print-inputs.
    if (request.list_inputs) {
        list_batch(paths, discovery, request);
        return failed;
    }
    if (request.compile && !failed) {
        failed = compile_batch(std::move(inputs), request, sink);
    }
    return failed;
}

// Preserve positional order and warning-only success using the same per-file operation.
int process_inputs(const Options &options) {
    FrontendRequest request{
        options.print_pp, options.print_ast,     options.parse_check, !options.preprocess || options.print_inputs,
        options.verbose,  options.preprocessing, options.backend};
    request.list_inputs = options.print_inputs; // Added for --print-inputs.
    request.print_source = options.print_source;
    request.print_abstr = options.print_abstr; // Added for parse transforms.
    if (options.output_explicit) {
        request.executable_output = options.output;
    }
    if (options.entry) {
        request.entry = project::SelectedEntry{*options.entry, "--entry"};
    }
    const DiagnosticSink sink = [](const std::string_view message) { std::cerr << message << '\n'; };
    return process_files(options.inputs, request, sink) ? 1 : 0;
}

// Check physical inputs before entering either preprocessing or future compilation.
bool validate_inputs(const std::vector<std::filesystem::path> &inputs) {
    for (const auto &input : inputs) {
        std::error_code error;
        const bool regular = std::filesystem::is_regular_file(input, error);
        if (error) {
            std::cerr << "clau: error: cannot access " << input << ": " << error.message() << '\n';
            return false;
        }
        if (!regular) {
            std::cerr << "clau: error: input is not a regular file: " << input << '\n';
            return false;
        }
        if (!std::ifstream{input, std::ios::binary}) {
            std::cerr << "clau: error: cannot read input: " << input << '\n';
            return false;
        }
    }
    return true;
}

} // namespace clause::cli
