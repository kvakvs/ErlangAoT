#include "publication.hpp"
#include "../artifacts/artifacts.hpp"
#include "../codegen/target.hpp"

namespace clause::cli {
Publication publication(codegen::Compilation compilation, const std::filesystem::path &root,
                        std::vector<std::filesystem::path> protected_inputs) {
    protected_inputs.reserve(protected_inputs.size() + compilation.request().inputs.size());
    for (const auto &input : compilation.request().inputs) {
        protected_inputs.push_back(input.source_path);
    }
    const auto extension = codegen::object_extension(compilation);
    const auto target = compilation.request().project_target;
    return {std::move(compilation).take_result(), root, std::move(protected_inputs), extension, target};
}

void publish(const Publication &pending) {
    artifacts::publish(pending.result.outputs(), pending.root, pending.inputs, pending.object_extension);
}
} // namespace clause::cli
