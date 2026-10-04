#include "../memory/heap_storage.hpp"
#include "term_layout.hpp"
#include "terms.hpp"
#include <erlang_aot/abi/equality.hpp>
#include <erlang_aot/abi/term.hpp>
#include <new>
#include <stdexcept>

namespace erlang_aot::runtime {
namespace {
// Bound constructor work independently of backing limits and validate every child before reservation.
TermResult<void> validate(ProcessHeap &heap, std::span<const Term> values) {
    if (values.size() > 1'000'000) {
        return std::unexpected(TermError::resource_limit);
    }
    for (const auto &value : values) {
        if (const auto copied = heap.add(value); !copied) {
            return std::unexpected(copied.error());
        }
    }
    return {};
}

// Keep allocation failures distinct from the configured backing ceiling.
TermError heap_error(HeapError error) {
    return error == HeapError::out_of_memory ? TermError::out_of_memory : TermError::resource_limit;
}

// Publish the initialized tuple header and fields as a single immutable object.
TermResult<Term> tuple(ProcessHeap &heap, const std::shared_ptr<detail::HeapStorage> &storage,
                       std::span<const Term> elements) {
    auto reserved = heap.reserve(elements.size() + 1);
    if (!reserved) {
        return std::unexpected(heap_error(reserved.error()));
    }
    auto *words = ::new (reserved->bytes().data()) Word[elements.size() + 1]{};
    words[0] = detail::layout::BoxHeader::make(BoxedKind::tuple, elements.size());
    for (std::size_t i = 0; i < elements.size(); ++i) {
        words[i + 1] = elements[i].word();
    }
    const auto value = reinterpret_cast<Word>(words) | static_cast<Word>(TermKindPrimary::boxed);
    const std::array objects{detail::HeapObject{value, TermKind::tuple, {words, elements.size() + 1}, elements.size()}};
    return detail::publish(storage, *reserved, objects);
}

// Stage one complete cons spine and every owned start before committing any of it.
TermResult<Term> list(ProcessHeap &heap, const std::shared_ptr<detail::HeapStorage> &storage,
                      std::span<const Term> elements, const Term &tail) {
    std::vector<detail::HeapObject> objects;
    objects.reserve(elements.size());
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
        objects.push_back(
            {reinterpret_cast<Word>(cell) | static_cast<Word>(TermKindPrimary::list), TermKind::list, {cell, 2}, 2});
    }
    return detail::publish(storage, *reserved, objects);
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
    if (const auto checked = validate(**owner, elements); !checked) {
        return std::unexpected(checked.error());
    }
    if (elements.empty()) {
        return Term::from_word(abi::v1::empty_tuple);
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
    const auto checked_tail = (*owner)->add(tail);
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
