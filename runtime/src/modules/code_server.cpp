#include <erlang_aot/runtime/code_server.hpp>

namespace erlang_aot::runtime {
std::shared_ptr<const CodeImage> CodeImage::linked() { return std::make_shared<CodeImage>(); }

// Copy spelling inside the caller's failure boundary; Debug STL string moves may allocate in noexcept code.
LoadedModule::LoadedModule(ModuleDefinition &&definition)
    : definition_{definition.name, definition.image, std::move(definition.functions), definition.atoms} {}

std::string_view LoadedModule::name() const noexcept { return definition_.name; }

const ModuleRegistry &LoadedModule::functions() const noexcept { return *definition_.functions; }

const ModuleAtoms *LoadedModule::atoms() const noexcept { return definition_.atoms.get(); }

const ModuleAtoms *CodeServer::find_atoms(const void *descriptor) const noexcept {
    for (const auto &[name, module] : modules_) {
        const auto *atoms = module->atoms();
        if (atoms && atoms->descriptor == descriptor) {
            return atoms;
        }
    }
    return nullptr;
}

TermResult<Word> CodeServer::atom_word(const void *descriptor, std::size_t slot) const noexcept {
    const auto *atoms = find_atoms(descriptor);
    if (!atoms) {
        return std::unexpected(TermError::wrong_owner);
    }
    if (slot >= atoms->slots.size()) {
        return std::unexpected(TermError::out_of_range);
    }
    return atoms->slots[slot].word();
}

const RecordDefinition *CodeServer::record_definition(const void *descriptor) const noexcept {
    for (const auto &[name, module] : modules_) {
        const auto *atoms = module->atoms();
        if (!atoms) {
            continue;
        }
        for (const auto &record : atoms->records) {
            if (record.descriptor == descriptor) {
                return &record;
            }
        }
    }
    return nullptr;
}

const FunDefinition *CodeServer::fun_definition(const void *descriptor) const noexcept {
    for (const auto &[name, module] : modules_) {
        const auto *atoms = module->atoms();
        if (!atoms) {
            continue;
        }
        for (const auto &fun : atoms->funs) {
            if (fun.descriptor == descriptor) {
                return &fun;
            }
        }
    }
    return nullptr;
}

const void *CodeServer::export_frame(const FunctionAtoms &name) const noexcept {
    for (const auto &[spelling, loaded] : modules_) {
        const auto *atoms = loaded->atoms();
        if (!atoms || atoms->module != name.module) {
            continue;
        }
        for (const auto &item : atoms->exports) {
            if (item.function == name.function && item.arity == name.arity) {
                return item.frame;
            }
        }
    }
    return nullptr;
}

const FunDefinition &CodeServer::external_fun(const FunctionAtoms &name) {
    auto &definition = external_funs_[name];
    if (!definition) {
        definition = std::make_unique<FunDefinition>(FunDefinition{.module = name.module,
                                                                   .function = name.function,
                                                                   .arity = name.arity,
                                                                   .external = true,
                                                                   .frame = export_frame(name)});
    }
    return *definition;
}

bool CodeServer::owns(const FunDefinition &definition) const noexcept {
    if (definition.descriptor) {
        return fun_definition(definition.descriptor) == &definition;
    }
    const auto found = external_funs_.find({definition.module, definition.function, definition.arity});
    return found != external_funs_.end() && found->second.get() == &definition;
}

CodeResult<std::shared_ptr<const LoadedModule>> CodeServer::load(ModuleDefinition &&definition) {
    if (definition.name.empty() || !definition.image || !definition.functions) {
        return std::unexpected(CodeError::invalid_module);
    }
    if (modules_.contains(definition.name)) {
        return std::unexpected(CodeError::duplicate_module);
    }
    if (definition.atoms && find_atoms(definition.atoms->descriptor)) {
        return std::unexpected(CodeError::invalid_module);
    }
    try {
        auto module = std::shared_ptr<LoadedModule>(new LoadedModule(std::move(definition)));
        module->definition_.functions->freeze();
        modules_.emplace(module->name(), module);
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
