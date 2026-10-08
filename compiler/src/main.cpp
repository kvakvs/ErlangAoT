#include "driver/command.hpp"
#include <cstdio>
#include <exception>

// Keep unexpected failures inside the CLI diagnostic and exit-code contract.
int main(const int argc, char *argv[]) {
    try {
        return clause::cli::run_command(std::span{argv, static_cast<std::size_t>(argc)}.subspan(1));
    } catch (const std::exception &error) {
        std::fprintf(stderr, "clau: error: %s\n", error.what());
        return 1;
    } catch (...) {
        std::fputs("clau: error: unexpected failure\n", stderr);
        return 1;
    }
}
