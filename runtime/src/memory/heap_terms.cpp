#include "heap_object.hpp"
#include "heap_storage.hpp"
#include <erlang_aot/runtime/atoms.hpp>

namespace erlang_aot::runtime::detail {
TermResult<Term> TermAccess::admit(Word value, const std::shared_ptr<HeapStorage> &storage) noexcept {
    if (!storage->alive()) {
        return std::unexpected(TermError::expired_context);
    }
    const auto kind = TermTag{value}.get_kind();
    if (kind == TermKind::atom) {
        return storage->atoms->lookup(value);
    }
    if (kind != TermKind::boxed && kind != TermKind::list) {
        return Term::from_word(value);
    }
    const auto object = storage->objects.find(value);
    if (object == storage->objects.end()) {
        return std::unexpected(TermError::wrong_owner);
    }
    Term result;
    result.value_ = value;
    result.heap_ = storage;
    result.object_ = &object->second;
    return result;
}

TermResult<const HeapObject *> TermAccess::object(const Term &value) noexcept {
    if (!value.heap_ || !value.object_) {
        return std::unexpected(TermError::wrong_type);
    }
    if (!value.heap_->alive()) {
        return std::unexpected(TermError::expired_context);
    }
    return value.object_;
}

TermResult<Term> TermAccess::child(const Term &parent, Word value) noexcept {
    const auto checked = object(parent);
    if (!checked) {
        return std::unexpected(checked.error());
    }
    return admit(value, parent.heap_);
}

TermResult<void> TermAccess::validate(const Term &value) noexcept {
    if (value.heap_) {
        return object(value).transform([](const HeapObject *) {});
    }
    if (value.is_atom()) {
        return {};
    }
    return Term::from_word(value.word()).transform([](const Term &) {});
}
} // namespace erlang_aot::runtime::detail
