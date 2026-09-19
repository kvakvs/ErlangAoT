#include "identity.hpp"
#include "diagnostics.hpp"
#include "paths.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <sys/stat.h>
#endif

namespace erlang_aot::project {
#ifdef _WIN32
namespace {
struct Handle {
    // Own the metadata handle so every failure path closes it.
    HANDLE value;

    // Close only handles successfully returned by CreateFileW.
    ~Handle() {
        if (value != INVALID_HANDLE_VALUE) {
            CloseHandle(value);
        }
    }
};
} // namespace

std::string file_identity(const std::filesystem::path &path, const Site &site) {
    const Handle handle{CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                    FILE_FLAG_BACKUP_SEMANTICS, nullptr)};
    if (handle.value == INVALID_HANDLE_VALUE) {
        fail(site, "cannot inspect filesystem identity: " + path_text(path));
    }
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(handle.value, &info)) {
        fail(site, "cannot obtain filesystem identity: " + path_text(path));
    }
    return std::to_string(info.dwVolumeSerialNumber) + ":" + std::to_string(info.nFileIndexHigh) + ":" +
           std::to_string(info.nFileIndexLow);
}
#else
std::string file_identity(const std::filesystem::path &path, const Site &site) {
    struct stat info{};
    if (::stat(path.c_str(), &info) != 0) {
        const std::error_code error(errno, std::generic_category());
        fail(site, "cannot inspect filesystem identity " + path_text(path) + ": " + error.message());
    }
    return std::to_string(info.st_dev) + ":" + std::to_string(info.st_ino);
}
#endif
} // namespace erlang_aot::project
