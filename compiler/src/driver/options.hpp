#pragma once
#include "project/cli.hpp"
#include <erlang_aot/compiler/preprocessor.hpp>
#include <span>

namespace erlang_aot::cli {
struct Options {
    // Select informational output after validating all command-line arguments.
    bool show_help = false;
    bool show_version = false;
    // Retain the requested destination for the future code-generation stage.
    std::filesystem::path output = "a.out";
    // Distinguish explicit output overrides from the positional-mode default.
    bool output_explicit = false;
    // Delegate project selection data and policy to the project component.
    erlang_aot::project::Request project;
    // Preserve source order for input validation and future compilation.
    std::vector<std::filesystem::path> inputs;
    // Preprocessing options are recreated independently for every input module.
    bool preprocess = false;
    // Emit expanded forms while sharing the diagnostics-only preprocessing path.
    bool print_pp = false;
    // Parse the expanded token stream and emit its typed syntax tree.
    bool print_ast = false;
    // Request syntax diagnostics without requiring tree output.
    bool parse_check = false;
    erlang_aot::PreprocessorOptions preprocessing;
};

// Parse and validate usage independently of reading source files.
std::optional<std::string> parse_options(std::span<char *> remaining, Options &options);
// Process every input using an independent frontend ownership context.
int preprocess(const Options &options);
// Check physical input paths before running the selected frontend mode.
bool validate_inputs(const std::vector<std::filesystem::path> &inputs);
} // namespace erlang_aot::cli
