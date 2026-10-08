#include <erlang_aot/runtime/code_server.hpp>
#include <mutex>

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
    const std::shared_lock lock(mutex_);
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
    const std::shared_lock lock(mutex_);
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
    const std::shared_lock lock(mutex_);
    return find_fun(descriptor);
}

const FunDefinition *CodeServer::find_fun(const void *descriptor) const noexcept {
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
    const std::shared_lock lock(mutex_);
    return find_export(name);
}

const void *CodeServer::find_export(const FunctionAtoms &name) const noexcept {
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

const void *CodeServer::function_frame(const Term &module, const Term &function, std::size_t arity) const noexcept {
    const std::shared_lock lock(mutex_);
    return find_function(module, function, arity);
}

const void *CodeServer::find_function(const Term &module, const Term &function, std::size_t arity) const noexcept {
    if (const auto *frame = find_export({module.word(), function.word(), arity})) {
        return frame;
    }
    const auto module_name = module.atom_spelling();
    const auto function_name = function.atom_spelling();
    return module_name && function_name ? builtins_.find(*module_name, *function_name, arity) : nullptr;
}

const FunDefinition &CodeServer::external_fun(const Term &module, const Term &function, std::size_t arity) {
    const FunctionAtoms key{module.word(), function.word(), arity};
    {
        const std::shared_lock lock(mutex_);
        if (const auto found = external_funs_.find(key); found != external_funs_.end()) {
            return *found->second;
        }
    }
    const std::unique_lock lock(mutex_);
    // Another worker may have built the definition between the two locks.
    auto &definition = external_funs_[key];
    if (!definition) {
        definition = std::make_unique<FunDefinition>(FunDefinition{.module = key.module,
                                                                   .function = key.function,
                                                                   .arity = arity,
                                                                   .external = true,
                                                                   .frame = find_function(module, function, arity)});
    }
    return *definition;
}

bool CodeServer::owns(const FunDefinition &definition) const noexcept {
    const std::shared_lock lock(mutex_);
    if (definition.descriptor) {
        return find_fun(definition.descriptor) == &definition;
    }
    const auto found = external_funs_.find({definition.module, definition.function, definition.arity});
    return found != external_funs_.end() && found->second.get() == &definition;
}

CodeResult<std::shared_ptr<const LoadedModule>> CodeServer::load(ModuleDefinition &&definition) {
    if (definition.name.empty() || !definition.image || !definition.functions) {
        return std::unexpected(CodeError::invalid_module);
    }
    const std::unique_lock lock(mutex_);
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
    const std::shared_lock lock(mutex_);
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
