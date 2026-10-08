#include "project/decode.hpp"
#include "project/diagnostics.hpp"
#include "project/glob.hpp"
#include "project/paths.hpp"
#include "project/plan.hpp"
#include "support.hpp"
#include <cstdio>
#include <fstream>
using namespace clause::project;

// Keep injected budgets and read failures that the product CLI deliberately does not expose.
template <typename Action> void rejects(Action action, std::string_view message) {
    try {
        action();
    } catch (const Failure &error) {
        require(std::string_view(error.what()).find(message) != std::string_view::npos);
        return;
    }
    require(false);
}

// Tiny limits exercise exact boundaries without allocating production-sized resource corpora.
void decoding() {
    Limits limits;
    limits.manifest_bytes = 5;
    require(parse_document("n = 1", "limits.toml", limits).table.size() == 1);
    rejects([&] { (void)parse_document("n = 12", "limits.toml", limits); }, "byte limit");
    rejects(
        [] {
            (void)load("limits.toml", {},
                       [](const auto &, auto) -> std::string { throw std::runtime_error("injected read failure"); });
        },
        "injected read failure");
    const auto document = parse_document("schema_version=1\n[[targets]]\nname='app'\nsources=['a.erl']", "limits.toml");
    limits = {};
    limits.targets = 0;
    rejects([&] { (void)decode(document, limits); }, "target count limit");
    limits.targets = 1;
    limits.entries = 1;
    rejects([&] { (void)decode(document, limits); }, "entry limit");
}

// Invalid UTF-8 filenames cannot be created portably; matcher work budgets are API-only.
void matching() {
    rejects([] { (void)matches(parse_glob({"*", {}}), std::string(1, static_cast<char>(0xff))); }, "UTF-8");
    rejects([] { (void)matches(parse_glob({"**/*.erl", {}}), "long/path/file.erl", {1}); }, "limit");
    std::string adversarial;
    for (int index = 0; index < 1000; ++index) {
        adversarial += "*a";
    }
    rejects([&] { (void)matches(parse_glob({adversarial, {}}), std::string(1000, 'a'), {100}); }, "limit");
}

// Discovery's injectable entry/depth/work ceilings must reject before publishing a partial plan.
void discovery(const std::filesystem::path &root) {
    std::filesystem::create_directories(root / "src/deep/leaf");
    {
        std::ofstream output(root / "src/deep/leaf/main.erl");
        output << "-module(main).";
    }
    const auto manifest = decode(
        parse_document("schema_version=1\n[[targets]]\nname='app'\nsources=['src/**/*.erl']", root / "limits.toml"));
    PlanOptions options;
    options.working_directory = root;
    options.frontend = true;
    require(prepare(manifest, options).targets.front().sources.size() == 1);
    options.discovery.entries = 1;
    rejects([&] { (void)prepare(manifest, options); }, "limit");
    options.discovery = {};
    options.discovery.depth = 1;
    rejects([&] { (void)prepare(manifest, options); }, "limit");
    options.discovery = {};
    options.discovery.work = 1;
    rejects([&] { (void)prepare(manifest, options); }, "limit");
}

// The real reader accepts exactly the byte budget and rejects one byte beyond it.
void file_budget(const std::filesystem::path &root) {
    const auto path = root / "byte-limit.toml";
    {
        std::ofstream output(path, std::ios::binary);
        output << "n = 1";
    }
    Limits limits;
    limits.manifest_bytes = 5;
    require(load(path, limits).table.size() == 1);
    limits.manifest_bytes = 4;
    rejects([&] { (void)load(path, limits); }, "byte limit");
}

// UNC and drive-relative syntax needs no live network share to validate its path contract.
void native_syntax(const std::filesystem::path &root) {
#ifdef _WIN32
    require(native_path("C:/work/main.erl").is_absolute());
    require(native_path(R"(C:\work\main.erl)").is_absolute());
    const auto unc = native_path(R"(\\server\share\main.erl)");
    require(unc.has_root_name() && absolute_path(root, unc) == unc.lexically_normal());
    rejects([&] { (void)absolute_path(root, native_path("C:relative.erl")); }, "drive-relative");
    require(absolute_path(root, native_path(R"(src\main.erl)")) == root / "src/main.erl");
#else
    (void)root;
    require(native_path("C:/work/main.erl").is_relative());
#endif
}

// Report invariant failures instead of escaping main on hosts with different exception runtimes.
int main(int argc, char **argv) {
    try {
        require(argc == 2);
        decoding();
        matching();
        discovery(std::filesystem::absolute(argv[1]));
        file_budget(std::filesystem::absolute(argv[1]));
        native_syntax(std::filesystem::absolute(argv[1]));
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    } catch (...) {
        std::fputs("unexpected project limits exception\n", stderr);
        return 1;
    }
}
