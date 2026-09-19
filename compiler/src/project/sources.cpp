#include "sources.hpp"
#include "diagnostics.hpp"
#include "identity.hpp"
#include "paths.hpp"
#include <unordered_set>

namespace erlang_aot::project {
namespace {
struct Collection {
    // Preserve first-spelling order while identifying physical aliases in expected linear time.
    std::vector<std::filesystem::path> paths;
    std::unordered_set<std::string> identities;
};

// Append only the first spelling of each existing filesystem object.
void append(Collection &result, const std::vector<std::filesystem::path> &paths, const Site &site) {
    for (const auto &path : paths) {
        if (result.identities.insert(file_identity(path, site)).second) {
            result.paths.push_back(path);
        }
    }
}
} // namespace

std::vector<std::filesystem::path> target_sources(const std::filesystem::path &base, const Target &target,
                                                  DiscoveryLimits limits) {
    Collection result;
    for (const auto &source : target.sources) {
        if (is_pattern(source.value)) {
            append(result, wildcard_sources(base, source, limits), source.site);
        } else {
            append(result, {literal_source(base, source, target.options.source_search_paths)}, source.site);
        }
    }
    for (const auto &directory : target.source_dirs) {
        append(result, directory_sources(base, directory, limits), directory.site);
    }
    if (result.paths.empty()) {
        fail(target.name.site, "target resolves to zero source files");
    }
    return result.paths;
}
} // namespace erlang_aot::project
