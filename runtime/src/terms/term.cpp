#include "../memory/heap_storage.hpp"
#include <erlang_aot/runtime/atoms.hpp>
#include <erlang_aot/runtime/process_context.hpp>

namespace erlang_aot::runtime {
TermResult<Term> Term::from_word(Word value) noexcept {
    const auto kind = classify_immediate(value);
    if (!kind) {
        return std::unexpected(kind.error());
    }
    if (*kind != TermKind::smallint && *kind != TermKind::empty_tuple && *kind != TermKind::empty_list) {
        return std::unexpected(TermError::not_implemented);
    }
    Term result;
    result.value_ = value;
    return result;
}

TermResult<Term> Term::from_word(Word value, ProcessContext &context) noexcept {
    return detail::TermAccess::admit(value, context.heap().storage_);
}

bool Term::is_atom() const { return static_cast<bool>(atom_); }

TermResult<AtomId> Term::atom_id() const {
    if (!atom_) {
        return std::unexpected(TermError::wrong_type);
    }
    return value_ >> 6;
}

TermResult<std::string> Term::atom_utf8() const {
    if (!atom_) {
        return std::unexpected(TermError::wrong_type);
    }
    try {
        return atom_->spelling;
    } catch (const std::bad_alloc &) {
        return std::unexpected(TermError::resource_limit);
    }
}

TermResult<bool> Term::boolean_value() const {
    if (!atom_) {
        return std::unexpected(TermError::wrong_type);
    }
    if (atom_->spelling == "true") {
        return true;
    }
    if (atom_->spelling == "false") {
        return false;
    }
    return std::unexpected(TermError::wrong_type);
}

bool Term::is_boolean() const { return boolean_value().has_value(); }

TermResult<std::string_view> Term::atom_spelling() const noexcept {
    if (!atom_) {
        return std::unexpected(TermError::wrong_type);
    }
    return atom_->spelling;
}

Word Term::word() const noexcept { return value_; }

TermKind Term::kind() const {
    if (heap_ && object_) {
        return heap_->alive() ? object_->kind : TermKind::invalid;
    }
    return TermTag{value_}.get_kind();
}

} // namespace erlang_aot::runtime
