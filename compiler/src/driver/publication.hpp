#pragma once
#include "../codegen/compilation.hpp"
#include <filesystem>

namespace erlang_aot::cli {
struct Publication {
    // Keep complete serialized batches independent of destroyed LLVM/AST ownership until project success.
    codegen::CompilationResult result;
    std::filesystem::path root;
    std::vector<std::filesystem::path> inputs;
    std::string object_extension;
    // Retain target context for publication failures after project execution has returned.
    std::string project_target;
};

// Retain or publish a completed batch according to the invocation's publication boundary.
Publication publication(codegen::Compilation compilation, const std::filesystem::path &root,
                        std::vector<std::filesystem::path> protected_inputs);
// Publish only complete batches; a platform replacement failure may leave earlier complete files installed.
void publish(const Publication &pending);
} // namespace erlang_aot::cli
