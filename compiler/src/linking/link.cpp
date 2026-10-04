#include "link.hpp"
#include "../artifacts/paths.hpp"
#include "../project/paths.hpp"
#include "toolchain.hpp"
#include <fstream>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/TargetParser/Triple.h>
#include <stdexcept>

namespace erlang_aot::linking {
namespace {
// Remove a staging tree; a failure (even allocation) during cleanup only leaves the directory behind.
bool remove_tree(const std::filesystem::path &directory) noexcept {
    try {
        std::error_code error;
        std::filesystem::remove_all(directory, error);
        return !error;
    } catch (...) {
        return false;
    }
}

struct Staging {
    // Own a uniquely created directory beside the output; objects, logs and partial executables live here.
    std::filesystem::path directory;

    // Reserve the directory atomically so concurrent invocations never share files.
    explicit Staging(const std::filesystem::path &parent) {
        llvm::SmallString<256> created;
        if (const auto error = llvm::sys::fs::createUniqueDirectory(utf8_path(parent / ".erlangaot-link"), created)) {
            throw std::runtime_error("cannot create link staging directory in " + project::path_text(parent) + ": " +
                                     error.message());
        }
        directory = project::native_path(created.str());
    }

    // Discard everything the link produced unless the executable was already moved out.
    ~Staging() { static_cast<void>(remove_tree(directory)); }

    Staging(const Staging &) = delete;
    Staging &operator=(const Staging &) = delete;
    Staging(Staging &&) = delete;
    Staging &operator=(Staging &&) = delete;
};

// Resolve the invocation-relative output, adding ".exe" for Windows targets when no extension is given.
std::filesystem::path executable_path(const LinkRequest &request) {
    auto path = project::absolute_path(std::filesystem::current_path(), request.output);
    if (!path.has_filename()) {
        throw std::runtime_error("output path has no file name: " + project::path_text(request.output));
    }
    if (llvm::Triple(request.target_triple).isOSWindows() && !path.has_extension()) {
        path += ".exe";
    }
    return path;
}

// Require an existing output directory and a replaceable destination that is not an input.
void check_destination(const std::filesystem::path &path, std::span<const std::filesystem::path> inputs) {
    std::error_code error;
    if (!std::filesystem::is_directory(path.parent_path(), error)) {
        throw std::runtime_error("output directory does not exist: " + project::path_text(path.parent_path()));
    }
    artifacts::detail::validate_destination(path, inputs);
}

// Write each object under a short indexed name and return the paths in link order.
std::vector<std::string> stage_objects(const Staging &staging, const LinkRequest &request) {
    const auto extension = llvm::Triple(request.target_triple).isOSBinFormatCOFF() ? ".obj" : ".o";
    std::vector<std::string> paths;
    for (const auto &object : request.objects) {
        const auto path = staging.directory / ("m" + std::to_string(paths.size()) + extension);
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char *>(object.bytes.data()),
                   static_cast<std::streamsize>(object.bytes.size()));
        file.close();
        if (!file) {
            throw std::runtime_error("cannot write object for linking: " + project::path_text(path));
        }
        paths.push_back(utf8_path(path));
    }
    return paths;
}

// Build the Clang driver command: C++ link mode, explicit target, staged output, objects, then the runtime.
std::vector<std::string> link_arguments(const LinkRequest &request, const std::filesystem::path &staged,
                                        std::vector<std::string> objects, const std::filesystem::path &runtime) {
    std::vector<std::string> arguments{"--driver-mode=g++", "--target=" + request.target_triple, "-o",
                                       utf8_path(staged)};
    arguments.insert(arguments.end(), std::make_move_iterator(objects.begin()), std::make_move_iterator(objects.end()));
    arguments.push_back(utf8_path(runtime));
    return arguments;
}

// Turn a failed driver run into one diagnostic that carries the linker's own output.
std::runtime_error link_failure(const std::filesystem::path &output, const std::string &linker, const LinkerRun &run) {
    auto message = "linking " + project::path_text(output) + " failed: " + linker + " exited with status " +
                   std::to_string(run.status);
    if (!run.output.empty()) {
        message += ":\n" + run.output;
    }
    return std::runtime_error(message);
}
} // namespace

std::string link_executable(const LinkRequest &request) {
    const auto output = executable_path(request);
    check_destination(output, request.protected_inputs);
    const auto linker = find_linker(request.linker);
    const auto runtime = find_runtime_library(request.runtime_library);
    check_runtime_target(runtime, request.target_triple);
    const Staging staging(output.parent_path());
    const auto staged = staging.directory / output.filename();
    const auto arguments = link_arguments(request, staged, stage_objects(staging, request), runtime);
    const auto run = run_linker(linker, arguments, staging.directory / "link.log");
    std::error_code error;
    if (run.status != 0 || !std::filesystem::is_regular_file(staged, error)) {
        throw link_failure(output, linker, run);
    }
    check_destination(output, request.protected_inputs);
    artifacts::detail::replace(staged, output);
    return run.output;
}
} // namespace erlang_aot::linking
