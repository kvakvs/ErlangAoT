#include "inference_values.hpp"
#include "../binary_options.hpp"
#include "../capabilities.hpp"
#include "../records.hpp"
#include "inference_containers.hpp"
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
    Construct(Inference &inference, const FunctionRef function)
        : inference_(inference), module_(*function.module), syntax_(*function.module->syntax) {}

    template <typename T> std::optional<Id> operator()(const T &) { return std::nullopt; }

    std::optional<Id> operator()(const ast::IntegerLiteral &value) { return lattice_.integer(value.value.decimal); }

    std::optional<Id> operator()(const ast::CharacterLiteral &value) {
        return lattice_.integer(std::to_string(static_cast<std::uint32_t>(value.value)));
    }

    std::optional<Id> operator()(const ast::FloatLiteral &) { return lattice_.category("float"); }

    std::optional<Id> operator()(const ast::Atom &value) { return lattice_.atom(utf8(value.name)); }

    std::optional<Id> operator()(const ast::StringLiteral &value) { return string(value.value); }

    std::optional<Id> operator()(const ast::MapComprehension &) { return lattice_.category("map"); }

    std::optional<Id> operator()(const ast::RecordIndex &value);
    std::optional<Id> operator()(const ast::List &value);
    std::optional<Id> operator()(const ast::Tuple &value);
    std::optional<Id> operator()(const ast::MapExpression &value);
    std::optional<Id> operator()(const ast::Bitstring &value);
    std::optional<Id> operator()(const ast::RecordExpression &value);
    std::optional<Id> operator()(const ast::RecordAccess &value);
    std::optional<Id> operator()(const ast::ListComprehension &value);
    std::optional<Id> operator()(const ast::BinaryComprehension &value);

  private:
    // The recorded fact of an operand; term() for one that is not evaluated.
    Id fact(const ast::ExprId &id) const {
        const auto found = inference_.expressions.find(&syntax_.expression(id));
        return found == inference_.expressions.end() ? inference_.graph.top() : found->second.type;
    }

    // Whether any operand never produces a value.
    bool never(const std::vector<ast::ExprId> &operands) const {
        return std::ranges::any_of(operands, [&](const ast::ExprId &operand) { return never(operand); });
    }

    // Whether an operand never produces a value: then neither does the expression.
    bool never(const ast::ExprId &id) const { return fact(id) == inference_.graph.bottom(); }

    // A map update: each field of each map the base can be.
    Id update(const ast::MapExpression &value);
    // A tuple record update: the base's matching tuples with the given fields set.
    Id record_update(const RecordLayout &layout, const ast::RecordExpression &value);
    // A tuple record's layout, or null for a native or unknown record.
    const RecordLayout *tuple_record(const ast::RecordIdentity &identity) const;
    // The members of a fact that can be tuple record `layout`: its tuples of the record's size and tag, and the
    // record with unknown fields for an unknown value.
    Id record_members(const RecordLayout &layout, Id fact);
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
    const Module &module_;
    const ast::Module &syntax_;
    Lattice lattice_{inference_.graph};
};

std::optional<Id> Construct::operator()(const ast::List &value) {
    if (value.elements.empty() && !value.tail) {
        return lattice_.nil();
    }
    if (never(value.elements) || (value.tail && never(*value.tail))) {
        return inference_.graph.bottom();
    }
    // Two or more known elements keep their positions in front of the tail; otherwise the joined elements are one
    // cell in front of it.
    std::vector<Id> elements;
    elements.reserve(value.elements.size() + 1);
    for (const auto &element : value.elements) {
        elements.push_back(fact(element));
    }
    const auto tail = value.tail ? fact(*value.tail) : lattice_.nil();
    if (elements.size() >= 2 && !std::ranges::contains(elements, inference_.graph.top())) {
        elements.push_back(tail);
        return lattice_.positional(std::move(elements));
    }
    return cons(lattice_, {lattice_.join(elements, 0), tail});
}

std::optional<Id> Construct::operator()(const ast::RecordIndex &value) {
    const auto *layout = record_layout(module_, value.record, value.name_source);
    const auto field = layout && !layout->native ? record_field(*layout, value.field) : std::nullopt;
    return field ? std::optional{lattice_.integer(std::to_string(*field + 2))} : std::nullopt;
}

const RecordLayout *Construct::tuple_record(const ast::RecordIdentity &identity) const {
    const auto *layout = record_layout(module_, identity);
    return layout && !layout->native ? layout : nullptr;
}

Id Construct::record_members(const RecordLayout &layout, const Id fact) {
    const auto size = layout.fields.size() + 1;
    const auto tag = lattice_.atom(utf8(layout.name.name));
    // A value that is not the record fails the access or update, so only matching tuples stay.
    std::vector<Id> results;
    for (const auto member : lattice_.members(fact)) {
        const auto first = tuple_element(lattice_, member, 1, size);
        if (first == inference_.graph.top()) {
            std::vector<Id> generic(size, inference_.graph.top());
            generic.front() = tag;
            results.push_back(lattice_.tuple(std::move(generic)));
        } else if (lattice_.holds_atom(first, utf8(layout.name.name))) {
            results.push_back(set_element(lattice_, {lattice_.integer("1"), member}, tag));
        }
    }
    return lattice_.join(results, 0);
}

Id Construct::record_update(const RecordLayout &layout, const ast::RecordExpression &value) {
    auto record = record_members(layout, fact(*value.base));
    for (const auto &field : value.fields) {
        const auto *name = std::get_if<ast::Atom>(&field.name);
        const auto position = name ? record_field(layout, *name) : std::nullopt;
        if (position) {
            const auto index = lattice_.integer(std::to_string(*position + 2));
            record = set_element(lattice_, {index, record}, fact(field.value));
        }
    }
    return record;
}

std::optional<Id> Construct::operator()(const ast::RecordExpression &value) {
    const auto *layout = tuple_record(value.identity);
    if (!layout) {
        return std::nullopt;
    }
    if (value.base) {
        return record_update(*layout, value);
    }
    std::vector<Id> elements{lattice_.atom(utf8(layout->name.name))};
    for (const auto &field : record_values(module_, value, false)) {
        // A field without a value or default is undefined.
        elements.push_back(field ? fact(*field) : lattice_.atom("undefined"));
    }
    return std::ranges::contains(elements, inference_.graph.bottom()) ? inference_.graph.bottom()
                                                                      : lattice_.tuple(std::move(elements));
}

std::optional<Id> Construct::operator()(const ast::RecordAccess &value) {
    const auto *layout = tuple_record(value.identity);
    const auto position = layout ? record_field(*layout, value.field) : std::nullopt;
    if (!position) {
        return std::nullopt;
    }
    return tuple_element(lattice_, record_members(*layout, fact(value.base)), *position + 2, layout->fields.size() + 1);
}

std::optional<Id> Construct::operator()(const ast::ListComprehension &value) {
    std::vector<Id> templates;
    templates.reserve(value.templates.size());
    for (const auto &item : value.templates) {
        templates.push_back(fact(item));
    }
    return lattice_.list(lattice_.join(templates, 0), false);
}

std::optional<Id> Construct::operator()(const ast::BinaryComprehension &value) {
    // Any number of copies of the template: every copy's sizes share its base and unit.
    const auto bits = value_bits(inference_.graph, fact(value.expression), 1);
    return lattice_.bitstring(0, std::gcd(bits.base, bits.unit));
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
        return update(value);
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

Id Construct::update(const ast::MapExpression &value) {
    std::vector<Id> fields;
    std::vector<bool> exact;
    for (const auto &field : value.fields) {
        fields.insert(fields.end(), {fact(field.key), fact(field.value)});
        exact.push_back(field.kind == ast::MapFieldKind::exact);
    }
    if (never(*value.base) || std::ranges::contains(fields, inference_.graph.bottom())) {
        return inference_.graph.bottom();
    }
    return map_update(lattice_, fact(*value.base), fields, exact);
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

std::optional<Id> constructed_fact(Inference &inference, const FunctionRef function, const ast::ExprValue &value) {
    Construct construct(inference, function);
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
    case Kind::positional:
        return std::ranges::all_of(node.children, [&](const Id child) { return singular(graph, child); });
    case Kind::tuple:
    case Kind::map:
        return node.name == "exact" &&
               std::ranges::all_of(node.children, [&](const Id child) { return singular(graph, child); });
    default:
        return false;
    }
}
} // namespace clause::semantic::types
