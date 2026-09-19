#include "paths.hpp"
#include "diagnostics.hpp"
#include <fstream>

namespace erlang_aot::project {
std::filesystem::path native_path(std::string_view text) {
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

std::string path_text(const std::filesystem::path &path) {
    const auto bytes = path.generic_u8string();
    return {bytes.begin(), bytes.end()};
}

std::filesystem::path absolute_path(const std::filesystem::path &base, const std::filesystem::path &path) {
    if (path.has_root_name() && !path.is_absolute()) {
        fail({path, {}, {}, 0, 0}, "drive-relative paths are unsupported; use an absolute path");
    }
    return std::filesystem::absolute(base / path).lexically_normal();
}

void require_source(const std::filesystem::path &path, const Site &site) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) {
        fail(site, "source is not an accessible regular file: " + path_text(path));
    }
    if (!std::ifstream(path, std::ios::binary)) {
        fail(site, "cannot read source: " + path_text(path));
    }
}

namespace {
// Distinguish absence from a present invalid file, including dangling symlinks.
bool candidate(const std::filesystem::path &path, const Site &site) {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error == std::errc::no_such_file_or_directory) {
        return false;
    }
    if (error) {
        fail(site, "cannot inspect source " + path_text(path) + ": " + error.message());
    }
    if (status.type() == std::filesystem::file_type::not_found) {
        return false;
    }
    require_source(path, site);
    return true;
}
} // namespace

std::filesystem::path literal_source(const std::filesystem::path &base, const Text &source,
                                     std::span<const Text> roots) {
    const auto supplied = native_path(source.value);
    if (supplied.extension() != ".erl") {
        fail(source.site, "source must have .erl extension");
    }
    auto direct = absolute_path(base, supplied);
    if (candidate(direct, source.site)) {
        return direct;
    }
    if (supplied.is_relative()) {
        for (const auto &root : roots) {
            auto alternative = absolute_path(absolute_path(base, native_path(root.value)), supplied);
            if (candidate(alternative, source.site)) {
                return alternative;
            }
        }
    }
    fail(source.site, "cannot find source: " + source.value);
}

std::filesystem::path source_directory(const std::filesystem::path &base, const Text &directory) {
    const auto result = absolute_path(base, native_path(directory.value));
    std::error_code error;
    if (!std::filesystem::is_directory(result, error)) {
        fail(directory.site, "source directory is not accessible: " + path_text(result));
    }
    return result;
}
} // namespace erlang_aot::project
