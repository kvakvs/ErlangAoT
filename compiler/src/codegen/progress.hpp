#pragma once
#include "compilation.hpp"

namespace clause::codegen {
// Notify a synchronous observer only when a phase actually begins; no callback means no formatting work.
void progress(const CompilationRequest &request, std::string_view phase, const std::filesystem::path &source,
              std::string_view module = {}, std::string_view detail = {});
// Report one LLVM module immediately before its phase begins.
void progress_module(const Compilation &compilation, std::size_t index, std::string_view phase,
                     std::string_view detail = {});
// Report a batch-wide LLVM phase in stable source/module order using current semantic module identities.
void progress_modules(const Compilation &compilation, std::string_view phase, std::string_view detail = {});
} // namespace clause::codegen
