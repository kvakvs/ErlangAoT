#include <erlang_aot/compiler/parser.hpp>
#include <erlang_aot/compiler/preprocessor.hpp>
#include <erlang_aot/compiler/printing.hpp>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

constexpr std::string_view help = R"(Usage: erlangaot [options] <source.erl>...

Ahead-of-time compiler for Erlang/OTP 29.

Options:
  -h, --help           Show this help and exit.
      --version        Show the tool version and exit.
  -o, --output <path>  Set the future executable output path (default: a.out).
      --preprocess-check  Preprocess each module and report diagnostics only.
      --print-pp         Print preprocessed Erlang source to stdout.
      --print-ast        Parse and print an indented syntax tree to stdout.
  -I, --include <dir>  Add an include directory (last supplied is searched first).
  -D, --define <name[=term]>  Predefine a macro (default value: true).
      --app-dir <app=dir>  Map an include_lib application to a directory.
      --enable-feature <name>  Enable an OTP 29.1 feature.
      --disable-feature <name>  Disable an OTP 29.1 feature.
      --               Treat all remaining arguments as input paths.

Compilation is not implemented yet. Compilation requests fail without writing
an output file. Input paths may contain spaces when quoted by the shell.
)";

struct Options {
    // Select informational output after validating all command-line arguments.
    bool show_help = false;
    bool show_version = false;
    // Retain the requested destination for the future code-generation stage.
    std::filesystem::path output = "a.out";
    // Preserve source order for input validation and future compilation.
    std::vector<std::filesystem::path> inputs;
    // Preprocessing options are recreated independently for every input module.
    bool preprocess = false;
    // Emit expanded forms while sharing the diagnostics-only preprocessing path.
    bool print_pp = false;
    // Parse the expanded token stream and emit its typed syntax tree.
    bool print_ast = false;
    erlang_aot::PreprocessorOptions preprocessing;
};

// Apply validated preprocessing operands to the per-module configuration.
std::optional<std::string> apply_pp_option(int kind, const std::string &value,
                                           erlang_aot::PreprocessorOptions &settings) {
    switch (kind) {
    case 0:
        settings.include_paths.insert(settings.include_paths.begin(), value);
        break;
    case 1:
        settings.definitions.push_back(value);
        break;
    case 2: {
        const auto equals = value.find('=');
        if (equals == std::string::npos || equals == 0 || equals + 1 == value.size()) {
            return "expected app=directory";
        }
        settings.applications.insert_or_assign(value.substr(0, equals), value.substr(equals + 1));
        break;
    }
    default:
        settings.features.emplace_back(value, kind == 3);
        break;
    }
    return std::nullopt;
}

// Consume one option operand, including joined -Ipath/-Dname spellings.
std::optional<std::string> pp_option(std::string_view argument, std::span<char *> &remaining, Options &options) {
    std::string value;
    if (argument.size() > 2 && (argument.starts_with("-I") || argument.starts_with("-D"))) {
        value = argument.substr(2);
        argument = argument.substr(0, 2);
    }
    static const std::map<std::string_view, int> names{{"-I", 0},
                                                       {"--include", 0},
                                                       {"-D", 1},
                                                       {"--define", 1},
                                                       {"--app-dir", 2},
                                                       {"--enable-feature", 3},
                                                       {"--disable-feature", 4}};
    const auto found = names.find(argument);
    if (found == names.end()) {
        return "unknown option '" + std::string(argument) + "'";
    }
    if (value.empty()) {
        if (remaining.empty() || std::string_view(remaining.front()).empty()) {
            return "expected a value after " + std::string(argument);
        }
        value = remaining.front();
        remaining = remaining.subspan(1);
    }
    return apply_pp_option(found->second, value, options.preprocessing);
}

// Consume the output operand and reject repeated or incomplete output options.
std::optional<std::string> parse_output(std::string_view option, std::span<char *> &remaining, Options &options,
                                        bool &output_seen) {
    if (output_seen) {
        return "output path specified more than once";
    }
    if (remaining.empty() || std::string_view{remaining.front()}.empty()) {
        return "expected a path after " + std::string{option};
    }
    options.output = remaining.front();
    remaining = remaining.subspan(1);
    output_seen = true;
    return std::nullopt;
}

// Apply a named option; operands are consumed only by options that require
// them.
std::optional<std::string> parse_option(std::string_view argument, std::span<char *> &remaining, Options &options,
                                        bool &output_seen) {
    if (argument == "-h" || argument == "--help") {
        options.show_help = true;
    } else if (argument == "--version") {
        options.show_version = true;
    } else if (argument == "-o" || argument == "--output") {
        return parse_output(argument, remaining, options, output_seen);
    } else if (argument == "--preprocess-check") {
        options.preprocess = true;
    } else if (argument == "--print-pp") {
        options.preprocess = true;
        options.print_pp = true;
    } else if (argument == "--print-ast") {
        options.preprocess = true;
        options.print_ast = true;
    } else {
        return pp_option(argument, remaining, options);
    }
    return std::nullopt;
}

// Check option combinations after operands and paths have been consumed.
std::optional<std::string> validate_options(const Options &options, bool output_seen) {
    if (!options.show_help && !options.show_version && options.inputs.empty()) {
        return "no input files";
    }
    if (options.preprocess && output_seen) {
        return "--output cannot be used with --preprocess-check, --print-pp, or --print-ast";
    }
    return std::nullopt;
}

// Walk the arguments in order while respecting the end-of-options marker.
std::optional<std::string> parse_options(std::span<char *> remaining, Options &options) {
    bool positional_only = false;
    bool output_seen = false;
    while (!remaining.empty()) {
        const std::string_view argument{remaining.front()};
        remaining = remaining.subspan(1);
        if (argument.empty()) {
            return "input path must not be empty";
        }
        if (positional_only || !argument.starts_with('-')) {
            options.inputs.emplace_back(argument);
        } else if (argument == "--") {
            positional_only = true;
        } else if (const auto error = parse_option(argument, remaining, options, output_seen)) {
            return error;
        }
    }
    return validate_options(options, output_seen);
}

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
bool parse_and_print(erlang_aot::PreprocessorSession &session, bool print_pp) {
    erlang_aot::ParserSession parser;
    while (!parser.stopped()) {
        const auto event = session.next();
        if (!event) {
            break;
        }
        if (print_pp) {
            print_form(*event);
        }
        parser.consume(*event);
    }
    auto result = std::move(parser).finish(session.features());
    for (const auto &diagnostic : result.diagnostics) {
        print_diagnostic(diagnostic);
    }
    erlang_aot::print_ast(std::cout, result.module);
    return result.failed || session.failed();
}

// Drain one module's events; diagnostics retain logical and physical provenance.
bool preprocess_module(const std::filesystem::path &path, const Options &options) {
    erlang_aot::SourceManager sources;
    erlang_aot::PreprocessorSession session(sources.read(path), options.preprocessing);
    if (options.print_ast) {
        return parse_and_print(session, options.print_pp);
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

// Validate the request and inputs before reporting the unimplemented backend.
int run(std::span<char *> arguments) {
    Options options;
    if (const auto error = parse_options(arguments, options)) {
        std::cerr << "erlangaot: error: " << *error << "\nTry 'erlangaot --help' for usage.\n";
        return 2;
    }
    if (options.show_help) {
        std::cout << help;
        return 0;
    }
    if (options.show_version) {
        std::cout << "erlangaot " << ERLANG_AOT_VERSION << '\n';
        return 0;
    }

    if (!validate_inputs(options.inputs)) {
        return 1;
    }

    if (options.preprocess) {
        return preprocess(options);
    }
    std::cerr << "erlangaot: error: compilation is not implemented yet; no "
                 "output was written.\n";
    return 1;
}

} // namespace

// Keep unexpected failures inside the CLI diagnostic and exit-code contract.
int main(int argc, char *argv[]) {
    try {
        return run(std::span{argv, static_cast<std::size_t>(argc)}.subspan(1));
    } catch (const std::exception &error) {
        std::cerr << "erlangaot: error: " << error.what() << '\n';
        return 1;
    }
}
