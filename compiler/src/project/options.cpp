#include "options.hpp"
#include "diagnostics.hpp"
#include "paths.hpp"

namespace erlang_aot::project {
namespace {
// Preserve last-CLI-first include order, followed by manifest declaration order.
void includes(PreprocessorOptions &result, const TargetOptions &target, const std::filesystem::path &base,
              const std::filesystem::path &invocation) {
    for (auto &path : result.include_paths) {
        path = absolute_path(invocation, path);
    }
    for (const auto &path : target.include_dirs) {
        result.include_paths.push_back(absolute_path(base, native_path(path.value)));
    }
}

// Keep macro spelling untouched so the existing preprocessor validates Erlang literals.
std::vector<std::string> definitions(const TargetOptions &target, const PreprocessorOptions &cli) {
    std::vector<std::string> result;
    result.reserve(target.defines.size() + cli.definitions.size());
    for (const auto &value : target.defines) {
        result.push_back(value.value);
    }
    result.insert(result.end(), cli.definitions.begin(), cli.definitions.end());
    return result;
}

// Apply manifest feature changes before the original ordered CLI changes.
std::vector<std::pair<std::string, bool>> features(const TargetOptions &target, const PreprocessorOptions &cli) {
    std::vector<std::pair<std::string, bool>> result;
    result.reserve(target.enable_features.size() + target.disable_features.size() + cli.features.size());
    for (const auto &value : target.enable_features) {
        result.emplace_back(value.value, true);
    }
    for (const auto &value : target.disable_features) {
        result.emplace_back(value.value, false);
    }
    result.insert(result.end(), cli.features.begin(), cli.features.end());
    return result;
}

// Overlay application names while resolving each layer against its own directory.
std::map<std::string, std::filesystem::path> applications(const TargetOptions &target, const PreprocessorOptions &cli,
                                                          const std::filesystem::path &base,
                                                          const std::filesystem::path &invocation) {
    std::map<std::string, std::filesystem::path> result;
    for (const auto &[name, path] : target.applications) {
        result.emplace(name, absolute_path(base, native_path(path.value)));
    }
    for (const auto &[name, path] : cli.applications) {
        result.insert_or_assign(name, absolute_path(invocation, path));
    }
    return result;
}

// Validate definitions and feature names using the real frontend before executing any source.
void validate(const PreprocessorOptions &options, const Site &site) {
    SourceManager sources;
    PreprocessorSession session(sources.add("<project-options>", ""), options);
    while (const auto event = session.next()) {
        const auto *diagnostic = std::get_if<Diagnostic>(&*event);
        if (diagnostic && diagnostic->severity == Severity::error) {
            fail(site, erlang_aot::render(*diagnostic));
        }
    }
}
} // namespace

PreprocessorOptions compose_options(const Target &target, const std::filesystem::path &base,
                                    const std::filesystem::path &invocation, const PreprocessorOptions &cli) {
    auto result = cli;
    result.working_directory = base;
    includes(result, target.options, base, invocation);
    result.definitions = definitions(target.options, cli);
    result.features = features(target.options, cli);
    result.applications = applications(target.options, cli, base, invocation);
    validate(result, target.name.site);
    return result;
}
} // namespace erlang_aot::project
