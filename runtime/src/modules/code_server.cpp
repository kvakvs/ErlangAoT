#include <erlang_aot/runtime/code_server.hpp>

namespace erlang_aot::runtime {
std::shared_ptr<const CodeImage> CodeImage::linked() { return std::make_shared<CodeImage>(); }

LoadedModule::LoadedModule(ModuleDefinition definition) : definition_(std::move(definition)) {}

std::string_view LoadedModule::name() const noexcept { return definition_.name; }

const ModuleRegistry &LoadedModule::functions() const noexcept { return *definition_.functions; }

CodeResult<std::shared_ptr<const LoadedModule>> CodeServer::load(ModuleDefinition definition) {
    if (definition.name.empty() || !definition.image || !definition.functions) {
        return std::unexpected(CodeError::invalid_module);
    }
    if (modules_.contains(definition.name)) {
        return std::unexpected(CodeError::duplicate_module);
    }
    try {
        auto module = std::shared_ptr<LoadedModule>(new LoadedModule(std::move(definition)));
        modules_.emplace(module->name(), module);
        module->definition_.functions->freeze();
        return module;
    } catch (const std::bad_alloc &) {
        return std::unexpected(CodeError::resource_limit);
    }
}

CodeResult<std::shared_ptr<const LoadedModule>> CodeServer::find_module(std::string_view name) const {
    const auto found = modules_.find(name);
    if (found == modules_.end()) {
        return std::unexpected(CodeError::module_not_found);
    }
    return found->second;
}

CodeResult<ResolvedFunction> CodeServer::resolve(FunctionRequest request) const {
    const auto found = find_module(request.module);
    if (!found) {
        return std::unexpected(found.error());
    }
    const auto target = (*found)->functions().find(request.function, request.arity);
    if (!target) {
        if (target.error() == RegistryError::resource_limit) {
            return std::unexpected(CodeError::resource_limit);
        }
        return std::unexpected(CodeError::function_not_exported);
    }
    return ResolvedFunction(*found, *target, request.arity);
}
} // namespace erlang_aot::runtime
