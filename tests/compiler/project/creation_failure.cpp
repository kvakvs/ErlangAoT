#include "project/create.hpp"
#include "project/diagnostics.hpp"
#include "support.hpp"
#include <cstdio>
using namespace erlang_aot::project;

// Deterministic write/close faults cannot be requested through the CLI or ordinary filesystems.
void fails(const std::filesystem::path &root, const CreationIO &io) {
    try {
        (void)create_project(root, {"partial"}, io);
    } catch (const Failure &error) {
        require(std::string_view(error.what()).find("manifest") != std::string_view::npos);
        require(!std::filesystem::exists(root / "partial.toml"));
        return;
    }
    require(false);
}

// Preserve cleanup coverage independently of normal creation, now exercised by CLI processes.
int main(int argc, char **argv) {
    try {
        require(argc == 2);
        const auto root = std::filesystem::absolute(argv[1]);
        std::filesystem::create_directories(root);
        CreationIO write_failure;
        write_failure.write = [](auto &output, auto text) {
            const auto partial = text.substr(0, 10);
            output.write(partial.data(), static_cast<std::streamsize>(partial.size()));
            output.flush();
            output.setstate(std::ios::badbit);
        };
        fails(root, write_failure);
        CreationIO close_failure;
        close_failure.close = [](auto &output) {
            output.close();
            output.setstate(std::ios::badbit);
        };
        fails(root, close_failure);
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    } catch (...) {
        std::fputs("unexpected creation failure exception\n", stderr);
        return 1;
    }
}
