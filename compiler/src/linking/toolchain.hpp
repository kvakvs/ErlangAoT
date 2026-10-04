#pragma once
#include <filesystem>
#include <optional>
#include <span>
#include <string>

namespace erlang_aot::linking {
struct LinkerRun {
    // Exit status of the Clang driver and its combined stdout/stderr text (bounded).
    int status = 0;
    std::string output;
};

// Spell a native path as UTF-8 for LLVM process and file APIs.
std::string utf8_path(const std::filesystem::path &path);
// Resolve the Clang driver: an explicit path or name must be executable; otherwise search PATH.
std::string find_linker(const std::optional<std::filesystem::path> &linker);
// Run the driver to completion, capturing its output in `log`; failure to start it throws.
LinkerRun run_linker(const std::string &program, std::span<const std::string> arguments,
                     const std::filesystem::path &log);
// Resolve the runtime archive: an explicit path, else the library built beside this compiler.
std::filesystem::path find_runtime_library(const std::optional<std::filesystem::path> &library);
// Require every native object in the runtime archive to match the target architecture and format.
void check_runtime_target(const std::filesystem::path &library, const std::string &target_triple);
} // namespace erlang_aot::linking
