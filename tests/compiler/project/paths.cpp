#include "project/paths.hpp"
#include "project/diagnostics.hpp"
#include "support.hpp"
#include <fstream>
using namespace erlang_aot::project;
namespace fs = std::filesystem;

// Verify the lookup does not fall through an existing invalid candidate.
void invalid_candidate(const fs::path &root, const std::vector<Text> &search) {
    fs::create_directories(root / "blocked.erl");
    {
        std::ofstream out(root / "fallback" / "blocked.erl");
        out << "ok";
    }
    bool failed = false;
    try {
        (void)literal_source(root, {"blocked.erl", {}}, search);
    } catch (const Failure &) {
        failed = true;
    }
    require(failed);
}

// Exercise explicit bases, source fallback order, and native Unicode filenames.
int main(int argc, char **argv) {
    require(argc == 2);
    const auto root = fs::absolute(argv[1]);
    fs::remove_all(root);
    fs::create_directories(root / "fallback");
    fs::create_directories(root / "later");
    const auto supplied = native_path("space λ.erl");
    {
        std::ofstream out(root / "fallback" / supplied);
        out << "-module(example).";
    }
    {
        std::ofstream out(root / "later" / supplied);
        out << "-module(later).";
    }
    const std::vector<Text> search{{"absent", {}}, {"fallback", {}}, {"later", {}}};
    const auto cwd = fs::current_path();
    require(literal_source(root, {path_text(supplied), {}}, search) == root / "fallback" / supplied);
    require(fs::current_path() == cwd);
    {
        std::ofstream out(root / supplied);
        out << "-module(direct).";
    }
    require(literal_source(root, {path_text(supplied), {}}, search) == root / supplied);
    require(literal_source(root / "later", {path_text(root / supplied), {}}, {}) == root / supplied);
    require(source_directory(root, {"fallback/../later", {}}) == root / "later");
    invalid_candidate(root, search);
    std::error_code error;
    fs::create_directory_symlink(root / "later", root / "linked", error);
    if (!error) {
        require(absolute_path(root, native_path("linked/project.toml")).parent_path() == root / "linked");
    }
}
