#include "bitstrings.hpp"
#include "floats.hpp"
#include "terms.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <erlang_aot/runtime/process_context.hpp>

namespace erlang_aot::runtime::detail {
namespace {
// Grow packed staging storage only within the shared per-value work/bit ceiling.
TermResult<void> grow(BitWriter &writer, std::size_t count) {
    if (count > bit_limit - writer.length) {
        return std::unexpected(TermError::resource_limit);
    }
    writer.bytes.resize((writer.length + count + 7) / 8);
    return {};
}

// Set a logical output bit without exposing host byte order or retaining unused padding.
void put(BitWriter &writer, bool bit) {
    if (bit) {
        writer.bytes[writer.length / 8] |= static_cast<std::byte>(1U << (7 - writer.length % 8));
    }
    ++writer.length;
}

// Build a bounded power of two through the explicit bit API rather than shifting an inline limb.
Integer power(std::size_t bit) {
    Integer value = 0;
    boost::multiprecision::bit_set(value, static_cast<unsigned>(bit));
    return value;
}

// Convert extracted numeric bits only after width/endian validation has completed.
TermResult<Term> numeric_extract(ProcessContext &context, const BitView &view, std::size_t cursor, std::size_t width,
                                 const BitSegment &segment) {
    Integer bits = bit_integer({view.bytes, view.offset + cursor, width}, segment.little);
    if (segment.type == abi::v1::BitType::integer && segment.signed_value && width != 0 &&
        boost::multiprecision::bit_test(bits, static_cast<unsigned>(width - 1))) {
        bits = integer_sum(bits, power(width), true);
    }
    return segment.type == abi::v1::BitType::integer ? IntegerAccess::make(context.heap(), bits)
                                                     : bit_read_float(context, bits, width);
}

// Append a proved binary prefix without duplicating numeric or UTF conversion paths.
TermResult<void> binary_construct(BitWriter &writer, const BitSegment &segment) {
    const auto source = bit_view(segment.value);
    if (!source) {
        return std::unexpected(source.error());
    }
    const auto width = bit_width(segment, source->length);
    if (!width) {
        return std::unexpected(width.error());
    }
    if (*width > source->length) {
        return std::unexpected(TermError::invalid_argument);
    }
    return writer.append({source->bytes, source->offset, *width});
}

} // namespace

TermResult<void> BitWriter::append(const BitView &source) {
    const auto grown = grow(*this, source.length);
    if (!grown) {
        return grown;
    }
    for (std::size_t i = 0; i < source.length; ++i) {
        put(*this, bit_at(source, i));
    }
    return {};
}

TermResult<void> BitWriter::integer(const Integer &value, std::size_t count, bool little) {
    const auto grown = grow(*this, count);
    if (!grown) {
        return grown;
    }
    Integer bits = value;
    if (bits < 0) {
        bits = integer_sum(bits, power(std::max(count, integer_bits(bits) + 1)), false);
    }
    for (std::size_t i = 0; i < count; ++i) {
        const auto group = i / 8 * 8;
        const auto index = little ? group + std::min(std::size_t{8}, count - group) - 1 - i % 8 : count - 1 - i;
        put(*this, boost::multiprecision::bit_test(bits, static_cast<unsigned>(index)));
    }
    return {};
}

Integer bit_integer(const BitView &view, bool little) {
    const auto count = view.length;
    Integer result = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (bit_at(view, i)) {
            const auto group = i / 8 * 8;
            const auto index = little ? group + std::min(std::size_t{8}, count - group) - 1 - i % 8 : count - 1 - i;
            boost::multiprecision::bit_set(result, static_cast<unsigned>(index));
        }
    }
    return result;
}

TermResult<std::size_t> bit_width(const BitSegment &segment, std::size_t remaining) {
    if (segment.all) {
        if (remaining % segment.unit != 0) {
            return std::unexpected(TermError::invalid_argument);
        }
        return remaining;
    }
    const auto size = integer_read(segment.size);
    if (!size) {
        return std::unexpected(size.error());
    }
    if (*size < 0) {
        return std::unexpected(TermError::invalid_argument);
    }
    if (*size > std::numeric_limits<std::size_t>::max() / segment.unit) {
        return std::unexpected(TermError::out_of_range);
    }
    return size->convert_to<std::size_t>() * segment.unit;
}

TermResult<void> bit_construct(BitWriter &writer, const BitSegment &segment) {
    using Type = abi::v1::BitType;
    if (segment.empty) {
        if (segment.type >= Type::utf8) {
            return {};
        }
        return bit_width(segment, 0).transform([](std::size_t) {});
    }
    if (segment.type >= Type::utf8) {
        return bit_utf_construct(writer, segment);
    }
    if (segment.type == Type::binary) {
        return binary_construct(writer, segment);
    }
    const auto width = bit_width(segment, 0);
    if (!width) {
        return std::unexpected(width.error());
    }
    const auto bits =
        segment.type == Type::integer ? integer_read(segment.value) : bit_float_bits(segment.value, *width);
    if (!bits) {
        return std::unexpected(bits.error());
    }
    return writer.integer(*bits, *width, segment.little);
}

TermResult<BitExtract> bit_extract(ProcessContext &context, const Term &source, std::size_t cursor,
                                   const BitSegment &segment) {
    const auto view = bit_view(source);
    if (!view) {
        return std::unexpected(view.error());
    }
    if (cursor > view->length) {
        return std::unexpected(TermError::out_of_range);
    }
    using Type = abi::v1::BitType;
    if (segment.type >= Type::utf8) {
        return bit_utf_extract(context, *view, cursor, segment);
    }
    const auto width = bit_width(segment, view->length - cursor);
    if (!width) {
        return std::unexpected(width.error());
    }
    if (*width > view->length - cursor) {
        return std::unexpected(TermError::out_of_range);
    }
    if (segment.type == Type::binary) {
        return BitAccess::slice(context.heap(), source, cursor, *width).transform([&](Term value) {
            return BitExtract{std::move(value), cursor + *width};
        });
    }
    const auto result = numeric_extract(context, *view, cursor, *width, segment);
    return result.transform([&](Term value) { return BitExtract{std::move(value), cursor + *width}; });
}
} // namespace erlang_aot::runtime::detail
