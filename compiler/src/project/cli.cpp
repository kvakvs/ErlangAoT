#include "cli.hpp"
#include "paths.hpp"

namespace erlang_aot::project {
bool is_option(std::string_view argument) { return argument == "--project" || argument == "--target"; }

std::optional<std::string> parse_option(std::string_view argument, std::span<char *> &remaining, Request &request) {
    if (argument == "--project" && request.file) {
        return "project path specified more than once";
    }
    if (remaining.empty() || std::string_view(remaining.front()).empty()) {
        return "expected a value after " + std::string(argument);
    }
    const std::string value = remaining.front();
    remaining = remaining.subspan(1);
    if (argument == "--project") {
        request.file = native_path(value);
    } else {
        request.targets.push_back(value);
    }
    return std::nullopt;
}

std::optional<std::string> validate(const Request &request, bool has_inputs) {
    if (request.file && has_inputs) {
        return "--project cannot be combined with positional source inputs";
    }
    if (!request.file && !request.targets.empty()) {
        return "--target requires --project";
    }
    return std::nullopt;
}

bool active(const Request &request) { return request.file.has_value(); }

std::string_view help() {
    return R"(
Project commands:
  erlangaot [options] --project <path> [--target <name>]...
      --project <path>  Read a TOML project instead of positional source inputs.
      --target <name>   Select a target; repeat to select more (default: all).
Manifest paths are relative to the TOML file; CLI paths use the invocation directory.
)";
}
} // namespace erlang_aot::project
