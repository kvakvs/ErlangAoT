#include "cli.hpp"
#include "create.hpp"
#include "paths.hpp"

namespace erlang_aot::project {
namespace {
// Keep standalone creation conflict policy out of generic driver validation.
bool creation_conflict(const Request &request, const Usage &usage) {
    return request.file || !request.targets.empty() || usage.positional_inputs || usage.output || usage.frontend ||
           usage.frontend_options;
}

// Check repetition before consuming a second project filename operand.
bool repeated(std::string_view argument, const Request &request) {
    return (argument == "--project" && request.file) || (argument == "--new-project" && request.create);
}
} // namespace

bool is_option(std::string_view argument) {
    return argument == "--project" || argument == "--target" || argument == "--new-project";
}

std::optional<std::string> parse_option(std::string_view argument, std::span<char *> &remaining, Request &request) {
    if (repeated(argument, request)) {
        return std::string(argument) + " specified more than once";
    }
    if (remaining.empty() || std::string_view(remaining.front()).empty()) {
        return "expected a value after " + std::string(argument);
    }
    const std::string value = remaining.front();
    remaining = remaining.subspan(1);
    if (argument == "--target") {
        request.targets.push_back(value);
    } else if (argument == "--project") {
        request.file = native_path(value);
    } else {
        request.create = native_path(value);
    }
    return std::nullopt;
}

std::optional<std::string> validate(const Request &request, const Usage &usage) {
    if (request.create && creation_conflict(request, usage)) {
        return "--new-project cannot be combined with source inputs, --project, --target, --output, or frontend "
               "options";
    }
    if (request.create && !valid_creation_filename(*request.create)) {
        return "--new-project requires a filename";
    }
    if (request.file && usage.positional_inputs) {
        return "--project cannot be combined with positional source inputs";
    }
    if (!request.file && !request.targets.empty()) {
        return "--target requires --project";
    }
    return std::nullopt;
}

bool active(const Request &request) { return request.file.has_value() || request.create.has_value(); }

std::string_view help() {
    return R"(
Project commands:
  erlangaot [options] --project <path> [--target <name>]...
  erlangaot --new-project <filename>
      --project <path>  Read a TOML project instead of positional source inputs.
                        Try appending .toml if the path is missing that suffix and absent.
      --target <name>   Select a target; repeat to select more (default: all).
      --new-project <filename>  Create one annotated default target; append .toml
                                when needed and refuse to overwrite existing files.
Manifest paths are relative to the TOML file; CLI paths use the invocation directory.
)";
}
} // namespace erlang_aot::project
