#pragma once
#include "../codegen/output.hpp"
#include <filesystem>
#include <optional>
#include <span>
#include <string>

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
};

// Link in a private staging directory beside the output, then replace it with the complete executable.
// Failures throw std::runtime_error and leave any existing output unchanged; returns linker warnings.
std::string link_executable(const LinkRequest &request);
} // namespace erlang_aot::linking
