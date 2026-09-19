#include "plan.hpp"
#include "diagnostics.hpp"
#include "identity.hpp"
#include "paths.hpp"
#include "selection.hpp"
#include "sources.hpp"
#include <unordered_set>

namespace erlang_aot::project {
namespace {
// Reserve the native platform's default executable spelling without creating it.
std::filesystem::path default_output(const Target &target) {
#ifdef _WIN32
    return std::filesystem::path("build") / native_path(target.name.value + ".exe");
#else
    return std::filesystem::path("build") / native_path(target.name.value);
#endif
}

// Apply output precedence only for compilation requests.
std::optional<std::filesystem::path> output_path(const Target &target, const PlanOptions &options,
                                                 const std::filesystem::path &base) {
    if (options.frontend) {
        return std::nullopt;
    }
    if (options.output) {
        return absolute_path(options.working_directory, *options.output);
    }
    const auto path = target.output ? native_path(target.output->value) : default_output(target);
    return absolute_path(base, path);
}

// Identify existing aliases and normalize unresolved paths without writing directories.
std::string output_identity(const std::filesystem::path &path, const Site &site) {
    std::error_code error;
    const auto present = std::filesystem::exists(path, error);
    if (error) {
        fail(site, "cannot inspect output " + path_text(path) + ": " + error.message());
    }
    if (present) {
        return "file:" + file_identity(path, site);
    }
    const auto normalized = std::filesystem::weakly_canonical(path, error);
    if (error) {
        fail(site, "cannot normalize output " + path_text(path) + ": " + error.message());
    }
    return "path:" + path_text(normalized);
}

// Refuse colliding compilation destinations across the selected target set.
void outputs(const Invocation &invocation) {
    std::unordered_set<std::string> identities;
    for (const auto &target : invocation.targets) {
        if (!target.output) {
            continue;
        }
        const Site site{invocation.file, "output", target.name, 0, 0};
        if (!identities.insert(output_identity(*target.output, site)).second) {
            fail(site, "selected targets have colliding output destinations");
        }
    }
}
} // namespace

Invocation prepare(const Manifest &manifest, const PlanOptions &options) {
    const auto selected = select_targets(manifest, options.selectors);
    if (options.output && (options.frontend || selected.size() != 1)) {
        fail({manifest.file, "output", {}, 0, 0},
             "--output requires exactly one compilation target and no check/print mode", 2);
    }
    const auto file = absolute_path(options.working_directory, manifest.file);
    const auto base = file.parent_path();
    Invocation result{file, {}, options.frontend};
    for (const auto index : selected) {
        const auto &target = manifest.targets[index];
        result.targets.push_back({target.name.value, target_sources(base, target, options.discovery),
                                  compose_options(target, base, options.working_directory, options.preprocessing),
                                  output_path(target, options, base)});
    }
    outputs(result);
    return result;
}
} // namespace erlang_aot::project
