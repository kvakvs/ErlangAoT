#include "project/create.hpp"
#include "project/decode.hpp"
#include "project/diagnostics.hpp"
#include "project/paths.hpp"
#include "project/plan.hpp"
#include "support.hpp"
#include <fstream>
#include <iostream>
using namespace erlang_aot::project;
namespace fs = std::filesystem;

// Require bounded failures with diagnostics instead of partial successful plans.
template <typename Action> void rejects(Action action) {
    try {
        action();
    } catch (const Failure &error) {
        require(!error.detail.message.empty());
        return;
    }
    require(false);
}

// Build a repeatable bounded corpus in a native Unicode directory.
void corpus(const fs::path &root) {
    fs::create_directories(root / "src/deep/leaf");
    for (int index = 0; index < 128; ++index) {
        std::ofstream output(root / "src" / ("module" + std::to_string(index) + ".erl"));
        output << "-module(example).\n";
    }
    std::ofstream output(root / "src/deep/leaf/last.erl");
    output << "-module(last).\n";
}

// Exercise parser, schema, and discovery budgets through a single manifest workflow.
void budgets(const fs::path &root) {
    const auto path = root / "limits.toml";
    const std::string text = R"(schema_version=1
[[targets]]
name="app"
sources=["src/**/*.erl"]
[[targets]]
name="tests"
source_dirs=["src"]
)";
    {
        std::ofstream output(path, std::ios::binary);
        output << text;
    }
    Limits limits;
    limits.manifest_bytes = text.size();
    const auto manifest = decode(load(path, limits));
    --limits.manifest_bytes;
    rejects([&] { (void)load(path, limits); });
    limits = {};
    limits.targets = 1;
    rejects([&] { (void)decode(load(path), limits); });
    limits = {};
    limits.entries = 4;
    rejects([&] { (void)decode(load(path), limits); });
    PlanOptions options;
    options.working_directory = root;
    options.frontend = true;
    const auto plan = prepare(manifest, options);
    require(plan.targets.size() == 2 && plan.targets.front().sources.size() == 129);
    require(plan.targets.front().sources == plan.targets.back().sources);
    options.discovery.entries = 10;
    rejects([&] { (void)prepare(manifest, options); });
    options.discovery = {};
    options.discovery.depth = 1;
    rejects([&] { (void)prepare(manifest, options); });
    options.discovery = {};
    options.discovery.work = 20;
    rejects([&] { (void)prepare(manifest, options); });
    require(!fs::exists(root / "build"));
}

// Report native link/case capabilities and verify identity-based deduplication when available.
void aliases(const fs::path &root) {
    const auto source = root / "src/module0.erl";
    std::error_code error;
    fs::create_hard_link(source, root / "hard.erl", error);
    std::cout << "hard links: " << (error ? error.message() : "available") << '\n';
    Target target;
    target.name = {"aliases", {}};
    target.sources = {{"src/module0.erl", {}}};
    if (!error) {
        target.sources.push_back({"hard.erl", {}});
    }
    error.clear();
    fs::create_symlink(source, root / "link.erl", error);
    std::cout << "file symlinks: " << (error ? error.message() : "available") << '\n';
    if (!error) {
        target.sources.push_back({"link.erl", {}});
    }
    const bool case_alias = fs::exists(root / "SRC/module0.erl");
    std::cout << "case aliases: " << (case_alias ? "available" : "case-sensitive filesystem") << '\n';
    if (case_alias) {
        target.sources.push_back({"SRC/module0.erl", {}});
    }
    Manifest manifest{root / "aliases.toml", {target}};
    PlanOptions options;
    options.working_directory = root;
    options.frontend = true;
    require(prepare(manifest, options).targets.front().sources == std::vector<fs::path>{source});
}

// Check native path conversion separately from the portable wildcard grammar.
void native_paths(const fs::path &root) {
    const auto generated = create_project(root, {native_path("starter λ; space.TOML")});
    require(generated.filename() == native_path("starter λ; space.TOML"));
    require(decode(load(generated)).targets.size() == 1);
    rejects([&] { (void)create_project(root, {generated}); });
#ifdef _WIN32
    require(native_path("C:/work/main.erl").is_absolute());
    require(native_path(R"(C:\work\main.erl)").is_absolute());
    const auto unc = native_path(R"(\\server\share\main.erl)");
    require(unc.has_root_name());
    require(absolute_path(root, unc) == unc.lexically_normal());
    rejects([&] { (void)absolute_path(root, native_path("C:relative.erl")); });
    require(absolute_path(root, native_path(R"(src\module0.erl)")) == root / "src/module0.erl");
#else
    require(native_path("C:/work/main.erl").is_relative());
#endif
}

// Combine stress and filesystem tests without requiring optional native privileges.
int main(int argc, char **argv) {
    require(argc == 2);
    const auto root = fs::absolute(argv[1]) / native_path("Unicode λ; [base]");
    fs::remove_all(root);
    corpus(root);
    budgets(root);
    aliases(root);
    native_paths(root);
}
