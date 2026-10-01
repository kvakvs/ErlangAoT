#include "capabilities.hpp"
#include "pattern_state.hpp"
#include <map>

namespace erlang_aot::semantic {
namespace {
struct Types {
    // Canonical categories detect contradictory aliases and preserve omitted unit/type distinctions.
    std::map<std::string, std::u32string> categories;
    bool unit = false;
};

// Aliases carry the same type but imply different default units in the later binary match plan.
std::pair<std::string, std::u32string> category(const std::u32string &name) {
    static const std::map<std::u32string, std::pair<std::string, std::u32string>> names{
        {U"integer", {"type", U"integer"}}, {U"float", {"type", U"float"}},       {U"binary", {"type", U"binary"}},
        {U"bytes", {"type", U"binary"}},    {U"bitstring", {"type", U"binary"}},  {U"bits", {"type", U"binary"}},
        {U"utf8", {"type", U"utf8"}},       {U"utf16", {"type", U"utf16"}},       {U"utf32", {"type", U"utf32"}},
        {U"signed", {"sign", U"signed"}},   {U"unsigned", {"sign", U"unsigned"}}, {U"big", {"endian", U"big"}},
        {U"little", {"endian", U"little"}}, {U"native", {"endian", U"native"}}};
    const auto found = names.find(name);
    return found == names.end() ? std::pair<std::string, std::u32string>{} : found->second;
}

// Units are bounded before decimal conversion, so arbitrary literal widths cannot overflow the host.
bool unit(const ast::BinaryModifier &modifier) {
    if (!modifier.parameter || modifier.parameter->decimal.size() > 3) {
        return false;
    }
    const auto number = arity(*modifier.parameter);
    return number && *number >= 1 && *number <= 256;
}

// Merge explicit and alias-implied fields with identical duplicate semantics.
void merge(BindingAnalysis &state, const ast::ExprId &id, Types &types, const std::string &key,
           const std::u32string &name) {
    const auto [entry, inserted] = types.categories.emplace(key, name);
    if (!inserted && entry->second != name) {
        pattern_error(state, id, "conflicting binary pattern modifiers");
    }
}

// OTP permits repeated identical modifiers, but rejects conflicts, unknown names and malformed units.
void modifier(BindingAnalysis &state, const ast::ExprId &id, const ast::BinaryModifier &value, Types &types) {
    auto [key, name] = category(value.name.name);
    if (value.name.name == U"unit" && value.parameter && unit(value)) {
        key = "unit";
        const auto &digits = value.parameter->decimal;
        name.assign(digits.begin(), digits.end());
        types.unit = true;
    } else if (key.empty() || value.parameter) {
        pattern_error(state, id, "invalid binary pattern modifier");
        return;
    }
    merge(state, id, types, key, name);
    if (value.name.name == U"bytes") {
        merge(state, id, types, "unit", U"8");
    }
    if (value.name.name == U"bits" || value.name.name == U"bitstring") {
        merge(state, id, types, "unit", U"1");
    }
}

// Parse a segment's modifier list under the shared work budget.
void segment_types(BindingAnalysis &state, const ast::BinarySegment &value, Types &types) {
    if (value.modifiers) {
        for (const auto &item : *value.modifiers) {
            if (!state.spend(value.value)) {
                return;
            }
            modifier(state, value.value, item, types);
        }
    }
}

// String literals have their own size/type rule, independently of general segment typing.
bool literal_string(BindingAnalysis &state, const ast::BinarySegment &value, const bool utf) {
    const bool string = std::holds_alternative<ast::StringLiteral>(
        state.module.syntax->expression(ungroup(*state.module.syntax, value.value)).value);
    if (string && (value.size || (value.modifiers && !utf))) {
        pattern_error(state, value.value, "literal string pattern cannot have this size or type");
    }
    return string;
}

// Default integer/float sizes forbid explicit units; UTF segments forbid both size and unit.
bool size_type(BindingAnalysis &state, const ast::BinarySegment &value, Types &types) {
    const auto &type = types.categories["type"];
    const bool utf = type.starts_with(U"utf");
    if (utf && (value.size || types.unit)) {
        pattern_error(state, value.value, "UTF binary pattern segment cannot have size or unit");
    }
    if (!value.size && types.unit && !utf && type != U"binary") {
        pattern_error(state, value.value, "binary pattern unit requires an explicit size");
    }
    return utf;
}

// Size/type constraints are semantic even while binary extraction and runtime construction remain deferred.
void segment(BindingAnalysis &state, const ast::BinarySegment &value, bool last, bool pattern) {
    Types types;
    segment_types(state, value, types);
    const bool utf = size_type(state, value, types);
    if (!pattern) {
        return;
    }
    const bool string = literal_string(state, value, utf);
    if (types.categories["type"] == U"binary" && !value.size && !last && !string) {
        pattern_error(state, value.value, "unsized binary pattern segment must be last");
    }
}
} // namespace

void pattern_binary(BindingAnalysis &state, const ast::ExprId &id, const ast::Bitstring &binary, const bool pattern) {
    if (!state.spend(id, binary.segments.size())) {
        return;
    }
    for (std::size_t i = 0; i < binary.segments.size(); ++i) {
        segment(state, binary.segments[i], i + 1 == binary.segments.size(), pattern);
    }
}
} // namespace erlang_aot::semantic
