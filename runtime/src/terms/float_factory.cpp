#include "../memory/heap_storage.hpp"
#include "floats.hpp"
#include "term_layout.hpp"
#include "terms.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <new>

namespace erlang_aot::runtime::detail {
TermResult<Term> FloatAccess::make(ProcessHeap &heap, double value) {
    static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);
    if (!std::isfinite(value)) {
        return std::unexpected(TermError::invalid_argument);
    }
    constexpr auto count = 1 + sizeof(double) / sizeof(Word);
    auto reserved = heap.reserve(count);
    if (!reserved) {
        return std::unexpected(reserved.error() == HeapError::out_of_memory ? TermError::out_of_memory
                                                                            : TermError::resource_limit);
    }
    auto *words = ::new (reserved->bytes().data()) Word[count]{};
    words[0] = (static_cast<Word>(count - 1) << layout::BoxHeader::CONTENT_SHIFT) |
               (static_cast<Word>(BoxedKind::floating) << 2);
    std::memcpy(words + 1, &value, sizeof(value));
    const auto encoded = reinterpret_cast<Word>(words) | static_cast<Word>(TermKindPrimary::boxed);
    const std::array objects{HeapObject{encoded, TermKind::floating, {words, count}, 1}};
    return publish(heap.storage_, *reserved, objects);
}
} // namespace erlang_aot::runtime::detail

namespace erlang_aot::runtime {
bool Term::is_float() const { return kind() == TermKind::floating; }

TermResult<double> Term::float_value() const {
    const auto object = detail::TermAccess::object(*this);
    if (!object) {
        return std::unexpected(object.error());
    }
    if ((*object)->kind != TermKind::floating) {
        return std::unexpected(TermError::wrong_type);
    }
    double value = 0;
    std::memcpy(&value, (*object)->words.data() + 1, sizeof(value));
    return value;
}

TermResult<Term> TermFactory::floating(double value) {
    const auto owner = heap();
    if (!owner) {
        return std::unexpected(owner.error());
    }
    try {
        return detail::FloatAccess::make(**owner, value);
    } catch (const std::bad_alloc &) {
        return std::unexpected(TermError::out_of_memory);
    }
}
} // namespace erlang_aot::runtime
