#include "atoms.hpp"

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
