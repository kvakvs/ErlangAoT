#include "../memory/heap_storage.hpp"
#include "integers.hpp"
#include "term_layout.hpp"
#include "terms.hpp"
#include <erlang_aot/abi/term.hpp>
#include <new>
#include <stdexcept>

namespace erlang_aot::runtime::detail {
TermResult<Term> IntegerAccess::make(ProcessHeap &heap, const Integer &value) {
    const auto bits = integer_bits(value);
    if (bits > integer_bit_limit) {
        return std::unexpected(TermError::resource_limit);
    }
    if (value >= abi::v1::NativeIntegerEncoding::minimum && value <= abi::v1::NativeIntegerEncoding::maximum) {
        return Term::from_word(encode_integer(value.convert_to<std::int64_t>()).value());
    }
    const auto count = (bits + sizeof(Word) * 8 - 1) / (sizeof(Word) * 8);
    auto reserved = heap.reserve(count + 2);
    if (!reserved) {
        return std::unexpected(reserved.error() == HeapError::out_of_memory ? TermError::out_of_memory
                                                                            : TermError::resource_limit);
    }
    auto *words = ::new (reserved->bytes().data()) Word[count + 2]{};
    words[0] = layout::BoxHeader::make(BoxedKind::bignum, count + 1);
    words[1] = static_cast<Word>(value < 0);
    const Integer magnitude = value < 0 ? -value : value;
    boost::multiprecision::export_bits(magnitude, words + 2, sizeof(Word) * 8, false);
    return publish(heap.storage_, *reserved, reinterpret_cast<Word>(words) | static_cast<Word>(TermKindPrimary::boxed));
}
} // namespace erlang_aot::runtime::detail

namespace erlang_aot::runtime {
TermResult<Term> TermFactory::integer(std::int64_t value) {
    const auto owner = heap();
    if (!owner) {
        return std::unexpected(owner.error());
    }
    try {
        return detail::IntegerAccess::make(**owner, detail::Integer(value));
    } catch (const std::bad_alloc &) {
        return std::unexpected(TermError::out_of_memory);
    } catch (const std::length_error &) {
        return std::unexpected(TermError::resource_limit);
    }
}

TermResult<Term> TermFactory::integer_decimal(std::string_view value) {
    const auto owner = heap();
    if (!owner) {
        return std::unexpected(owner.error());
    }
    try {
        return detail::integer_parse(value).and_then(
            [&](const auto &integer) { return detail::IntegerAccess::make(**owner, integer); });
    } catch (const std::bad_alloc &) {
        return std::unexpected(TermError::out_of_memory);
    } catch (const std::length_error &) {
        return std::unexpected(TermError::resource_limit);
    }
}
} // namespace erlang_aot::runtime
