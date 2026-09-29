#include "progress.hpp"
#include "llvm_state.hpp"

namespace erlang_aot::codegen {
void progress(const CompilationRequest &request, std::string_view phase, const std::filesystem::path &source,
              std::string_view module, std::string_view detail) {
    if (request.progress) {
        request.progress({std::string(phase), source, std::string(module), std::string(detail)});
    }
}

void progress_module(const Compilation &compilation, std::size_t index, std::string_view phase,
                     std::string_view message) {
    if (!compilation.request().progress) {
        return;
    }
    const auto &state = detail::state(compilation);
    progress(state.request, phase, state.request.inputs.at(index).source_path,
             state.modules.at(index)->getModuleIdentifier(), message);
}

void progress_modules(const Compilation &compilation, std::string_view phase, std::string_view message) {
    if (!compilation.request().progress) {
        return;
    }
    for (std::size_t i = 0; i < compilation.request().inputs.size(); ++i) {
        progress_module(compilation, i, phase, message);
    }
}
} // namespace erlang_aot::codegen
