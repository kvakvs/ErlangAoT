#include "diagnostics.hpp"

namespace erlang_aot::project {
std::string render(const Error &error) {
    const auto bytes = error.site.file.generic_u8string();
    std::string result(bytes.begin(), bytes.end());
    if (error.site.line != 0) {
        result += ":" + std::to_string(error.site.line) + ":" + std::to_string(error.site.column);
    }
    if (!error.site.target.empty()) {
        result += " [target " + error.site.target + "]";
    }
    if (!error.site.key.empty()) {
        result += " (" + error.site.key + ")";
    }
    return result + ": " + error.message;
}

Failure::Failure(Error error) : std::runtime_error(render(error)), detail(std::move(error)) {}

void fail(const Site &site, std::string message, int exit_code) {
    throw Failure({site, std::move(message), exit_code});
}
} // namespace erlang_aot::project
