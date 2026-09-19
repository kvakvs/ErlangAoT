#include "project/create.hpp"
#include "project/decode.hpp"
#include "project/diagnostics.hpp"
#include "project/paths.hpp"
#include "support.hpp"
#include <atomic>
#include <thread>
using namespace erlang_aot::project;
namespace fs = std::filesystem;

// Require errors without claiming a successful partial or overwritten manifest.
template <typename Action> void rejects(Action action, int code = 1) {
    try {
        action();
    } catch (const Failure &error) {
        require(error.detail.exit_code == code);
        return;
    }
    require(false);
}

// Verify two simultaneous creators cannot replace each other's destination.
void simultaneous(const fs::path &root) {
    std::atomic<int> successes = 0;
    const auto create = [&] {
        try {
            (void)create_project(root, {"race"});
            ++successes;
        } catch (const Failure &) {
        }
    };
    std::thread first(create);
    std::thread second(create);
    first.join();
    second.join();
    require(successes == 1);
    require(decode(load(root / "race.toml")).targets.size() == 1);
}

// Cover filename completion, native paths, no-overwrite semantics, and I/O cleanup.
int main(int argc, char **argv) {
    require(argc == 2);
    const auto root = fs::absolute(argv[1]);
    fs::remove_all(root);
    fs::create_directories(root);
    require(creation_path(root, {"app"}) == root / "app.toml");
    require(creation_path(root, {"app.TOML"}) == root / "app.TOML");
    require(creation_path(root, {".toml"}) == root / ".toml");
    require(creation_path(root, {"app.config"}) == root / "app.config.toml");
    for (const auto *name : {"", ".", "..", "dir/"}) {
        rejects([&] { (void)creation_path(root, {name}); }, 2);
    }
    const auto created = create_project(root, {native_path("space λ")});
    require(decode(load(created)).targets.size() == 1);
    const auto before = fs::file_size(created);
    rejects([&] { (void)create_project(root, {created}); });
    require(fs::file_size(created) == before);
    fs::create_directories(root / "directory.toml");
    rejects([&] { (void)create_project(root, {"directory"}); });
    rejects([&] { (void)create_project(root, {"missing/app"}); });
    require(!fs::exists(root / "missing"));
    std::error_code error;
    fs::create_symlink(root / "absent", root / "link.toml", error);
    if (!error) {
        rejects([&] { (void)create_project(root, {"link"}); });
    }
    CreationIO write_failure;
    write_failure.write = [](auto &output, auto text) {
        output.write(text.data(), 10);
        output.flush();
        output.setstate(std::ios::badbit);
    };
    rejects([&] { (void)create_project(root, {"partial"}, write_failure); });
    require(!fs::exists(root / "partial.toml"));
    CreationIO close_failure;
    close_failure.close = [](auto &output) {
        output.close();
        output.setstate(std::ios::badbit);
    };
    rejects([&] { (void)create_project(root, {"close"}, close_failure); });
    require(!fs::exists(root / "close.toml"));
    simultaneous(root);
    require(!fs::exists(root / "src") && !fs::exists(root / "build"));
}
