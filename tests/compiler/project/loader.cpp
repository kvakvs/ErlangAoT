#include "project/loader.hpp"
#include "project/diagnostics.hpp"
#include "support.hpp"
#include <fstream>
using namespace erlang_aot::project;

// Require a located failure from either syntax or injected I/O.
template <typename Action> void fails(Action action, std::string_view fragment) {
    try {
        action();
    } catch (const Failure &error) {
        require(error.detail.site.file == "sample.toml");
        require(std::string_view(error.what()).find(fragment) != std::string_view::npos);
        return;
    }
    require(false);
}

// Check input boundaries and retained syntax positions without filesystem fixtures.
void syntax() {
    require(parse_document("n = 1", "sample.toml").table["n"].value<int>() == 1);
    fails([] { (void)parse_document("n = 1\nn = 2", "sample.toml"); }, "sample.toml:2:");
    fails([] { (void)parse_document("n = [", "sample.toml"); }, "sample.toml:1:");
    Limits limits;
    limits.manifest_bytes = 5;
    require(parse_document("n = 1", "sample.toml", limits).table.size() == 1);
    fails([&] { (void)parse_document("n = 12", "sample.toml", limits); }, "byte limit");
    fails(
        [] {
            (void)load("sample.toml", {},
                       [](const auto &, auto) -> std::string { throw std::runtime_error("injected read failure"); });
        },
        "injected read failure");
}

// Exercise native paths, absent files, directories, and bounded file reading.
int main(int argc, char **argv) {
    require(argc == 2);
    syntax();
    const auto root = std::filesystem::path(argv[1]);
    std::filesystem::create_directories(root);
    const auto file = root / std::filesystem::path(u8"space λ.toml");
    {
        std::ofstream out(file);
        out << "n = 1";
    }
    require(load(file).table["n"].value<int>() == 1);
    for (const auto &path : {root, root / "absent.toml"}) {
        bool failed = false;
        try {
            (void)load(path);
        } catch (const Failure &) {
            failed = true;
        }
        require(failed);
    }
}
