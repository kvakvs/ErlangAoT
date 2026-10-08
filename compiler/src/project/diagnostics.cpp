#include "diagnostics.hpp"

namespace clause::project {
std::string where(const Site &site) {
    const auto bytes = site.file.generic_u8string();
    std::string result(bytes.begin(), bytes.end());
    if (site.line != 0) {
        result += ":" + std::to_string(site.line) + ":" + std::to_string(site.column);
    }
    if (!site.target.empty()) {
        result += " [target " + site.target + "]";
    }
    if (!site.key.empty()) {
        result += " (" + site.key + ")";
    }
    return result;
}

std::string render(const Error &error) { return where(error.site) + ": " + error.message; }

Failure::Failure(Error error) : std::runtime_error(render(error)), detail(std::move(error)) {}

void fail(const Site &site, std::string message, const int exit_code) {
    throw Failure({site, std::move(message), exit_code});
}
} // namespace clause::project
