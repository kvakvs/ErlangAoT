#include "atoms.hpp"
#include <algorithm>
#include <erlang_aot/abi/funs.hpp>
#include <erlang_aot/abi/records.hpp>

namespace erlang_aot::runtime {
namespace {
// Normalize the one permitted null spelling pointer without hiding invalid nonempty descriptors.
bool valid_atom(const abi::v1::AtomDescriptor &atom) {
    if (!atom.spelling && atom.size) {
        return false;
    }
    return valid_atom_spelling({atom.spelling ? atom.spelling : "", atom.size});
}

// Reject malformed descriptors without retaining any prefix of their spelling table.
bool valid_spellings(const abi::v1::ModuleDescriptor &descriptor) {
    if (!valid_atom_spelling({descriptor.name, descriptor.name_size})) {
        return false;
    }
    for (const auto &atom : std::span(descriptor.atoms, descriptor.atom_count)) {
        if (!valid_atom(atom)) {
            return false;
        }
    }
    for (const auto &item : std::span(descriptor.exports, descriptor.export_count)) {
        if (!item.name || !valid_atom_spelling({item.name, item.name_size})) {
            return false;
        }
    }
    return true;
}

// Metadata spellings share the same bounded runtime table as executable literals.
CodeResult<void> bind_metadata(AtomStorage &storage, const abi::v1::ModuleDescriptor &descriptor) {
    if (!storage.intern({descriptor.name, descriptor.name_size})) {
        return std::unexpected(CodeError::resource_limit);
    }
    for (const auto &item : std::span(descriptor.exports, descriptor.export_count)) {
        if (!storage.intern({item.name, item.name_size})) {
            return std::unexpected(CodeError::resource_limit);
        }
    }
    return {};
}

// A record names only slots of its own module, so its atoms are bound before any cell can use them.
bool valid_record(const abi::v1::ModuleDescriptor &module, const abi::v1::RecordDescriptor &record) {
    const auto slot = [&](std::size_t index) { return index < module.atom_count; };
    if (record.module != &module || !slot(record.module_atom) || !slot(record.name_atom) ||
        (record.field_count && !record.fields)) {
        return false;
    }
    return std::ranges::all_of(std::span(record.fields, record.field_count), slot);
}

// Bind every record definition to the module's atom words in descriptor order.
CodeResult<void> bind_records(ModuleAtoms &bindings, const abi::v1::ModuleDescriptor &descriptor) {
    if (descriptor.record_count && !descriptor.records) {
        return std::unexpected(CodeError::invalid_module);
    }
    const auto word = [&](std::size_t slot) { return bindings.slots[slot].word(); };
    bindings.records.reserve(descriptor.record_count);
    for (const auto &record : std::span(descriptor.records, descriptor.record_count)) {
        if (!valid_record(descriptor, record)) {
            return std::unexpected(CodeError::invalid_module);
        }
        RecordDefinition bound{&record, word(record.module_atom), word(record.name_atom), record.exported != 0, {}};
        bound.fields.reserve(record.field_count);
        for (const auto slot : std::span(record.fields, record.field_count)) {
            bound.fields.push_back(word(slot));
        }
        bindings.records.push_back(std::move(bound));
    }
    return {};
}

// A fun names only slots of its own module; a local fun's code takes its arguments, then its captured values.
bool valid_fun(const abi::v1::ModuleDescriptor &module, const abi::v1::FunDescriptor &fun) {
    const auto slot = [&](std::size_t index) { return index < module.atom_count; };
    if (fun.module != &module || !slot(fun.module_atom) || !slot(fun.function_atom)) {
        return false;
    }
    return fun.external || (fun.frame && fun.frame->arity >= fun.arity);
}

// Bind every fun definition to the module's atom words in descriptor order; only an external fun may lack code.
CodeResult<void> bind_funs(ModuleAtoms &bindings, const abi::v1::ModuleDescriptor &descriptor) {
    if (descriptor.fun_count && !descriptor.funs) {
        return std::unexpected(CodeError::invalid_module);
    }
    const auto word = [&](std::size_t slot) { return bindings.slots[slot].word(); };
    bindings.funs.reserve(descriptor.fun_count);
    for (const auto &fun : std::span(descriptor.funs, descriptor.fun_count)) {
        if (!valid_fun(descriptor, fun)) {
            return std::unexpected(CodeError::invalid_module);
        }
        const std::size_t captures = fun.external ? 0 : fun.frame->arity - fun.arity;
        bindings.funs.push_back(FunDefinition{.descriptor = &fun,
                                              .module = word(fun.module_atom),
                                              .function = word(fun.function_atom),
                                              .arity = fun.arity,
                                              .index = fun.index,
                                              .external = fun.external != 0,
                                              .captures = captures,
                                              .frame = fun.frame});
    }
    return {};
}
} // namespace

CodeResult<std::shared_ptr<const ModuleAtoms>> bind_atoms(AtomStorage &storage,
                                                          const abi::v1::ModuleDescriptor &descriptor) {
    if (!valid_spellings(descriptor)) {
        return std::unexpected(CodeError::invalid_module);
    }
    const auto metadata = bind_metadata(storage, descriptor);
    if (!metadata) {
        return std::unexpected(metadata.error());
    }
    auto bindings = std::make_shared<ModuleAtoms>();
    bindings->descriptor = &descriptor;
    bindings->slots.reserve(descriptor.atom_count);
    for (const auto &atom : std::span(descriptor.atoms, descriptor.atom_count)) {
        const auto term = storage.intern({atom.spelling ? atom.spelling : "", atom.size});
        if (!term) {
            return std::unexpected(CodeError::resource_limit);
        }
        bindings->slots.push_back(*term);
    }
    if (const auto records = bind_records(*bindings, descriptor); !records) {
        return std::unexpected(records.error());
    }
    if (const auto funs = bind_funs(*bindings, descriptor); !funs) {
        return std::unexpected(funs.error());
    }
    return bindings;
}
} // namespace erlang_aot::runtime

erlang_aot::abi::v1::TermWord erlang_aot_atom_v3(void *context, std::size_t slot, const void *descriptor) noexcept {
    using namespace erlang_aot;
    if (!context) {
        return 0;
    }
    auto &owner = *static_cast<runtime::ProcessContext *>(context);
    if (!owner.generated_calls().active() || owner.generated_calls().failure()) {
        return 0;
    }
    const auto word = owner.code_server().atom_word(descriptor, slot);
    if (!word) {
        owner.generated_calls().fail_service(abi::v1::Status::wrong_owner);
        return 0;
    }
    return *word;
}
