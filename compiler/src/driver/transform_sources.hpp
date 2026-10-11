#pragma once
// Added for parse transforms: finding a project transform module's sources (docs/transforms.md#transform-modules).
#include <clause/compiler/preprocessor.hpp>
#include <filesystem>
#include <span>

namespace clause::cli {
struct SourcePlaces {
    // The batch's inputs, then the directories searched for Module.erl.
    std::span<const std::filesystem::path> inputs_;
    std::span<const std::filesystem::path> directories_;
};

// The source of a transform module and of the project modules it calls: Module.erl among the batch inputs, then
// in the search directories; modules found nowhere (OTP's own) are left to the host.
std::vector<std::filesystem::path> transform_sources(const std::u32string &module, const SourcePlaces &places,
                                                     const PreprocessorOptions &options);
} // namespace clause::cli
