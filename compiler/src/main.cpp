#include "driver/command.hpp"
#include <cstdio>
#include <exception>

// Keep unexpected failures inside the CLI diagnostic and exit-code contract.
int main(int argc, char *argv[]) {
    try {
        return erlang_aot::cli::run_command(std::span{argv, static_cast<std::size_t>(argc)}.subspan(1));
    } catch (const std::exception &error) {
        std::fprintf(stderr, "erlangaot: error: %s\n", error.what());
        return 1;
    } catch (...) {
        std::fputs("erlangaot: error: unexpected failure\n", stderr);
        return 1;
    }
}
