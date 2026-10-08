#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace clause::cli {
struct EscriptSource {
    // Source bytes with the "#!" line replaced (by -module when missing); later line numbers are unchanged.
    std::string bytes;
    // One-based line of a "%%!" emulator-argument line, which compiled executables cannot honor.
    std::optional<std::size_t> emulator_arguments;
};

// Rewrite an escript header so later stages see an ordinary module; nullopt when there is no "#!" line.
std::optional<EscriptSource> escript_source(const std::filesystem::path &path, std::string_view bytes);
// Module name for escripts without a leading -module: file name with '.' replaced by '_', plus "__escript".
std::u32string escript_module_name(const std::filesystem::path &path);
} // namespace clause::cli
