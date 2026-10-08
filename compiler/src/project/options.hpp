#pragma once
#include "model.hpp"
#include <clause/compiler/preprocessor.hpp>

namespace clause::project {
// Compose immutable per-target settings with explicit manifest and CLI path bases.
PreprocessorOptions compose_options(const Target &target, const std::filesystem::path &base,
                                    const std::filesystem::path &invocation, const PreprocessorOptions &cli);
} // namespace clause::project
