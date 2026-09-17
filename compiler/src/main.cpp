#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

constexpr std::string_view help = R"(Usage: erlangaot [options] <source.erl>...

Ahead-of-time compiler for Erlang (initial CLI scaffold).

Options:
  -h, --help           Show this help and exit.
      --version        Show the tool version and exit.
  -o, --output <path>  Set the future executable output path (default: a.out).
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
};

// Consume the output operand and reject repeated or incomplete output options.
std::optional<std::string> parse_output(std::string_view option,
    std::span<char*>& remaining, Options& options, bool& output_seen)
{
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

// Apply a named option; operands are consumed only by options that require them.
std::optional<std::string> parse_option(std::string_view argument,
    std::span<char*>& remaining, Options& options, bool& output_seen)
{
    if (argument == "-h" || argument == "--help") {
        options.show_help = true;
    } else if (argument == "--version") {
        options.show_version = true;
    } else if (argument == "-o" || argument == "--output") {
        return parse_output(argument, remaining, options, output_seen);
    } else {
        return "unknown option '" + std::string{argument} + "'";
    }
    return std::nullopt;
}

// Walk the arguments in order while respecting the end-of-options marker.
std::optional<std::string> parse_options(std::span<char*> remaining, Options& options)
{
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
    if (!options.show_help && !options.show_version && options.inputs.empty()) {
        return "no input files";
    }
    return std::nullopt;
}

// Validate the request and inputs before reporting the unimplemented backend.
int run(std::span<char*> arguments)
{
    Options options;
    if (const auto error = parse_options(arguments, options)) {
        std::cerr << "erlangaot: error: " << *error
                  << "\nTry 'erlangaot --help' for usage.\n";
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

    for (const auto& input : options.inputs) {
        std::error_code error;
        const bool regular = std::filesystem::is_regular_file(input, error);
        if (error) {
            std::cerr << "erlangaot: error: cannot access " << input
                      << ": " << error.message() << '\n';
            return 1;
        }
        if (!regular) {
            std::cerr << "erlangaot: error: input is not a regular file: " << input << '\n';
            return 1;
        }
        if (!std::ifstream{input, std::ios::binary}) {
            std::cerr << "erlangaot: error: cannot read input: " << input << '\n';
            return 1;
        }
    }

    std::cerr << "erlangaot: error: compilation is not implemented yet; no output was written.\n";
    return 1;
}

} // namespace

// Keep unexpected failures inside the CLI diagnostic and exit-code contract.
int main(int argc, char* argv[])
{
    try {
        return run(std::span{argv, static_cast<std::size_t>(argc)}.subspan(1));
    } catch (const std::exception& error) {
        std::cerr << "erlangaot: error: " << error.what() << '\n';
        return 1;
    }
}
