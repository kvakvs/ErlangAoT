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

// Consume an output operand while retaining explicit option presence.
std::optional<std::string> parse_output(std::string_view option, std::span<char *> &remaining, Options &options) {
    if (options.output_explicit) {
        return "output path specified more than once";
    }
    if (remaining.empty() || std::string_view(remaining.front()).empty()) {
        return "expected a path after " + std::string(option);
    }
    options.output = remaining.front();
    remaining = remaining.subspan(1);
    options.output_explicit = true;
    return std::nullopt;
}

namespace {
struct Flag {
    // Map a flag to its destination and whether it requests frontend processing.
    bool Options::*field;
    bool frontend;
};

// Keep generic mode/informational flags separate from operand-consuming options.
bool parse_flag(std::string_view argument, Options &options) {
    static const std::map<std::string_view, Flag> flags{
        {"-h", {&Options::show_help, false}},           {"--help", {&Options::show_help, false}},
        {"--version", {&Options::show_version, false}}, {"--preprocess-check", {&Options::preprocess, true}},
        {"--print-pp", {&Options::print_pp, true}},     {"--parse-check", {&Options::parse_check, true}},
        {"--print-ast", {&Options::print_ast, true}}};
    const auto found = flags.find(argument);
    if (found == flags.end()) {
        return false;
    }
    options.*(found->second.field) = true;
    options.preprocess = options.preprocess || found->second.frontend;
    return true;
}
} // namespace

// Delegate project operands while retaining the original generic CLI spelling rules.
std::optional<std::string> parse_option(std::string_view argument, std::span<char *> &remaining, Options &options) {
    if (parse_flag(argument, options)) {
        return std::nullopt;
    }
    if (argument == "-o" || argument == "--output") {
        return parse_output(argument, remaining, options);
    }
    if (project::is_option(argument)) {
        return project::parse_option(argument, remaining, options.project);
    }
    return pp_option(argument, remaining, options);
}

// Check command combinations before honoring informational requests or reading inputs.
std::optional<std::string> validate_options(const Options &options) {
    if (const auto error = project::validate(options.project, !options.inputs.empty())) {
        return error;
    }
    if (!options.show_help && !options.show_version && options.inputs.empty() && !project::active(options.project)) {
        return "no input files";
    }
    if (options.preprocess && options.output_explicit) {
        return "--output cannot be used with --preprocess-check, --parse-check, --print-pp, or --print-ast";
    }
    return std::nullopt;
}

// Walk arguments in order and retain positional semantics after the end-of-options marker.
std::optional<std::string> parse_options(std::span<char *> remaining, Options &options) {
    bool positional_only = false;
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
        } else if (const auto error = parse_option(argument, remaining, options)) {
            return error;
        }
    }
    return validate_options(options);
}
} // namespace erlang_aot::cli
