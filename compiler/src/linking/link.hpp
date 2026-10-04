#pragma once
#include "../codegen/output.hpp"
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace erlang_aot::linking {
struct LinkRequest {
    // Requested executable path; Windows targets gain ".exe" when the name has no extension.
    std::filesystem::path output;
    // Normalized triple of the objects; selects the Clang target, object suffix and runtime check.
    std::string target_triple;
    // Serialized module objects followed by the startup object, linked in batch order.
    std::span<const codegen::OutputBuffer> objects;
    // Explicit Clang driver and runtime archive; absent values use PATH and the runtime beside this compiler.
    std::optional<std::filesystem::path> linker;
    std::optional<std::filesystem::path> runtime_library;
    // Sources and manifests the executable must never replace.
    std::span<const std::filesystem::path> protected_inputs;
    // Create a missing output directory (manifest outputs) instead of rejecting it (explicit -o).
    bool create_directory = false;
};

// Own a uniquely created directory beside an output; it is removed with its contents on destruction.
class StagingDirectory {
  public:
    explicit StagingDirectory(const std::filesystem::path &parent);
    ~StagingDirectory();
    StagingDirectory(StagingDirectory &&other) noexcept;
    StagingDirectory(const StagingDirectory &) = delete;
    StagingDirectory &operator=(const StagingDirectory &) = delete;
    StagingDirectory &operator=(StagingDirectory &&) = delete;

    // Directory holding the staged objects, linker log and linked executable.
    [[nodiscard]] const std::filesystem::path &path() const { return directory; }

  private:
    // Empty after a move, so only the last owner removes the directory.
    std::filesystem::path directory;
};

struct StagedExecutable {
    // Keep the linked file private until every executable of the invocation is ready.
    StagingDirectory staging;
    // Linked file inside the staging directory and its final destination (".exe" already applied).
    std::filesystem::path staged;
    std::filesystem::path output;
    // Inputs the destination is checked against again just before replacement.
    std::vector<std::filesystem::path> protected_inputs;
    // Output of the successful linker run, forwarded as warnings by the caller.
    std::string warnings;
};

// Link in a private staging directory beside the output without touching the output.
// Failures throw std::runtime_error; the staging directory is removed.
StagedExecutable stage_executable(const LinkRequest &request);
// Replace the destination with a staged executable; any failure keeps an existing output unchanged.
void publish_executable(const StagedExecutable &executable);
// Stage and immediately publish one executable; returns linker warnings.
std::string link_executable(const LinkRequest &request);
} // namespace erlang_aot::linking
