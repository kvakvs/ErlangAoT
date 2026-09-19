#pragma once
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace erlang_aot::project {
struct Request {
    // Preserve explicit project selection separately from positional driver inputs.
    std::optional<std::filesystem::path> file;
    // Select standalone annotated-manifest creation instead of compilation inputs.
    std::optional<std::filesystem::path> create;
    std::vector<std::string> targets;
};

struct Usage {
    // Retain explicit generic option presence for standalone-command conflict checks.
    bool positional_inputs = false;
    bool output = false;
    bool frontend = false;
    bool frontend_options = false;
};

// Identify only the project-owned option spellings supported by this version.
bool is_option(std::string_view argument);
// Consume a project option and its operand without accessing the filesystem.
std::optional<std::string> parse_option(std::string_view argument, std::span<char *> &remaining, Request &request);
// Validate project/positional selection before informational command handling.
std::optional<std::string> validate(const Request &request, const Usage &usage);
// Tell the driver whether a project command replaces positional source handling.
bool active(const Request &request);
// Keep project-specific help alongside the implementation that owns its behavior.
std::string_view help();
} // namespace erlang_aot::project
