#include "expression.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <map>

namespace erlang_aot {
namespace {
struct Segment {
    // Default binary syntax encodes an integer in eight big-endian bits.
    std::u32string type = U"integer";
    std::size_t unit = 1;
    bool explicit_unit = false;
    bool little = false;
};

// Consume unit:N with OTP's positive, at-most-256 unit range.
void set_unit(Segment &result, std::span<const Token> tokens, std::size_t &position) {
    if (tokens[position].text() != U"unit" || ++position == tokens.size()) {
        throw EvaluationFailure();
    }
    const auto *value = std::get_if<Integer>(&tokens[position].value);
    if (!value) {
        throw EvaluationFailure();
    }
    const auto unit = index(literal_value(tokens[position]), 256);
    if (unit == 0 || (result.explicit_unit && result.unit != unit)) {
        throw EvaluationFailure();
    }
    result.unit = unit;
    result.explicit_unit = true;
}

// Apply one modifier and reject conflicting specifications before allocating binary data.
void set_modifier(std::optional<std::u32string> &slot, std::u32string_view name) {
    if (slot && *slot != name) {
        throw EvaluationFailure();
    }
    slot = name;
}

// Decode modifier categories independently from defaults and compatibility checks.
void read_modifier(Segment &result, std::array<std::optional<std::u32string>, 3> &seen, std::span<const Token> tokens,
                   std::size_t &position) {
    auto name = tokens[position].text();
    if (name == U"bytes") {
        name = U"binary";
    }
    if (name == U"bits") {
        name = U"bitstring";
    }
    static const std::map<std::u32string_view, std::size_t> category{
        {U"integer", 0}, {U"float", 0},  {U"binary", 0}, {U"bitstring", 0}, {U"utf8", 0},   {U"utf16", 0},
        {U"utf32", 0},   {U"little", 1}, {U"big", 1},    {U"native", 1},    {U"signed", 2}, {U"unsigned", 2}};
    if (const auto found = category.find(name); found != category.end()) {
        set_modifier(seen[found->second], name);
    } else {
        set_unit(result, tokens, position);
    }
}

Segment modifiers(const Expr &expression) {
    Segment result;
    std::array<std::optional<std::u32string>, 3> seen;
    for (std::size_t i = 0; i < expression.modifiers.size(); ++i) {
        read_modifier(result, seen, expression.modifiers, i);
    }
    result.type = seen[0].value_or(U"integer");
    result.little = seen[1] == U"little" || (seen[1] == U"native" && std::endian::native == std::endian::little);
    if (!result.explicit_unit && result.type == U"binary") {
        result.unit = 8;
    }
    if (result.type.starts_with(U"utf") && (result.explicit_unit || expression.children.size() == 2)) {
        throw EvaluationFailure();
    }
    return result;
}

// Round a finite double to IEEE binary16, including subnormals and signed zero.
std::uint16_t half(double number) {
    const auto sign = static_cast<std::uint16_t>(std::signbit(number) ? 0x8000 : 0);
    const auto magnitude = std::abs(number);
    if (magnitude >= 65520) {
        return static_cast<std::uint16_t>(sign | 0x7c00);
    }
    if (magnitude < std::ldexp(1.0, -14)) {
        return static_cast<std::uint16_t>(sign | static_cast<std::uint16_t>(std::nearbyint(std::ldexp(magnitude, 24))));
    }
    int exponent = 0;
    std::frexp(magnitude, &exponent);
    const auto mantissa = static_cast<std::uint16_t>(std::nearbyint(std::ldexp(magnitude, 11 - exponent)));
    return static_cast<std::uint16_t>(sign | static_cast<std::uint16_t>((exponent + 13) * 1024 + mantissa));
}

// Encode low-order integer bits; little-endian swaps octets while preserving partial octets.
void append_integer(Value &output, const BigInt &number, std::size_t count, bool little) {
    if (count > 1000000 - output.bits.size()) {
        throw EvaluationLimit();
    }
    std::vector<bool> bits;
    bits.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        bits.push_back(static_cast<bool>((number >> (count - i - 1)) & 1));
    }
    if (!little) {
        output.bits.insert(output.bits.end(), bits.begin(), bits.end());
        return;
    }
    while (count > 0) {
        const auto chunk = std::min<std::size_t>(count, 8);
        output.bits.insert(output.bits.end(), bits.begin() + static_cast<std::ptrdiff_t>(count - chunk),
                           bits.begin() + static_cast<std::ptrdiff_t>(count));
        count -= chunk;
    }
}

void append_unicode(Value &output, const Value &value, const Segment &segment) {
    const auto character = index(value, 0x10ffff);
    if (character >= 0xd800 && character <= 0xdfff) {
        throw EvaluationFailure();
    }
    if (segment.type == U"utf8") {
        for (const unsigned char byte : utf8(std::u32string(1, static_cast<char32_t>(character)))) {
            append_integer(output, byte, 8, false);
        }
    } else if (segment.type == U"utf32") {
        append_integer(output, character, 32, segment.little);
    } else if (character < 0x10000) {
        append_integer(output, character, 16, segment.little);
    } else {
        append_integer(output, 0xd800 + ((character - 0x10000) >> 10), 16, segment.little);
        append_integer(output, 0xdc00 + ((character - 0x10000) & 0x3ff), 16, segment.little);
    }
}

// Floating segments share IEEE encodings across target hosts.
void append_float(Value &output, const Value &value, const Segment &segment, std::size_t count) {
    const auto number = real(value);
    if (count == 16) {
        append_integer(output, half(number), count, segment.little);
    } else if (count == 64) {
        append_integer(output, std::bit_cast<std::uint64_t>(number), count, segment.little);
    } else if (count == 32) {
        append_integer(output, std::bit_cast<std::uint32_t>(static_cast<float>(number)), count, segment.little);
    } else {
        throw EvaluationFailure();
    }
}

void append_value(Value &output, const Value &value, const Segment &segment, std::size_t count) {
    if (segment.type.starts_with(U"utf")) {
        append_unicode(output, value, segment);
        return;
    }
    if (segment.type == U"integer") {
        append_integer(output, integral(value), count, segment.little);
        return;
    }
    if (segment.type == U"float") {
        append_float(output, value, segment, count);
        return;
    }
    if (value.kind != ValueKind::bits || count > value.bits.size() || count % segment.unit != 0) {
        throw EvaluationFailure();
    }
    if (count > 1000000 - output.bits.size()) {
        throw EvaluationLimit();
    }
    output.bits.insert(output.bits.end(), value.bits.begin(), value.bits.begin() + static_cast<std::ptrdiff_t>(count));
}

void append_segment(Value &result, const Expr &expression, const std::function<bool(std::u32string_view)> &defined) {
    const auto value = evaluate(expression.children.front(), defined);
    std::optional<Value> size;
    if (expression.children.size() == 2) {
        size = evaluate(expression.children[1], defined);
    }
    append_literal_bits(result, value, size, expression.modifiers,
                        expression.children.front().token.kind == TokenKind::string);
}
} // namespace

Value evaluate_bits(const Expr &expression, const std::function<bool(std::u32string_view)> &defined) {
    Value result;
    result.kind = ValueKind::bits;
    for (const auto &segment : expression.children) {
        append_segment(result, segment, defined);
    }
    return result;
}

void append_literal_bits(Value &output, const Value &value, const std::optional<Value> &size,
                         std::span<const Token> tokens, bool string) {
    Expr descriptor{ExprKind::segment, {}, {}, {tokens.begin(), tokens.end()}};
    descriptor.children.resize(size ? 2 : 1);
    const auto settings = modifiers(descriptor);
    auto count = settings.type == U"float" ? std::size_t{64} : std::size_t{8};
    if (value.kind == ValueKind::bits) {
        count = value.bits.size();
    }
    if (size) {
        if (integral(*size) > 1000000 / settings.unit) {
            throw EvaluationLimit();
        }
        count = index(*size) * settings.unit;
    }
    if (string) {
        for (const auto &character : value.elements) {
            append_value(output, character, settings, count);
        }
    } else {
        append_value(output, value, settings, count);
    }
}
} // namespace erlang_aot
