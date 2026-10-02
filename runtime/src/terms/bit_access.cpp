#include "../memory/heap_object.hpp"
#include "bitstrings.hpp"
#include <algorithm>
#include <new>

namespace erlang_aot::runtime::detail {
TermResult<BitView> bit_view(const Term &term) {
    const auto object = TermAccess::object(term);
    if (!object) {
        return std::unexpected(object.error());
    }
    if ((*object)->kind != TermKind::bitstring) {
        return std::unexpected(TermError::wrong_type);
    }
    const auto &cell = *reinterpret_cast<const BitCell *>((*object)->words.data());
    return BitView{cell.shared ? std::span<const std::byte>(*cell.shared) : cell.small, cell.offset, cell.length};
}

bool bit_at(const BitView &view, std::size_t index) noexcept {
    const auto position = view.offset + index;
    return (std::to_integer<unsigned>(view.bytes[position / 8]) & (1U << (7 - position % 8))) != 0;
}

TermResult<int> bit_order(const Term &left, const Term &right, std::size_t &budget) {
    const auto lhs = bit_view(left).value();
    const auto rhs = bit_view(right).value();
    const auto count = std::min(lhs.length, rhs.length);
    for (std::size_t i = 0; i < count; ++i) {
        if (budget == 0) {
            return std::unexpected(TermError::resource_limit);
        }
        --budget;
        const bool a = bit_at(lhs, i);
        const bool b = bit_at(rhs, i);
        if (a != b) {
            return a ? 1 : -1;
        }
    }
    return static_cast<int>(lhs.length > rhs.length) - static_cast<int>(lhs.length < rhs.length);
}
} // namespace erlang_aot::runtime::detail

namespace erlang_aot::runtime {
bool Term::is_bitstring() const { return kind() == TermKind::bitstring; }

bool Term::is_binary() const {
    return bit_size().transform([](std::size_t count) { return count % 8 == 0; }).value_or(false);
}

TermResult<std::size_t> Term::bit_size() const {
    return detail::bit_view(*this).transform([](const detail::BitView &view) { return view.length; });
}

TermResult<std::vector<std::byte>> Term::bitstring_bytes() const {
    const auto view = detail::bit_view(*this);
    if (!view) {
        return std::unexpected(view.error());
    }
    try {
        detail::BitWriter writer;
        const auto copied = writer.append(*view);
        if (!copied) {
            return std::unexpected(copied.error());
        }
        return std::move(writer.bytes);
    } catch (const std::bad_alloc &) {
        return std::unexpected(TermError::out_of_memory);
    }
}

TermResult<std::vector<std::byte>> Term::binary_bytes() const {
    const auto count = bit_size();
    if (!count) {
        return std::unexpected(count.error());
    }
    return *count % 8 == 0 ? bitstring_bytes()
                           : TermResult<std::vector<std::byte>>(std::unexpected(TermError::wrong_type));
}
} // namespace erlang_aot::runtime
