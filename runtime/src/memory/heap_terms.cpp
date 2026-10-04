#include "heap_object.hpp"
#include "heap_storage.hpp"
#include "heap_walk.hpp"
#include <erlang_aot/runtime/atoms.hpp>

namespace erlang_aot::runtime::detail {
namespace {
using layout::BoxHeader;

// Strip the primary tag; the mask is narrower than a 64-bit Word, so widen it first.
std::uintptr_t address(Word value) {
    return static_cast<std::uintptr_t>(value & ~static_cast<Word>(abi::v1::primary_mask));
}

// The object a process pointer names must have the shape its tag claims and a well-formed header.
bool shaped(TermKind tag, std::span<const Word> area) {
    const auto cell = parse_cell(area);
    return cell && cell->shape == (tag == TermKind::list ? HeapCell::Shape::cons : HeapCell::Shape::boxed);
}

// Map an admitted header to its term kind and logical element count.
HeapObject boxed(Word value, std::span<const Word> words) {
    const auto payload = words.subspan(1);
    switch (BoxHeader::kind(words[0])) {
    case BoxedKind::tuple:
        return {value, TermKind::tuple, words, payload.size()};
    case BoxedKind::map:
        return {value, TermKind::map, words, payload.size() / 2};
    case BoxedKind::bignum:
        return {value, TermKind::bignum, words, payload.size() - 1};
    case BoxedKind::floating:
        return {value, TermKind::floating, words, 1};
    case BoxedKind::heap_binary:
        return {value, TermKind::bitstring, words, payload[0]};
    case BoxedKind::refc_binary:
        return {value, TermKind::bitstring, words, payload[1]};
    default:
        return {value, TermKind::invalid, words, 0};
    }
}

// Decode an admitted word from the words at its object start; admission proved the shape and header.
HeapObject decode(Word value, std::span<const Word> area) {
    if (area.empty()) {
        return {value, TermKind::invalid, {}, 0};
    }
    if (TermTag{value}.get_kind() == TermKind::list) {
        return {value, TermKind::list, area.first(2), 2};
    }
    return boxed(value, area.first(1 + BoxHeader::count(area[0])));
}
} // namespace

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
    const auto area = storage->owned(address(value));
    if (area.empty() || !shaped(kind, area)) {
        return std::unexpected(TermError::wrong_owner);
    }
    Term result;
    result.value_ = value;
    result.heap_ = storage;
    return result;
}

TermResult<HeapObject> TermAccess::object(const Term &value) noexcept {
    if (!value.heap_) {
        return std::unexpected(TermError::wrong_type);
    }
    if (!value.heap_->alive()) {
        return std::unexpected(TermError::expired_context);
    }
    return decode(value.value_, value.heap_->owned(address(value.value_)));
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
        return object(value).transform([](const HeapObject &) {});
    }
    if (value.is_atom()) {
        return {};
    }
    return Term::from_word(value.word()).transform([](const Term &) {});
}
} // namespace erlang_aot::runtime::detail
