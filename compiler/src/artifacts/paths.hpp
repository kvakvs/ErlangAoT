#pragma once
#include "artifacts.hpp"

namespace clause::artifacts::detail {
struct Destination {
    // Borrow serialized bytes while owning the validated native destination.
    const codegen::OutputBuffer *output;
    std::filesystem::path path;
};

// Reject input aliases, links and nonregular destinations before staging or replacing any file.
void validate_destination(const std::filesystem::path &path, std::span<const std::filesystem::path> inputs);
// Resolve names and reject duplicate/aliased destinations for the complete batch.
std::vector<Destination> plan(std::span<const codegen::OutputBuffer> outputs, const std::filesystem::path &root,
                              std::span<const std::filesystem::path> inputs, std::string_view object_extension);
// Replace a complete same-filesystem staged file, reporting platform errors without deleting the old file first.
void replace(const std::filesystem::path &from, const std::filesystem::path &to);
} // namespace clause::artifacts::detail
