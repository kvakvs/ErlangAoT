#include "project/options.hpp"
#include "project/diagnostics.hpp"
#include "support.hpp"
using namespace erlang_aot;
using namespace erlang_aot::project;

// Require option semantic failures to retain target context.
void rejects(const Target &target, const PreprocessorOptions &cli) {
    try {
        (void)compose_options(target, std::filesystem::current_path(), std::filesystem::current_path(), cli);
    } catch (const Failure &) {
        return;
    }
    require(false);
}

// Verify precedence and source-search separation without sharing mutable sessions.
int main() {
    const auto base = std::filesystem::current_path() / "manifest";
    const auto cwd = std::filesystem::current_path() / "invocation";
    Target target;
    target.name = {"app", {}};
    target.options.include_dirs = {{"include", {}}, {"vendor/include", {}}};
    target.options.source_search_paths = {{"not-a-header-path", {}}};
    target.options.defines = {{"FLAG", {}}, {"VALUE={1,atom}", {}}};
    target.options.applications.emplace("demo", Text{"vendor/demo", {}});
    target.options.enable_features = {{"maybe_expr", {}}};
    PreprocessorOptions cli;
    cli.include_paths = {"second", "first"};
    cli.applications.emplace("demo", "override/demo");
    cli.definitions = {"EXTRA=42"};
    cli.features = {{"maybe_expr", false}, {"compr_assign", true}};
    auto result = compose_options(target, base, cwd, cli);
    require(result.working_directory == base);
    require(result.include_paths == std::vector<std::filesystem::path>{cwd / "second", cwd / "first", base / "include",
                                                                       base / "vendor/include"});
    require(result.applications.at("demo") == cwd / "override/demo");
    require(result.definitions == std::vector<std::string>{"FLAG", "VALUE={1,atom}", "EXTRA=42"});
    require(result.features.front() == std::pair<std::string, bool>{"maybe_expr", true});
    require(result.features[1] == std::pair<std::string, bool>{"maybe_expr", false});
    result.definitions.clear();
    require(compose_options(target, base, cwd, cli).definitions.size() == 3);
    cli.definitions = {"FLAG=2"};
    rejects(target, cli);
    cli.definitions = {"BAD=1+2"};
    rejects(target, cli);
    cli.definitions.clear();
    cli.features = {{"unknown_feature", true}};
    rejects(target, cli);
}
