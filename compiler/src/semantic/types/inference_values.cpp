#include "inference_values.hpp"
#include "../binary_options.hpp"
#include "../capabilities.hpp"
#include "lattice.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <numeric>
#include <set>

namespace clause::semantic::types {
namespace {
using BitType = abi::v1::BitType;

// The largest bitstring size inference counts exactly; a larger one is any bitstring().
constexpr std::uint64_t SIZE_LIMIT = std::uint64_t{1} << 40;

// A bitstring of `base` bits plus any multiple of `unit` bits (unit 0: exactly `base` bits).
struct Bits {
    std::uint64_t base = 0;
    std::uint64_t unit = 0;
};

// Two bitstrings one after the other; none once a size passes SIZE_LIMIT.
std::optional<Bits> append(const std::optional<Bits> left, const std::optional<Bits> right) {
    if (!left || !right || left->base + right->base > SIZE_LIMIT) {
        return std::nullopt;
    }
    return Bits{left->base + right->base, std::gcd(left->unit, right->unit)};
}

// `count` copies of a bitstring; none once a size passes SIZE_LIMIT.
std::optional<Bits> repeat(const std::optional<Bits> bits, const std::uint64_t count) {
    if (!bits || (count != 0 && bits->base > SIZE_LIMIT / count)) {
        return std::nullopt;
    }
    return count == 0 ? Bits{} : Bits{bits->base * count, bits->unit};
}

// A canonical decimal as a size within SIZE_LIMIT.
std::optional<std::uint64_t> size_value(const std::string_view decimal) {
    std::uint64_t value = 0;
    const auto parsed = std::from_chars(decimal.data(), decimal.data() + decimal.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != decimal.data() + decimal.size() || value > SIZE_LIMIT) {
        return std::nullopt;
    }
    return value;
}

// The bits of one character encoded as UTF-8, UTF-16 or UTF-32.
std::uint64_t encoded_bits(const char32_t code, const BitType type) {
    // The first code each further UTF-8 byte starts at.
    static constexpr std::array<char32_t, 3> UTF8_STEPS{0x80, 0x800, 0x10000};
    if (type == BitType::utf8) {
        return 8 * (1 + std::ranges::count_if(UTF8_STEPS, [&](const char32_t step) { return code >= step; }));
    }
    return type == BitType::utf16 && code < 0x10000 ? 16 : 32;
}

// The sizes a UTF segment of an unknown character can have: 8 to 32 bits by bytes, 16 or 32 bits, or 32 bits.
Bits utf_bits(const BitType type) {
    if (type == BitType::utf8) {
        return {8, 8};
    }
    return type == BitType::utf16 ? Bits{16, 16} : Bits{32, 0};
}

// The sizes of a bitstring fact, or any multiple of `unit` bits for another fact.
Bits value_bits(const Graph &graph, const Id fact, const std::uint64_t unit) {
    const auto &node = graph.get(fact);
    if (node.kind == Kind::bitstring) {
        Bits result;
        for (std::size_t index = 0; index < node.children.size() && index < node.labels.size(); ++index) {
            const auto size = size_value(graph.get(node.children[index]).name).value_or(0);
            (node.labels[index] == "unit" ? result.unit : result.base) = size;
        }
        return result;
    }
    static const std::map<std::string_view, Bits> NAMED{
        {"binary", {0, 8}}, {"nonempty_binary", {8, 8}}, {"bitstring", {0, 1}}, {"nonempty_bitstring", {1, 1}}};
    const auto found = node.kind == Kind::application ? NAMED.find(node.name) : NAMED.end();
    return found == NAMED.end() ? Bits{0, unit} : found->second;
}

// Set `key` to `value` in map fields (keys and values alternate): a key given twice keeps its last value.
void put(std::vector<Id> &fields, const Id key, const Id value) {
    for (std::size_t index = 0; index < fields.size(); index += 2) {
        if (fields[index] == key) {
            fields[index + 1] = value;
            return;
        }
    }
    fields.insert(fields.end(), {key, value});
}

// Builds the facts of literals and constructed values from their operands' recorded facts.
class Construct final {
  public:
    Construct(Inference &inference, const ast::Module &syntax) : inference_(inference), syntax_(syntax) {}

    template <typename T> std::optional<Id> operator()(const T &) { return std::nullopt; }

    std::optional<Id> operator()(const ast::IntegerLiteral &value) { return lattice_.integer(value.value.decimal); }

    std::optional<Id> operator()(const ast::CharacterLiteral &value) {
        return lattice_.integer(std::to_string(static_cast<std::uint32_t>(value.value)));
    }

    std::optional<Id> operator()(const ast::FloatLiteral &) { return lattice_.category("float"); }

    std::optional<Id> operator()(const ast::Atom &value) { return lattice_.atom(utf8(value.name)); }

    std::optional<Id> operator()(const ast::StringLiteral &value) { return string(value.value); }

    std::optional<Id> operator()(const ast::List &value) {
        return value.elements.empty() && !value.tail ? std::optional{lattice_.nil()} : std::nullopt;
    }

    std::optional<Id> operator()(const ast::UnaryExpression &value);
    std::optional<Id> operator()(const ast::Tuple &value);
    std::optional<Id> operator()(const ast::MapExpression &value);
    std::optional<Id> operator()(const ast::Bitstring &value);

  private:
    // The recorded fact of an operand.
    Id fact(const ast::ExprId &id) const { return inference_.expressions.at(&syntax_.expression(id)).type; }

    // Whether an operand never produces a value: then neither does the expression.
    bool never(const ast::ExprId &id) const { return fact(id) == inference_.graph.bottom(); }

    // A nonempty list of the characters of a string literal; [] for "".
    Id string(const std::u32string &text);
    // The sizes of one segment of a bitstring construction; none for sizes too large to count.
    std::optional<Bits> segment(const ast::BinarySegment &segment);
    // The sizes of a sized integer, float or binary segment: its size times its unit.
    std::optional<Bits> sized(const ast::BinarySegment &segment, const BinaryOptions &options);
    // The sizes of a string literal segment: each character as one segment of the segment's type.
    std::optional<Bits> characters(const std::u32string &text, const ast::BinarySegment &segment,
                                   const BinaryOptions &options);

    Inference &inference_;
    const ast::Module &syntax_;
    Lattice lattice_{inference_.graph};
};

std::optional<Id> Construct::operator()(const ast::UnaryExpression &value) {
    if (value.operation != ast::UnaryOperator::negative && value.operation != ast::UnaryOperator::positive) {
        return std::nullopt;
    }
    const auto operand = fact(value.operand);
    const auto &node = inference_.graph.get(operand);
    if (node.kind == Kind::integer) {
        const auto &digits = node.name;
        const bool negate = value.operation == ast::UnaryOperator::negative && digits != "0";
        return negate ? lattice_.integer(digits.starts_with('-') ? digits.substr(1) : '-' + digits) : operand;
    }
    return operand == lattice_.category("float") ? std::optional{operand} : std::nullopt;
}

std::optional<Id> Construct::operator()(const ast::Tuple &value) {
    std::vector<Id> elements;
    elements.reserve(value.elements.size());
    for (const auto &element : value.elements) {
        if (never(element)) {
            return inference_.graph.bottom();
        }
        elements.push_back(fact(element));
    }
    return lattice_.tuple(std::move(elements));
}

std::optional<Id> Construct::operator()(const ast::MapExpression &value) {
    if (value.base) {
        return std::nullopt;
    }
    std::vector<Id> fields;
    for (const auto &field : value.fields) {
        if (never(field.key) || never(field.value)) {
            return inference_.graph.bottom();
        }
        const auto key = fact(field.key);
        if (!singular(inference_.graph, key)) {
            return lattice_.category("map");
        }
        put(fields, key, fact(field.value));
    }
    return lattice_.map(std::move(fields));
}

std::optional<Id> Construct::operator()(const ast::Bitstring &value) {
    std::optional<Bits> total = Bits{};
    for (const auto &item : value.segments) {
        if (never(item.value) || (item.size && never(*item.size))) {
            return inference_.graph.bottom();
        }
        total = append(total, segment(item));
    }
    return total ? lattice_.bitstring(total->base, total->unit) : lattice_.category("bitstring");
}

Id Construct::string(const std::u32string &text) {
    if (text.empty()) {
        return lattice_.nil();
    }
    const std::set<char32_t> codes(text.begin(), text.end());
    if (codes.size() > lattice_.limits().singletons) {
        return lattice_.list(lattice_.range({std::to_string(static_cast<std::uint32_t>(*codes.begin())),
                                             std::to_string(static_cast<std::uint32_t>(*codes.rbegin()))}),
                             true);
    }
    std::vector<Id> elements;
    elements.reserve(codes.size());
    for (const auto code : codes) {
        elements.push_back(lattice_.integer(std::to_string(static_cast<std::uint32_t>(code))));
    }
    return lattice_.list(lattice_.join(elements, 0), true);
}

std::optional<Bits> Construct::segment(const ast::BinarySegment &segment) {
    const auto options = binary_options(segment);
    const auto &value = syntax_.expression(ungroup(syntax_, segment.value)).value;
    if (const auto *text = std::get_if<ast::StringLiteral>(&value)) {
        return characters(text->value, segment, options);
    }
    if (options.type == BitType::utf8 || options.type == BitType::utf16 || options.type == BitType::utf32) {
        return utf_bits(options.type);
    }
    return options.all ? value_bits(inference_.graph, fact(segment.value), options.unit) : sized(segment, options);
}

std::optional<Bits> Construct::sized(const ast::BinarySegment &segment, const BinaryOptions &options) {
    if (!segment.size) {
        return Bits{std::uint64_t{options.size} * options.unit, 0};
    }
    const auto &size = inference_.graph.get(fact(*segment.size));
    if (size.kind != Kind::integer) {
        return Bits{0, options.unit};
    }
    const auto count = size_value(size.name);
    return count ? repeat(Bits{options.unit, 0}, *count) : std::nullopt;
}

std::optional<Bits> Construct::characters(const std::u32string &text, const ast::BinarySegment &segment,
                                          const BinaryOptions &options) {
    if (options.type == BitType::binary) {
        // A string as a binary segment is its characters as bytes.
        return segment.size ? std::nullopt : std::optional{Bits{8 * text.size(), 0}};
    }
    if (options.type == BitType::integer || options.type == BitType::floating) {
        return repeat(sized(segment, options), text.size());
    }
    std::optional<Bits> total = Bits{};
    for (const auto code : text) {
        total = append(total, Bits{encoded_bits(code, options.type), 0});
    }
    return total;
}
} // namespace

std::optional<Id> constructed_fact(Inference &inference, const ast::Module &syntax, const ast::ExprValue &value) {
    Construct construct(inference, syntax);
    return std::visit(construct, value);
}

bool singular(const Graph &graph, const Id fact) {
    const auto &node = graph.get(fact);
    switch (node.kind) {
    case Kind::integer:
    case Kind::atom:
        return true;
    case Kind::list:
        return node.children.empty();
    case Kind::tuple:
    case Kind::map:
        return node.name == "exact" &&
               std::ranges::all_of(node.children, [&](const Id child) { return singular(graph, child); });
    default:
        return false;
    }
}
} // namespace clause::semantic::types
