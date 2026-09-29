#pragma once
#include "../codegen/output.hpp"
#include <filesystem>
#include <span>

namespace erlang_aot::artifacts {
// Encode a semantic UTF-8 identity into a reversible, case-insensitive-filesystem-safe basename.
std::string encoded_name(std::string_view identity);
// Validate every destination, stage the entire batch, then replace complete files in source order.
// Publication errors may leave earlier complete files installed; compilation/staging errors never do.
void publish(std::span<const codegen::OutputBuffer> outputs, const std::filesystem::path &root,
             std::span<const std::filesystem::path> inputs, std::string_view object_extension);
} // namespace erlang_aot::artifacts
