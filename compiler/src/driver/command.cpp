#include "../project/command.hpp"
#include "frontend.hpp"
#include "options.hpp"
#include <iostream>

namespace erlang_aot::cli {

constexpr std::string_view help = R"(Usage: erlangaot [options] <source.erl>...

Ahead-of-time compiler for Erlang/OTP 29.

Options:
  -h, --help           Show this help and exit.
      --version        Show the tool version and exit.
      --verbose        Trace inputs and compilation phases to stderr with [pp]/[parse]/[comp].
      --impldebug <n[,n...]>  Enable selected implementation-step debug output; repeatable.
  -o, --output <path>  Set the future executable output path (default: a.out).
      --emit <obj|llvm-ir|llvm-bc>  Select one artifact per module (default: in memory).
      --artifact-dir <dir>  Override artifact root; requires --emit.
      --target-triple <triple>  Select machine/OS/ABI, independently of --target.
      -O0 | -O2          Select generic O0 (default) or speed specialization and LLVM O2.
      --no-type-specialization  Disable variants regardless of optimization option order.
      --print-ir          Print verified IR before LLVM optimization; no files or object emission.
      --print-types       Report declared/inferred types; stop before LLVM lowering.
      --print-optimized-ir  Print verified IR after the selected pipeline; combine for both stages.
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
With no check/print action, source batches compile to verified objects in memory.
Only --emit writes module artifacts (default root: build/aot); native executable linking is deferred.
Compilation switches conflict with frontend check/print actions and --new-project.
--emit conflicts with explicit --output; --output remains reserved for executables.
IR inspection accepts target/optimization/preprocessing options, but rejects emission/output options.
Type inspection accepts preprocessing/project/verbosity options; it rejects other actions and backend policy.
Type reports describe conservative analysis and are not an intermediate stage input format.
Multiple IR snapshots use LLVM-comment headers; use --emit llvm-ir for separate machine-readable files.
Value options and optimization levels may appear only once.
Input paths may contain spaces when quoted by the shell.
)";

// Validate the request and dispatch explicit actions or the default compiler pipeline.
int run_command(std::span<char *> arguments) {
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

    return erlang_aot::cli::process_inputs(options);
}

} // namespace erlang_aot::cli
