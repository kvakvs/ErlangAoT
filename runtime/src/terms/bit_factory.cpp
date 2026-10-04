#include "../memory/heap_storage.hpp"
#include "bitstrings.hpp"
#include "term_layout.hpp"
#include "terms.hpp"
#include <algorithm>
#include <new>

namespace erlang_aot::runtime::detail {
namespace {
// Pair the shared-buffer cell's C++ lifetime with its containing process reservation.
void destroy(std::byte *bytes) noexcept { std::destroy_at(reinterpret_cast<BitCell *>(bytes)); }
} // namespace

TermResult<Term> BitAccess::publish(ProcessHeap &heap, BitCell cell, bool charge_backing) {
    constexpr auto count = sizeof(BitCell) / sizeof(Word);
    static_assert(sizeof(BitCell) % sizeof(Word) == 0);
    const auto charged = charge_backing && cell.shared ? (cell.shared->size() + sizeof(Word) - 1) / sizeof(Word) : 0;
    auto reserved = heap.reserve(count + charged);
    if (!reserved) {
        return std::unexpected(reserved.error() == HeapError::out_of_memory ? TermError::out_of_memory
                                                                            : TermError::resource_limit);
    }
    // MSVC reports C4554 for a subtraction inside the shifted cast; name the content size instead.
    const auto content = static_cast<Word>(count - 1);
    cell.header = (content << layout::BoxHeader::CONTENT_SHIFT) |
                  (static_cast<Word>(cell.shared ? BoxedKind::refc_binary : BoxedKind::heap_binary) << 2);
    auto *stored = std::construct_at(reinterpret_cast<BitCell *>(reserved->bytes().data()), std::move(cell));
    const auto encoded = reinterpret_cast<Word>(stored) | static_cast<Word>(TermKindPrimary::boxed);
    const std::array objects{HeapObject{encoded, TermKind::bitstring, {&stored->header, 1}, stored->length}};
    return detail::publish(heap.storage_, *reserved, objects, destroy);
}

TermResult<Term> BitAccess::make(ProcessHeap &heap, std::span<const std::byte> bytes, std::size_t count) {
    if (count > bit_limit) {
        return std::unexpected(TermError::resource_limit);
    }
    const auto size = (count + 7) / 8;
    if (size > bytes.size()) {
        return std::unexpected(TermError::invalid_argument);
    }
    try {
        BitCell cell{0, 0, count, {}, {}};
        if (size <= cell.small.size()) {
            std::ranges::copy(bytes.first(size), cell.small.begin());
            if (count % 8 != 0) {
                cell.small[size - 1] &= static_cast<std::byte>(0xffU << (8 - count % 8));
            }
        } else {
            auto buffer = std::make_shared<std::vector<std::byte>>(bytes.begin(),
                                                                   bytes.begin() + static_cast<std::ptrdiff_t>(size));
            if (count % 8 != 0) {
                buffer->back() &= static_cast<std::byte>(0xffU << (8 - count % 8));
            }
            cell.shared = std::move(buffer);
        }
        return publish(heap, std::move(cell), true);
    } catch (const std::bad_alloc &) {
        return std::unexpected(TermError::out_of_memory);
    }
}

TermResult<Term> BitAccess::slice(ProcessHeap &heap, const Term &source, std::size_t offset, std::size_t count) {
    const auto owned = heap.add(source);
    if (!owned) {
        return std::unexpected(owned.error());
    }
    const auto view = bit_view(source);
    if (!view) {
        return std::unexpected(view.error());
    }
    if (offset > view->length || count > view->length - offset) {
        return std::unexpected(TermError::out_of_range);
    }
    const auto object = TermAccess::object(source).value();
    const auto &original = *reinterpret_cast<const BitCell *>(object->words.data());
    if (original.shared) {
        return publish(heap, {0, original.offset + offset, count, original.shared, {}});
    }
    BitWriter writer;
    const auto copied = writer.append({view->bytes, view->offset + offset, count});
    if (!copied) {
        return std::unexpected(copied.error());
    }
    return make(heap, writer.bytes, count);
}
} // namespace erlang_aot::runtime::detail

namespace erlang_aot::runtime {
TermResult<Term> TermFactory::binary(std::span<const std::byte> bytes) {
    if (bytes.size() > detail::bit_limit / 8) {
        return std::unexpected(TermError::resource_limit);
    }
    return bitstring(bytes, bytes.size() * 8);
}

TermResult<Term> TermFactory::bitstring(std::span<const std::byte> bytes, std::size_t count) {
    return heap().and_then([&](ProcessHeap *owner) { return detail::BitAccess::make(*owner, bytes, count); });
}
} // namespace erlang_aot::runtime
