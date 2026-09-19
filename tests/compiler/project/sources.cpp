#include "project/sources.hpp"
#include "project/diagnostics.hpp"
#include "project/paths.hpp"
#include "support.hpp"
#include <fstream>
using namespace erlang_aot::project;
namespace fs = std::filesystem;

// Verify overlapping selections retain first source order and physical identity.
int main(int argc, char **argv) {
    require(argc == 2);
    const auto root = fs::absolute(argv[1]);
    fs::remove_all(root);
    fs::create_directories(root / "src");
    fs::create_directories(root / "extra");
    for (const auto *file : {"src/a.erl", "src/b.erl", "extra/unused.erl"}) {
        std::ofstream out(root / file);
        out << "-module(example).";
    }
    Target target;
    target.name = {"app", {}};
    target.sources = {{"src/b.erl", {}}, {"src/*.erl", {}}, {"src/../src/b.erl", {}}};
    target.source_dirs = {{"src", {}}};
    target.options.source_search_paths = {{"extra", {}}};
    const auto result = target_sources(root, target);
    require(result == std::vector<fs::path>{root / "src/b.erl", root / "src/a.erl"});
    std::error_code error;
    fs::create_hard_link(root / "src/b.erl", root / "hard.erl", error);
    if (!error) {
        target.sources.push_back({"hard.erl", {}});
    }
    error.clear();
    fs::create_symlink(root / "src/a.erl", root / "link.erl", error);
    if (!error) {
        target.sources.push_back({"link.erl", {}});
    }
    if (fs::exists(root / "SRC" / "a.erl")) {
        target.sources.push_back({"SRC/a.erl", {}});
    }
    require(target_sources(root, target) == result);
    auto another = target;
    another.name.value = "tests";
    require(target_sources(root, another) == result);
    target.sources.clear();
    target.source_dirs.clear();
    bool failed = false;
    try {
        (void)target_sources(root, target);
    } catch (const Failure &) {
        failed = true;
    }
    require(failed);
}
