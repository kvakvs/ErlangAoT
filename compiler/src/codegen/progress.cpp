#include "progress.hpp"
#include "llvm_state.hpp"

namespace clause::codegen {
void progress(const CompilationRequest &request, const std::string_view phase, const std::filesystem::path &source,
              const std::string_view module, const std::string_view detail) {
    if (request.progress) {
        request.progress({std::string(phase), source, std::string(module), std::string(detail)});
    }
}

void progress_module(const Compilation &compilation, const std::size_t index, const std::string_view phase,
                     const std::string_view message) {
    if (!compilation.request().progress) {
        return;
    }
    const auto &state = detail::state(compilation);
    // The startup module follows the batch's inputs and has no source file.
    const auto &inputs = state.request.inputs;
    const auto source = index < inputs.size() ? inputs[index].source_path : std::filesystem::path{};
    progress(state.request, phase, source, state.modules.at(index)->getModuleIdentifier(), message);
}

void progress_modules(const Compilation &compilation, const std::string_view phase, const std::string_view message) {
    if (!compilation.request().progress) {
        return;
    }
    for (std::size_t i = 0; i < compilation.request().inputs.size(); ++i) {
        progress_module(compilation, i, phase, message);
    }
}
} // namespace clause::codegen
