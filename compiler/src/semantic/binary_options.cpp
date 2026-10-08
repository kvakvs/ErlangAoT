#include "binary_options.hpp"
#include <map>

namespace clause::semantic {
namespace {
// Canonical type aliases retain their distinct default unit without repeating legality validation.
void modifier(BinaryOptions &options, const ast::BinaryModifier &modifier) {
    using Type = abi::v1::BitType;
    static const std::map<std::u32string, Type> types{
        {U"integer", Type::integer}, {U"float", Type::floating},   {U"binary", Type::binary},
        {U"bytes", Type::binary},    {U"bitstring", Type::binary}, {U"bits", Type::binary},
        {U"utf8", Type::utf8},       {U"utf16", Type::utf16},      {U"utf32", Type::utf32}};
    const auto found = types.find(modifier.name.name);
    if (found != types.end()) {
        options.type = found->second;
    }
    if (modifier.name.name == U"signed") {
        options.signed_value = true;
    }
    if (modifier.name.name == U"little") {
        options.little = true;
    }
    if (modifier.name.name == U"native") {
        options.native = true;
    }
}

// Explicit units override type defaults, while bitstring aliases imply a one-bit unit.
void unit(BinaryOptions &options, const ast::BinaryModifier &modifier) {
    if (modifier.name.name == U"bits" || modifier.name.name == U"bitstring") {
        options.unit = 1;
    }
    if (modifier.name.name == U"unit" && modifier.parameter) {
        options.unit = static_cast<unsigned>(std::stoul(modifier.parameter->decimal));
    }
}
} // namespace

BinaryOptions binary_options(const ast::BinarySegment &segment) {
    BinaryOptions result;
    if (segment.modifiers) {
        for (const auto &item : *segment.modifiers) {
            modifier(result, item);
        }
    }
    using Type = abi::v1::BitType;
    result.unit = result.type == Type::binary ? 8 : 1;
    result.size = result.type == Type::floating ? 64 : 8;
    result.all = result.type == Type::binary && !segment.size;
    if (segment.modifiers) {
        for (const auto &item : *segment.modifiers) {
            unit(result, item);
        }
    }
    return result;
}
} // namespace clause::semantic
