#include "../memory/heap_storage.hpp"
#include "term_layout.hpp"
#include "terms.hpp"
#include <erlang_aot/abi/equality.hpp>
#include <erlang_aot/abi/term.hpp>
#include <new>
#include <stdexcept>

namespace erlang_aot::runtime {
namespace {
// Validate every child before reservation; lists have no length cap beyond memory.
TermResult<void> validate(ProcessHeap &heap, std::span<const Term> values) {
    for (const auto &value : values) {
        if (const auto owned = heap.retain(value); !owned) {
            return std::unexpected(owned.error());
        }
    }
    return {};
}

// Keep allocation failures distinct from the configured backing ceiling.
TermError heap_error(HeapError error) {
    return error == HeapError::out_of_memory ? TermError::out_of_memory : TermError::resource_limit;
}

// Every tuple arity fits the header's word count on 32-bit targets too.
static_assert(MAX_TUPLE_ARITY <= static_cast<Word>(~Word{0}) >> detail::layout::BoxHeader::CONTENT_SHIFT);

// The ABI word of a tuple field given as a Term or as a word.
Word word_of(const Term &value) { return value.word(); }

Word word_of(Word value) { return value; }

// Publish the initialized tuple header and fields of validated elements as a single immutable object.
template <typename Element>
TermResult<Term> tuple(ProcessHeap &heap, const std::shared_ptr<detail::HeapStorage> &storage,
                       std::span<const Element> elements) {
    if (elements.empty()) {
        return Term::from_word(abi::v1::empty_tuple);
    }
    auto reserved = heap.reserve(elements.size() + 1);
    if (!reserved) {
        return std::unexpected(heap_error(reserved.error()));
    }
    auto *words = ::new (reserved->bytes().data()) Word[elements.size() + 1]{};
    words[0] = detail::layout::BoxHeader::make(BoxedKind::tuple, elements.size());
    for (std::size_t i = 0; i < elements.size(); ++i) {
        words[i + 1] = word_of(elements[i]);
    }
    return detail::publish(storage, *reserved,
                           reinterpret_cast<Word>(words) | static_cast<Word>(TermKindPrimary::boxed));
}

// Stage one complete cons spine before committing any of it; publication marks every cell start.
TermResult<Term> list(ProcessHeap &heap, const std::shared_ptr<detail::HeapStorage> &storage,
                      std::span<const Term> elements, const Term &tail) {
    auto reserved = heap.reserve(elements.size() * 2);
    if (!reserved) {
        return std::unexpected(heap_error(reserved.error()));
    }
    auto *words = ::new (reserved->bytes().data()) Word[elements.size() * 2]{};
    for (std::size_t i = 0; i < elements.size(); ++i) {
        auto *cell = words + i * 2;
        cell[0] = elements[i].word();
        cell[1] = i + 1 == elements.size()
                      ? tail.word()
                      : reinterpret_cast<Word>(cell + 2) | static_cast<Word>(TermKindPrimary::list);
    }
    return detail::publish(storage, *reserved,
                           reinterpret_cast<Word>(words) | static_cast<Word>(TermKindPrimary::list));
}
} // namespace

TermResult<ProcessHeap *> TermFactory::heap() const noexcept {
    const auto lifetime = lifetime_.lock();
    if (!lifetime || !lifetime->alive()) {
        return std::unexpected(TermError::expired_context);
    }
    return heap_;
}

TermResult<Term> TermFactory::nil() {
    return heap().and_then([](ProcessHeap *) { return Term::from_word(abi::v1::empty_list); });
}

TermResult<Term> TermFactory::tuple(std::span<const Term> elements) {
    const auto owner = heap();
    if (!owner) {
        return std::unexpected(owner.error());
    }
    if (elements.size() > MAX_TUPLE_ARITY) {
        return std::unexpected(TermError::resource_limit);
    }
    if (const auto checked = validate(**owner, elements); !checked) {
        return std::unexpected(checked.error());
    }
    return runtime::tuple(**owner, (*owner)->storage_, elements);
}

TermResult<Term> TermFactory::tuple_words(std::span<const Word> elements) {
    const auto owner = heap();
    if (!owner) {
        return std::unexpected(owner.error());
    }
    if (elements.size() > MAX_TUPLE_ARITY) {
        return std::unexpected(TermError::resource_limit);
    }
    for (const auto element : elements) {
        if (const auto admitted = Term::from_word(element, (*owner)->owner_); !admitted) {
            return std::unexpected(admitted.error());
        }
    }
    return runtime::tuple(**owner, (*owner)->storage_, elements);
}

TermResult<Term> TermFactory::list_tail(std::span<const Term> elements, const Term &tail) {
    const auto owner = heap();
    if (!owner) {
        return std::unexpected(owner.error());
    }
    if (const auto checked = validate(**owner, elements); !checked) {
        return std::unexpected(checked.error());
    }
    const auto checked_tail = (*owner)->retain(tail);
    if (!checked_tail || elements.empty()) {
        return checked_tail;
    }
    try {
        return runtime::list(**owner, (*owner)->storage_, elements, *checked_tail);
    } catch (const std::bad_alloc &) {
        return std::unexpected(TermError::out_of_memory);
    } catch (const std::length_error &) {
        return std::unexpected(TermError::resource_limit);
    }
}

TermResult<Term> TermFactory::cons(const Term &head, const Term &tail) { return list_tail(std::array{head}, tail); }

TermResult<Term> TermFactory::list(std::span<const Term> elements) {
    return list_tail(elements, Term::from_word(abi::v1::empty_list).value());
}

TermResult<Term> TermFactory::list(std::span<const Term> elements, const Term &tail) {
    return list_tail(elements, tail);
}
} // namespace erlang_aot::runtime
