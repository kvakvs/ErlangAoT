#pragma once
#include <erlang_aot/runtime/terms.hpp>

namespace erlang_aot::runtime::detail {
class HeapStorage;

struct HeapObject {
    // Index only fully constructed starts; exact tagged identity precedes any cell dereference.
    Word value;
    TermKind kind;
    // Borrow stable initialized words; count denotes logical tuple arity or cons field count.
    std::span<const Word> words;
    std::size_t count;
};

struct TermAccess {
    // Admit immediates/atoms or an exact published object belonging to this live storage owner.
    static TermResult<Term> admit(Word value, const std::shared_ptr<HeapStorage> &storage) noexcept;
    // Check host lifetime before extracting any resource or child pointer.
    static TermResult<const HeapObject *> object(const Term &value) noexcept;
    // Retain the parent's backing when resolving a checked child slot.
    static TermResult<Term> child(const Term &parent, Word value) noexcept;
    // Preserve expiration distinctly from unsupported cross-heap copying.
    static TermResult<void> validate(const Term &value) noexcept;
};
} // namespace erlang_aot::runtime::detail
