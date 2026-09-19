#include "project/selection.hpp"
#include "project/diagnostics.hpp"
#include "support.hpp"
using namespace erlang_aot::project;

// Verify pure selection never inspects source paths and preserves both ordering modes.
int main() {
    Manifest manifest{"selection.toml", {}};
    for (const auto *name : {"app", "tests", "tools"}) {
        Target target;
        target.name = {name, {}};
        target.sources = {{"absent.erl", {}}};
        manifest.targets.push_back(target);
    }
    require(select_targets(manifest, {}) == std::vector<std::size_t>{0, 1, 2});
    const std::vector<std::string> selectors{"tools", "app", "tools"};
    require(select_targets(manifest, selectors) == std::vector<std::size_t>{2, 0});
    const std::vector<std::string> bad{"App"};
    bool failed = false;
    try {
        (void)select_targets(manifest, bad);
    } catch (const Failure &error) {
        failed = true;
        require(error.detail.exit_code == 2);
        require(std::string_view(error.what()).find("app, tests, tools") != std::string_view::npos);
    }
    require(failed);
}
