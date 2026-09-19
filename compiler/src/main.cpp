#include "driver/frontend.hpp"
#include "driver/options.hpp"
#include "project/command.hpp"
#include <exception>
#include <iostream>

namespace {

constexpr std::string_view help = R"(Usage: erlangaot [options] <source.erl>...

Ahead-of-time compiler for Erlang/OTP 29.

Options:
  -h, --help           Show this help and exit.
      --version        Show the tool version and exit.
  -o, --output <path>  Set the future executable output path (default: a.out).
      --preprocess-check  Preprocess each module and report diagnostics only.
      --parse-check      Preprocess and parse; report syntax diagnostics only.
      --print-pp         Print preprocessed Erlang source to stdout.
      --print-ast        Parse and print an indented syntax tree to stdout.
  -I, --include <dir>  Add an include directory (last supplied is searched first).
  -D, --define <name[=term]>  Predefine a macro (default value: true).
      --app-dir <app=dir>  Map an include_lib application to a directory.
      --enable-feature <name>  Enable an OTP 29.1 feature.
      --disable-feature <name>  Disable an OTP 29.1 feature.
      --               Treat all remaining arguments as input paths.

Checks do not validate semantics or run parse transforms.
Compilation is not implemented yet. Compilation requests fail without writing
an output file. Input paths may contain spaces when quoted by the shell.
)";

// Map generic invocation settings onto the project command's existing frontend callback.
int run_project(const erlang_aot::cli::Options &options) {
    erlang_aot::project::PlanOptions settings;
    settings.working_directory = std::filesystem::current_path();
    settings.preprocessing = options.preprocessing;
    settings.frontend = options.preprocess;
    if (options.output_explicit) {
        settings.output = options.output;
    }
    const erlang_aot::project::FileExecutor execute = [&](const auto &path, const auto &preprocessing,
                                                          const auto &sink) {
        return erlang_aot::cli::process_file(
            path, {options.print_pp, options.print_ast, options.parse_check, preprocessing}, sink);
    };
    return erlang_aot::project::run(options.project, settings, execute, std::cout, std::cerr);
}

// Validate the request and inputs before reporting the unimplemented backend.
int run(std::span<char *> arguments) {
    erlang_aot::cli::Options options;
    if (const auto error = erlang_aot::cli::parse_options(arguments, options)) {
        std::cerr << "erlangaot: error: " << *error << "\nTry 'erlangaot --help' for usage.\n";
        return 2;
    }
    if (options.show_help) {
        std::cout << help << erlang_aot::project::help();
        return 0;
    }
    if (options.show_version) {
        std::cout << "erlangaot " << ERLANG_AOT_VERSION << '\n';
        return 0;
    }

    if (erlang_aot::project::active(options.project)) {
        return run_project(options);
    }

    if (!erlang_aot::cli::validate_inputs(options.inputs)) {
        return 1;
    }

    if (options.preprocess) {
        return erlang_aot::cli::preprocess(options);
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
