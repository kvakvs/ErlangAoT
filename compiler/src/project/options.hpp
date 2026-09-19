#pragma once
#include "model.hpp"
#include <erlang_aot/compiler/preprocessor.hpp>

namespace erlang_aot::project {
// Compose immutable per-target settings with explicit manifest and CLI path bases.
PreprocessorOptions compose_options(const Target &target, const std::filesystem::path &base,
                                    const std::filesystem::path &invocation, const PreprocessorOptions &cli);
} // namespace erlang_aot::project
