#include "selection.hpp"
#include "diagnostics.hpp"
#include <numeric>
#include <unordered_map>
#include <unordered_set>

namespace erlang_aot::project {
namespace {
// List available names in manifest order for actionable selector diagnostics.
std::string available(const Manifest &manifest) {
    std::string result;
    for (const auto &target : manifest.targets) {
        if (!result.empty()) {
            result += ", ";
        }
        result += target.name.value;
    }
    return result;
}
} // namespace

std::vector<std::size_t> select_targets(const Manifest &manifest, std::span<const std::string> selectors) {
    std::vector<std::size_t> result;
    if (selectors.empty()) {
        result.resize(manifest.targets.size());
        std::iota(result.begin(), result.end(), std::size_t{0});
        return result;
    }
    std::unordered_map<std::string, std::size_t> names;
    for (std::size_t i = 0; i < manifest.targets.size(); ++i) {
        names.emplace(manifest.targets[i].name.value, i);
    }
    std::unordered_set<std::string> seen;
    for (const auto &name : selectors) {
        const auto found = names.find(name);
        if (found == names.end()) {
            fail({manifest.file, "targets", name, 0, 0},
                 "unknown target '" + name + "'; available targets: " + available(manifest), 2);
        }
        if (seen.insert(name).second) {
            result.push_back(found->second);
        }
    }
    return result;
}
} // namespace erlang_aot::project
