#include "project/discovery.hpp"
#include "project/diagnostics.hpp"
#include "support.hpp"
#include <fstream>
using namespace erlang_aot::project;
namespace fs = std::filesystem;

// Require expansion failures without returning a successful partial source list.
template <typename Action> void rejects(Action action) {
    try {
        action();
    } catch (const Failure &) {
        return;
    }
    require(false);
}

// Exercise deterministic traversal, hidden files, depth limits, and link policy.
int main(int argc, char **argv) {
    require(argc == 2);
    const auto root = fs::absolute(argv[1]);
    fs::remove_all(root);
    fs::create_directories(root / "nested" / "empty");
    for (const auto *name : {"z.erl", ".hidden.erl", "nested/a.erl", "header.hrl"}) {
        std::ofstream out(root / name);
        out << "-module(example).";
    }
    const auto direct = wildcard_sources(root, {"*.erl", {}});
    require(direct.size() == 2 && direct.front().filename() == ".hidden.erl");
    const auto recursive = wildcard_sources(root, {"**/*.erl", {}});
    require(recursive.size() == 3);
    require(directory_sources(root, {".", {}}) == recursive);
    require(directory_sources(root, {"nested/empty", {}}).empty());
    rejects([&] { (void)wildcard_sources(root, {"missing*.erl", {}}); });
    rejects([&] { (void)directory_sources(root, {"missing", {}}); });
    rejects([&] { (void)directory_sources(root, {".", {}}, {1, 128, 1000}); });
    rejects([&] { (void)directory_sources(root, {".", {}}, {100, 1, 1000}); });
    rejects([&] { (void)wildcard_sources(root, {"**/*.erl", {}}, {100, 128, 1}); });
    std::error_code error;
    fs::create_directory_symlink(root, root / "nested" / "cycle", error);
    if (!error) {
        require(directory_sources(root, {".", {}}) == recursive);
    }
    error.clear();
    fs::create_symlink(root / "z.erl", root / "alias.erl", error);
    if (!error) {
        require(wildcard_sources(root, {"*.erl", {}}).size() == 3);
    }
    error.clear();
    fs::create_symlink(root / "absent.erl", root / "dangling.erl", error);
    if (!error) {
        rejects([&] { (void)wildcard_sources(root, {"*.erl", {}}); });
    }
    const auto special = root / "base[!]";
    fs::create_directories(special);
    {
        std::ofstream out(special / "module.erl");
        out << "-module(example).";
    }
    require(wildcard_sources(special, {"*.erl", {}}).size() == 1);
}
