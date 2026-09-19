#include "discovery.hpp"
#include "diagnostics.hpp"
#include "glob_utf8.hpp"
#include "paths.hpp"
#include <algorithm>
#include <limits>

namespace erlang_aot::project {
namespace {
namespace fs = std::filesystem;

struct Walk {
    // Keep selection identity and an optional filter alive throughout one traversal.
    fs::path root;
    Site site;
    const Glob *glob = nullptr;
    // Stop descending once a nonrecursive pattern cannot consume another component.
    std::size_t levels = std::numeric_limits<std::size_t>::max();
    DiscoveryLimits remaining;
};

// Report filesystem failures with the selected manifest entry and physical path.
void check_io(const std::error_code &error, const fs::path &path, const Site &site) {
    if (error) {
        fail(site, "cannot traverse " + path_text(path) + ": " + error.message());
    }
}

// Prune finite-depth patterns and reject directory depth exhaustion explicitly.
void prune(fs::recursive_directory_iterator &iterator, const Walk &walk) {
    std::error_code error;
    const auto status = iterator->symlink_status(error);
    check_io(error, iterator->path(), walk.site);
    if (!fs::is_directory(status)) {
        return;
    }
    const auto level = static_cast<std::size_t>(iterator.depth()) + 1;
    if (level >= walk.levels) {
        iterator.disable_recursion_pending();
        return;
    }
    if (level >= walk.remaining.depth) {
        fail(walk.site, "source traversal depth limit exceeded");
    }
}

// Filter before following file symlinks so dangling selected files produce diagnostics.
void collect(const fs::directory_entry &entry, Walk &walk, std::vector<fs::path> &result) {
    (void)filename_scalars(path_text(entry.path().filename()), walk.site);
    if (entry.path().extension() != ".erl") {
        return;
    }
    const auto relative = path_text(entry.path().lexically_relative(walk.root));
    if (walk.glob && !matches_with_budget(*walk.glob, relative, walk.remaining.work)) {
        return;
    }
    std::error_code error;
    const auto status = entry.status(error);
    check_io(error, entry.path(), walk.site);
    if (status.type() == fs::file_type::not_found) {
        fail(walk.site, "dangling selected source: " + path_text(entry.path()));
    }
    if (fs::is_regular_file(status)) {
        require_source(entry.path(), walk.site);
        result.push_back(entry.path());
    }
}

// Traverse with error-code iterators and stable sorting; default policy skips directory symlinks.
std::vector<fs::path> discover(Walk walk) {
    std::error_code error;
    fs::recursive_directory_iterator iterator(walk.root, fs::directory_options::none, error);
    check_io(error, walk.root, walk.site);
    std::vector<fs::path> result;
    while (iterator != fs::recursive_directory_iterator{}) {
        if (walk.remaining.entries == 0) {
            fail(walk.site, "source traversal entry limit exceeded");
        }
        --walk.remaining.entries;
        prune(iterator, walk);
        collect(*iterator, walk, result);
        iterator.increment(error);
        check_io(error, walk.root, walk.site);
    }
    std::ranges::sort(result, {}, [](const auto &path) { return path_text(path); });
    return result;
}

// Separate the explicitly named root from wildcard components before traversal.
std::pair<fs::path, fs::path> split_pattern(const fs::path &path) {
    fs::path root;
    fs::path suffix;
    bool wild = false;
    for (const auto &part : path) {
        wild = wild || is_pattern(path_text(part));
        if (wild) {
            suffix /= part;
        } else {
            root /= part;
        }
    }
    return {root, suffix};
}
} // namespace

std::vector<fs::path> directory_sources(const fs::path &base, const Text &directory, DiscoveryLimits limits) {
    const auto root = source_directory(base, directory);
    return discover({root, directory.site, nullptr, std::numeric_limits<std::size_t>::max(), limits});
}

std::vector<fs::path> wildcard_sources(const fs::path &base, const Text &pattern, DiscoveryLimits limits) {
    const auto [prefix, suffix] = split_pattern(native_path(pattern.value));
    const auto root = absolute_path(base, prefix);
    if (suffix.empty()) {
        return {literal_source(base, {path_text(root), pattern.site}, {})};
    }
    const auto glob = parse_glob({path_text(suffix), pattern.site});
    (void)source_directory(base, {path_text(root), pattern.site});
    const bool recursive = std::ranges::find(glob.components, U"**") != glob.components.end();
    const auto levels = recursive ? std::numeric_limits<std::size_t>::max() : glob.components.size();
    auto result = discover({root, pattern.site, &glob, levels, limits});
    if (result.empty()) {
        fail(pattern.site, "unmatched source pattern: " + pattern.value);
    }
    return result;
}
} // namespace erlang_aot::project
