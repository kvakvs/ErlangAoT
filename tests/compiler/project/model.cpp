#include "project/model.hpp"
#include "support.hpp"
#include <type_traits>
using namespace erlang_aot::project;

// Verify configuration ownership, defaults, and independent target mutation.
int main() {
    static_assert(std::is_nothrow_move_constructible_v<Manifest>);
    Manifest original{"project.toml", {}};
    Target target;
    target.name = {"app", {original.file, "name", "app", 3, 1}};
    target.sources.push_back({"src/main.erl", {original.file, "sources", "app", 4, 12}});
    original.targets.push_back(target);
    original.targets.push_back(target);
    original.targets[1].sources.front().value = "test.erl";
    const auto owned = std::move(original);
    require(owned.targets[0].sources.front().value == "src/main.erl");
    require(owned.targets[1].sources.front().site.line == 4);
    require(!owned.targets[0].output);
    require(owned.targets[0].options.defines.empty());
    require(owned.targets[0].options.applications.empty());
}
