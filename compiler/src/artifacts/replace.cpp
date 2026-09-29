#include "paths.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace erlang_aot::artifacts::detail {
void replace(const std::filesystem::path &from, const std::filesystem::path &to) {
#ifdef _WIN32
    if (!MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        throw std::filesystem::filesystem_error(
            "cannot publish artifact", from, to,
            std::error_code(static_cast<int>(GetLastError()), std::system_category()));
    }
#else
    std::filesystem::rename(from, to);
#endif
}
} // namespace erlang_aot::artifacts::detail
