#include "options.hpp"
#include <map>

namespace erlang_aot::cli {
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
    } else if (argument == "--parse-check") {
        options.preprocess = true;
        options.parse_check = true;
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
        return "--output cannot be used with --preprocess-check, --parse-check, --print-pp, or --print-ast";
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

} // namespace erlang_aot::cli
